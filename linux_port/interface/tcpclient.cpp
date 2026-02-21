//////////////////////////////////////////////////////////////////////
// tcpclient.cpp: Linux port of TCP client
//
// Sends messages to host/API and remote controllers via TCP.
// Uses POSIX sockets. All message formats unchanged from RTX version.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "tcpconfig.h"

#undef  _FILE_
#define _FILE_      "tcpclient.cpp"

// External declarations
extern void DumpData(BYTE *pbuf, int len);

// Global variables from client.cpp (keep same interface)
extern DWORD   dwMaximumSizeHigh;
extern LONG    lInitialCount;
extern LONG    lMaximumCount;
extern LONG    lReleaseCount;

// TCP client sockets
static SOCKET g_tcpSock[INSTANCES];
static BOOL   g_tcpConnected[INSTANCES];

// IP address configuration
static char   g_tcpHost[INSTANCES][32];
static USHORT g_tcpPort[INSTANCES];

// Connection state tracking for clean status messages
static BOOL   g_prevHostConnected = FALSE;
static int    g_reconnectAttempts[INSTANCES];
#define RECONNECT_LOG_INTERVAL  30  // Only log reconnect attempts every N tries


//--------------------------------------------------------
// InitTcpClientSockets
//--------------------------------------------------------
void InitTcpClientSockets()
{
    for (int i = 0; i < INSTANCES; i++)
    {
        g_tcpSock[i] = INVALID_SOCKET;
        g_tcpConnected[i] = FALSE;
        memset(g_tcpHost[i], 0, sizeof(g_tcpHost[i]));
        g_tcpPort[i] = TCP_CLIENT_BASE_PORT;
        g_reconnectAttempts[i] = 0;
    }
    g_prevHostConnected = FALSE;
}

//--------------------------------------------------------
// ParseHostAddress (unchanged logic)
//--------------------------------------------------------
static void ParseHostAddress(const char* input, char* ipOut, USHORT* portOut)
{
    char temp[64];
    char* colonPos;

    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = 0;

    colonPos = strchr(temp, ':');

    if (colonPos != NULL)
    {
        *colonPos = 0;
        strncpy(ipOut, temp, 15);
        ipOut[15] = 0;
        *portOut = (USHORT)atoi(colonPos + 1);
    }
    else
    {
        strncpy(ipOut, temp, 15);
        ipOut[15] = 0;
        *portOut = TCP_CLIENT_BASE_PORT;
    }
}

//--------------------------------------------------------
// ConnectToTcpHost - Connect via TCP (Linux POSIX sockets)
//--------------------------------------------------------
int ConnectToTcpHost(int idx)
{
    SOCKET sock;
    SOCKADDR_IN serverAddr;
    int optVal;
    char ipAddr[16];
    USHORT port;
    fd_set writeSet;
    struct timeval timeout;
    int result;

    ParseHostAddress(pShm->sys_set.IsysLineSettings.pipe_path[idx], ipAddr, &port);

    strncpy(g_tcpHost[idx], ipAddr, sizeof(g_tcpHost[idx]) - 1);
    g_tcpPort[idx] = port;

    // Only log connect attempts on first try or periodically to reduce spam
    if (g_reconnectAttempts[idx] == 0 || g_reconnectAttempts[idx] % RECONNECT_LOG_INTERVAL == 0)
        RtPrintf("Connecting to TCP host %s:%d (idx %d, attempt %d)\n",
                 ipAddr, port, idx, g_reconnectAttempts[idx] + 1);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET)
    {
        if (g_reconnectAttempts[idx] == 0)
            RtPrintf("Error: socket failed %d, file %s, line %d\n",
                     errno, _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Set socket options
    optVal = TCP_NODELAY_ENABLE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&optVal, sizeof(optVal));

    optVal = TCP_KEEPALIVE_ENABLE;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&optVal, sizeof(optVal));

    // Set non-blocking for connect with timeout
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Setup server address
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ipAddr);
    serverAddr.sin_port = htons(port);

    if (serverAddr.sin_addr.s_addr == INADDR_NONE)
    {
        if (g_reconnectAttempts[idx] == 0)
            RtPrintf("Error: Invalid IP address %s, file %s, line %d\n",
                     ipAddr, _FILE_, __LINE__);
        close(sock);
        return ERROR_OCCURED;
    }

    // Attempt connection
    result = connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    if (result < 0)
    {
        if (errno != EINPROGRESS)
        {
            if (g_reconnectAttempts[idx] == 0)
                RtPrintf("Error: connect failed %d, file %s, line %d\n",
                         errno, _FILE_, __LINE__);
            close(sock);
            return ERROR_OCCURED;
        }

        // Wait for connection with timeout
        FD_ZERO(&writeSet);
        FD_SET(sock, &writeSet);
        timeout.tv_sec = TCP_CONNECT_TIMEOUT_MS / 1000;
        timeout.tv_usec = (TCP_CONNECT_TIMEOUT_MS % 1000) * 1000;

        result = select(sock + 1, NULL, &writeSet, NULL, &timeout);

        if (result <= 0)
        {
            if (g_reconnectAttempts[idx] == 0)
                RtPrintf("Error: connect timeout to %s:%d\n", ipAddr, port);
            close(sock);
            return ERROR_OCCURED;
        }

        // Check if connect succeeded
        socklen_t optLen = sizeof(optVal);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&optVal, &optLen);

        if (optVal != 0)
        {
            if (g_reconnectAttempts[idx] == 0)
                RtPrintf("Error: connect error %d to %s:%d\n", optVal, ipAddr, port);
            close(sock);
            return ERROR_OCCURED;
        }
    }

    // Set back to blocking mode
    fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);

    // Set send/recv timeouts
    struct timeval sendTimeout, recvTimeout;
    sendTimeout.tv_sec = TCP_SEND_TIMEOUT_MS / 1000;
    sendTimeout.tv_usec = (TCP_SEND_TIMEOUT_MS % 1000) * 1000;
    recvTimeout.tv_sec = TCP_RECV_TIMEOUT_MS / 1000;
    recvTimeout.tv_usec = (TCP_RECV_TIMEOUT_MS % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&sendTimeout, sizeof(sendTimeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));

    // Store socket
    g_tcpSock[idx] = sock;
    g_tcpConnected[idx] = TRUE;
    g_reconnectAttempts[idx] = 0;

    RtPrintf("Connected to %s:%d\n", ipAddr, port);

    return NO_ERRORS;
}

