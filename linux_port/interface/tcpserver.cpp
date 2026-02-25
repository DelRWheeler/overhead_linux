//////////////////////////////////////////////////////////////////////
// tcpserver.cpp: Linux port of TCP server
//
// Receives messages from host/API via TCP sockets.
// Uses epoll instead of WSAEventSelect for event-driven I/O.
// All message formats and routing logic unchanged from RTX version.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "tcpconfig.h"
#include <sys/epoll.h>

#undef  _FILE_
#define _FILE_              "tcpserver.cpp"

// Macro to skip sending msg to intersystems if not enabled
#define ISYS_ENABLE pShm->sys_set.IsysLineSettings.isys_enable

// External function declarations
int PostAppRxMessage(PCHAR Message, int bytes);
int PostIsysRxMessage(PCHAR Message, int bytes);
void DumpData(BYTE *pbuf, int len);

// TCP client connection structure (Linux version)
typedef struct {
    SOCKET      sock;
    BOOL        connected;
    CHAR        recvBuf[TCP_RECV_BUFFER_SIZE];
    int         recvBufLen;
    SOCKADDR_IN clientAddr;
} TCP_CLIENT_CONN;

// Global server state
static SOCKET           g_listenSock = INVALID_SOCKET;
static TCP_CLIENT_CONN  g_clients[TCP_MAX_CLIENTS];
static int              g_epollFd = -1;
static int              g_numClients = 0;

//--------------------------------------------------------
// InitTcpClient
//--------------------------------------------------------
static void InitTcpClient(int idx)
{
    g_clients[idx].sock = INVALID_SOCKET;
    g_clients[idx].connected = FALSE;
    g_clients[idx].recvBufLen = 0;
    memset(&g_clients[idx].clientAddr, 0, sizeof(SOCKADDR_IN));
    memset(g_clients[idx].recvBuf, 0, TCP_RECV_BUFFER_SIZE);
}

//--------------------------------------------------------
// DisconnectTcpClient
//--------------------------------------------------------
static void DisconnectTcpClient(int idx)
{
    if (g_clients[idx].sock != INVALID_SOCKET)
    {
        if (g_epollFd >= 0) {
            epoll_ctl(g_epollFd, EPOLL_CTL_DEL, g_clients[idx].sock, NULL);
        }
        shutdown(g_clients[idx].sock, SHUT_RDWR);
        close(g_clients[idx].sock);
    }

    InitTcpClient(idx);
    RtPrintf("TCP client %d disconnected\n", idx);
}