//--------------------------------------------------------
// DisconnectTcpHost
//--------------------------------------------------------
void DisconnectTcpHost(int idx)
{
    if (g_tcpSock[idx] != INVALID_SOCKET)
    {
        shutdown(g_tcpSock[idx], SHUT_RDWR);
        close(g_tcpSock[idx]);
        g_tcpSock[idx] = INVALID_SOCKET;
    }
    g_tcpConnected[idx] = FALSE;
}

//--------------------------------------------------------
// SendTcpMsg (unchanged logic)
//--------------------------------------------------------
int SendTcpMsg(int idx, char* pMsg, int len)
{
    int bytesSent;
    int totalSent = 0;

    if (!g_tcpConnected[idx] || g_tcpSock[idx] == INVALID_SOCKET)
    {
        RtPrintf("Error: Not connected idx %d, file %s, line %d\n",
                 idx, _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    while (totalSent < len)
    {
        bytesSent = send(g_tcpSock[idx], pMsg + totalSent, len - totalSent, MSG_NOSIGNAL);

        if (bytesSent < 0)
        {
            RtPrintf("Error: send failed %d (%s), idx %d, file %s, line %d\n",
                     errno, strerror(errno), idx, _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (bytesSent == 0)
        {
            RtPrintf("Error: send returned 0, file %s, line %d\n",
                     _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        totalSent += bytesSent;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// SendTcpHstMsg
//--------------------------------------------------------
int SendTcpHstMsg(int idx, char* pMsg, int len)
{
    return SendTcpMsg(idx, pMsg, len);
}

//--------------------------------------------------------
// SendTcpMbxServer (unchanged logic)
//--------------------------------------------------------
int RTFCNDCL SendTcpMbxServer(PVOID unused)
{
    mhdr* phdr;
    (void)unused;

    pShm->IsysLineStatus.app_threads |= W32_TX_SVR_RDY;

    RtPrintf("TCP SendMbxServer started\n");

    for (;;)
    {
        if (RtWaitForSingleObject(hInterfaceThreadsOk, INFINITE) == WAIT_FAILED)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        }
        else
        {
            if (RtWaitForSingleObject(hTxSemPost, INFINITE) == WAIT_FAILED)
            {
                RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            }
            else
            {
                phdr = (mhdr*)&pTxMsg->Buffer;

                if (pShm->IsysLineStatus.connected[pTxMsg->LineId - 1])
                {
                    if (SendTcpMsg(pTxMsg->LineId - 1, (char*)&pTxMsg->Buffer, pTxMsg->Len) != NO_ERRORS)
                    {
                        pShm->IsysLineStatus.connected[pTxMsg->LineId - 1] = FALSE;
                        DisconnectTcpHost(pTxMsg->LineId - 1);
                        RtPrintf("SendTcpMsg failed LineId %d\n", pTxMsg->LineId - 1);
                    }
                    else
                    {
                        pShm->IsysLineStatus.pkts_sent++;
                    }
                }

                pTxMsg->Ack = 1;

                if (!RtReleaseSemaphore(hTxSemAck, lReleaseCount, NULL))
                {
                    RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
                }
            }
        }
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// SendTcpHostMbxServer (unchanged logic)
//--------------------------------------------------------
int RTFCNDCL SendTcpHostMbxServer(PVOID unused)
{
    mhdr* phdr;
    (void)unused;

    pShm->IsysLineStatus.app_threads |= W32_HST_TX_SVR_RDY;

    RtPrintf("TCP SendHostMbxServer started\n");

    for (;;)
    {
        if (RtWaitForSingleObject(hInterfaceThreadsOk, INFINITE) == WAIT_FAILED)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        }
        else
        {
            if (RtWaitForSingleObject(hHstTxSemPost, INFINITE) == WAIT_FAILED)
            {
                RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            }
            else
            {
                phdr = (mhdr*)&pHstTxMsg->Buffer;

                if (pShm->IsysLineStatus.connected[HOST_ID - 1])
                {
                    if (SendTcpHstMsg(HOST_ID - 1, (char*)&pHstTxMsg->Buffer, pHstTxMsg->Len) != NO_ERRORS)
                    {
                        pShm->IsysLineStatus.connected[HOST_ID - 1] = FALSE;
                        DisconnectTcpHost(HOST_ID - 1);
                        RtPrintf("SendTcpHstMsg failed HOST_ID\n");
                    }
                    else
                    {
                        pShm->IsysLineStatus.pkts_sent++;
                    }
                }

                pHstTxMsg->Ack = 1;

                if (!RtReleaseSemaphore(hHstTxSemAck, lReleaseCount, NULL))
                {
                    RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
                }
            }
        }
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// TcpConnectionManager (unchanged logic)
//--------------------------------------------------------
int RTFCNDCL TcpConnectionManager()
{
    int         i;
    int         name_compare;
    int         valid_char;
    bool        active;
    int         this_line;
    char        current_host[32];

    RtPrintf("TCP ConnectionManager started\n");

    for (;;)
    {
        for (i = 0; i <= MAXIISYSLINES; i++)
        {
            active = false;

            if (i < MAXIISYSLINES)
            {
                if (pShm->sys_set.IsysLineSettings.active[i])
                    active = true;
            }
            else
            {
                // HOST connection (index 4) - always active.
                // This is the API/interface outbound connection, NOT an InterSystems line.
                // It must always reconnect regardless of InterSystems active flags.
                active = true;
            }

            if (active)
            {
                ParseHostAddress(pShm->sys_set.IsysLineSettings.pipe_path[i],
                                 current_host, &g_tcpPort[i]);

                name_compare = strcmp(g_tcpHost[i], current_host);

                if (name_compare != 0)
                {
                    DisconnectTcpHost(i);
                    pShm->IsysLineStatus.connected[i] = FALSE;
                }

                if (pShm->IsysLineStatus.connected[i] == FALSE)
                {
                    // Detect HOST connection lost (was connected, now disconnected)
                    if (i == TCP_HOST_INDEX && g_prevHostConnected)
                    {
                        g_prevHostConnected = FALSE;
                        RtPrintf("\n");
                        RtPrintf("*** Host Comms Lost ***\n");
                        RtPrintf("    Check switch and ethernet connections.\n");
                        RtPrintf("\n");
                    }

                    valid_char = 0;
                    for (int j = 0; current_host[j] != 0 && j < 16; j++)
                    {
                        if (isdigit(current_host[j]) || current_host[j] == '.')
                            valid_char++;
                    }

                    if (valid_char >= 7)
                    {
                        int connectResult = ConnectToTcpHost(i);

                        if (connectResult == NO_ERRORS)
                        {
                            pShm->IsysLineStatus.connected[i] = TRUE;

                            // Detect HOST connection established
                            if (i == TCP_HOST_INDEX)
                            {
                                g_prevHostConnected = TRUE;
                                RtPrintf("\n");
                                RtPrintf("*** Host Comms Normal ***\n");
                                RtPrintf("\n");
                            }
                        }
                        else
                        {
                            pShm->IsysLineStatus.connected[i] = FALSE;
                            DisconnectTcpHost(i);
                            g_reconnectAttempts[i]++;
                        }
                    }
                }
            }
        }

        Sleep(TCP_RECONNECT_INTERVAL_MS);
    }
}

//--------------------------------------------------------
// CleanupTcpClient
//--------------------------------------------------------
void CleanupTcpClient()
{
    for (int i = 0; i < INSTANCES; i++)
    {
        DisconnectTcpHost(i);
    }
}