//--------------------------------------------------------
// AcceptTcpClient
//--------------------------------------------------------
static int AcceptTcpClient(SOCKET listenSock)
{
    SOCKET clientSock;
    SOCKADDR_IN clientAddr;
    socklen_t addrLen = sizeof(SOCKADDR_IN);
    int idx;
    int optVal;

    clientSock = accept(listenSock, (SOCKADDR*)&clientAddr, &addrLen);

    if (clientSock == INVALID_SOCKET)
    {
        RtPrintf("Error: accept failed %d, file %s, line %d\n",
                 errno, _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Find an empty slot
    for (idx = 0; idx < TCP_MAX_CLIENTS; idx++)
    {
        if (!g_clients[idx].connected)
            break;
    }

    if (idx >= TCP_MAX_CLIENTS)
    {
        RtPrintf("Warning: Max clients reached, rejecting connection\n");
        close(clientSock);
        return ERROR_OCCURED;
    }

    // Set socket options
    optVal = TCP_NODELAY_ENABLE;
    setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, (char*)&optVal, sizeof(optVal));

    optVal = TCP_KEEPALIVE_ENABLE;
    setsockopt(clientSock, SOL_SOCKET, SO_KEEPALIVE, (char*)&optVal, sizeof(optVal));

    // Set non-blocking mode
    int flags = fcntl(clientSock, F_GETFL, 0);
    fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);

    // Add to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
    ev.data.fd = clientSock;
    if (epoll_ctl(g_epollFd, EPOLL_CTL_ADD, clientSock, &ev) < 0)
    {
        RtPrintf("Error: epoll_ctl failed, file %s, line %d\n", _FILE_, __LINE__);
        close(clientSock);
        return ERROR_OCCURED;
    }

    // Store client info
    g_clients[idx].sock = clientSock;
    g_clients[idx].connected = TRUE;
    g_clients[idx].recvBufLen = 0;
    memcpy(&g_clients[idx].clientAddr, &clientAddr, sizeof(SOCKADDR_IN));

    g_numClients++;

    RtPrintf("TCP client %d connected from %s:%d\n",
             idx, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    return NO_ERRORS;
}

//--------------------------------------------------------
// ProcessTcpMessage - Process a complete message (unchanged)
//--------------------------------------------------------
static int ProcessTcpMessage(PCHAR message, int len)
{
    mhdr* phdr = (mhdr*)message;

    pShm->IsysLineStatus.pkts_recvd++;

    // Check for interface threads ok
    if (RtWaitForSingleObject(hInterfaceThreadsOk, INFINITE) == WAIT_FAILED)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Route message based on command type (unchanged from RTX)
    if ((phdr->cmd > ISYS_MSG_START) &&
        (phdr->cmd < LAST_ISYS_MSG) &&
        ISYS_ENABLE)
    {
        PostIsysRxMessage(message, len);
    }
    else if ((phdr->cmd > APP_MSG_START) && (phdr->cmd < LAST_APP_MSG))
    {
        if (phdr->cmd == SET_DATE_TIME)
        {
            app_gen_rw_msg *p_dt_msg = (app_gen_rw_msg*)message;
            SYSTEMTIME *dt = (SYSTEMTIME*)&p_dt_msg->data;
            SetTime(dt);
        }
        else if (phdr->cmd == RESTART_APP)
        {
            Shutdown(RESTART_APP);
        }
        else if (phdr->cmd == RESTART_NT)
        {
            Shutdown(RESTART_NT);
        }
        else if (phdr->cmd == SHUTDOWN_NT)
        {
            Shutdown(SHUTDOWN_NT);
        }
        else
        {
            PostAppRxMessage(message, len);
        }
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// ReceiveTcpData - Receive and frame messages (unchanged logic)
//--------------------------------------------------------
static int ReceiveTcpData(int idx)
{
    int bytesRecv;
    int msgLen;
    int processed;

    bytesRecv = recv(g_clients[idx].sock,
                     g_clients[idx].recvBuf + g_clients[idx].recvBufLen,
                     TCP_RECV_BUFFER_SIZE - g_clients[idx].recvBufLen,
                     0);

    if (bytesRecv < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return NO_ERRORS;

        RtPrintf("Error: recv failed %d, file %s, line %d\n", errno, _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if (bytesRecv == 0)
    {
        return ERROR_OCCURED;  // Connection closed
    }

    g_clients[idx].recvBufLen += bytesRecv;

    // Process complete messages using mhdr framing (unchanged)
    processed = 0;

    while (g_clients[idx].recvBufLen - processed >= (int)sizeof(mhdr))
    {
        mhdr* phdr = (mhdr*)(g_clients[idx].recvBuf + processed);

        msgLen = sizeof(mhdr) + phdr->len;

        if (msgLen < (int)sizeof(mhdr) || msgLen > MAX_APP_MSG_SIZE)
        {
            RtPrintf("Warning: Invalid message length %d, resetting buffer\n", msgLen);
            g_clients[idx].recvBufLen = 0;
            return NO_ERRORS;
        }

        if (g_clients[idx].recvBufLen - processed < msgLen)
            break;

        ProcessTcpMessage(g_clients[idx].recvBuf + processed, msgLen);

        processed += msgLen;
    }

    // Move remaining partial data to start of buffer
    if (processed > 0)
    {
        if (g_clients[idx].recvBufLen > processed)
        {
            memmove(g_clients[idx].recvBuf,
                    g_clients[idx].recvBuf + processed,
                    g_clients[idx].recvBufLen - processed);
        }
        g_clients[idx].recvBufLen -= processed;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// FindClientBySocket
//--------------------------------------------------------
static int FindClientBySocket(SOCKET sock)
{
    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
    {
        if (g_clients[i].connected && g_clients[i].sock == sock)
            return i;
    }
    return -1;
}

//--------------------------------------------------------
// TcpServer - Main TCP server thread (Linux epoll version)
//--------------------------------------------------------
int RTFCNDCL TcpServer(PVOID unused)
{
    SOCKADDR_IN serverAddr;
    int i;
    int optVal;
    struct epoll_event events[TCP_MAX_CLIENTS + 1];
    int nfds;

    (void)unused;

    RtPrintf("TCP Server starting on port %d...\n", TCP_SERVER_PORT);

    // Initialize client structures
    for (i = 0; i < TCP_MAX_CLIENTS; i++)
    {
        InitTcpClient(i);
    }

    // Create epoll instance
    g_epollFd = epoll_create1(0);
    if (g_epollFd < 0)
    {
        RtPrintf("Error: epoll_create failed, file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Create listen socket
    g_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (g_listenSock == INVALID_SOCKET)
    {
        RtPrintf("Error: socket failed %d, file %s, line %d\n",
                 errno, _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Set socket options - REUSEADDR + REUSEPORT to handle orphaned sockets after crash
    optVal = 1;
    setsockopt(g_listenSock, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, sizeof(optVal));
    setsockopt(g_listenSock, SOL_SOCKET, SO_REUSEPORT, (char*)&optVal, sizeof(optVal));

    // Set non-blocking
    int flags = fcntl(g_listenSock, F_GETFL, 0);
    fcntl(g_listenSock, F_SETFL, flags | O_NONBLOCK);

    // Bind
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(TCP_SERVER_PORT);

    // Retry bind in case port is still held from a previous SIGKILL'd process
    int bind_retries = 0;
    while (bind(g_listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        bind_retries++;
        if (bind_retries > 30)  // Give up after ~30 seconds
        {
            RtPrintf("Error: bind failed after %d retries, errno=%d, file %s, line %d\n",
                     bind_retries, errno, _FILE_, __LINE__);
            close(g_listenSock);
            return ERROR_OCCURED;
        }
        RtPrintf("Warning: bind failed (errno=%d), retry %d/30...\n", errno, bind_retries);
        Sleep(1000);
    }

    // Listen
    if (listen(g_listenSock, TCP_SERVER_BACKLOG) < 0)
    {
        RtPrintf("Error: listen failed %d, file %s, line %d\n",
                 errno, _FILE_, __LINE__);
        close(g_listenSock);
        return ERROR_OCCURED;
    }

    // Add listen socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = g_listenSock;
    epoll_ctl(g_epollFd, EPOLL_CTL_ADD, g_listenSock, &ev);

    // Show server ready
    pShm->IsysLineStatus.app_threads |= W32_PIPE_SVR_RDY;
    RtPrintf("TCP Server listening on port %d\n", TCP_SERVER_PORT);

    // Main server loop
    for (;;)
    {
        nfds = epoll_wait(g_epollFd, events, TCP_MAX_CLIENTS + 1, 1000);

        if (nfds < 0)
        {
            if (errno == EINTR) continue;
            RtPrintf("Error: epoll_wait failed %d\n", errno);
            continue;
        }

        for (int n = 0; n < nfds; n++)
        {
            if (events[n].data.fd == g_listenSock)
            {
                // New connection
                AcceptTcpClient(g_listenSock);
            }
            else
            {
                int clientIdx = FindClientBySocket(events[n].data.fd);
                if (clientIdx < 0) continue;

                if (events[n].events & (EPOLLHUP | EPOLLERR))
                {
                    DisconnectTcpClient(clientIdx);
                    g_numClients--;
                }
                else if (events[n].events & EPOLLIN)
                {
                    if (ReceiveTcpData(clientIdx) == ERROR_OCCURED)
                    {
                        DisconnectTcpClient(clientIdx);
                        g_numClients--;
                    }
                }
            }
        }
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// CleanupTcpServer
//--------------------------------------------------------
void CleanupTcpServer()
{
    int i;

    for (i = 0; i < TCP_MAX_CLIENTS; i++)
    {
        if (g_clients[i].connected)
        {
            DisconnectTcpClient(i);
        }
    }

    if (g_listenSock != INVALID_SOCKET)
    {
        close(g_listenSock);
        g_listenSock = INVALID_SOCKET;
    }

    if (g_epollFd >= 0)
    {
        close(g_epollFd);
        g_epollFd = -1;
    }
}
