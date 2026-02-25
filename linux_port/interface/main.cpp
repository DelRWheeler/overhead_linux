//////////////////////////////////////////////////////////////////////
// main.cpp: Linux port of main_tcp.cpp
//
// DCH Server - Overhead Sizing System Interface Process
//
// This is the main entry point for the Interface (communications)
// subsystem, ported from RTX/Windows to Linux PREEMPT_RT.
//
// Key changes from RTX version:
//   - POSIX threads replace Windows threads
//   - POSIX shared memory/semaphores replace RTX IPC
//   - POSIX sockets (already similar to WinSock)
//   - Signal handlers replace Console handlers
//   - syslog replaces Windows Event Log
//   - fork/exec replaces _spawnl
//   - All communication protocols and message formats unchanged
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "tcpconfig.h"
#include "console.h"
#include <time.h>
#include <sys/syscall.h>

#undef  _FILE_
#define _FILE_      "main.cpp"

// Write diagnostic to /tmp/overhead_appflags.log when Interface touches AppFlags
static void logInterfaceAppFlags(const char* reason)
{
    int fd = open("/tmp/overhead_appflags.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    char buf[256];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    int len = snprintf(buf, sizeof(buf),
        "[%02d:%02d:%02d.%03ld] PID=%d INTERFACE: %s\n---\n",
        tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000,
        getpid(), reason);
    write(fd, buf, len);
    fsync(fd);
    close(fd);
}

// External declarations
extern void TelnetMain();
extern int  RTFCNDCL TcpServer(PVOID unused);
extern int  RTFCNDCL TcpConnectionManager();
extern int  RTFCNDCL SendTcpMbxServer(PVOID unused);
extern int  RTFCNDCL SendTcpHostMbxServer(PVOID unused);
extern void InitTcpClientSockets();
extern void CleanupTcpServer();
extern void CleanupTcpClient();

// Global variables
app_type::SHARE_MEMORY *pShm;
app_type::TRACE_MEMORY *pTraceMemory;
SOCKADDR_IN saUdpServ, saUdpCli;

HANDLE              hConnectionManager;
HANDLE              hMbxServer;
HANDLE              hHstMbxServer;
HANDLE              hTcpServer;
HANDLE              hUdpTraceThread;
HANDLE              hShmem;
HANDLE              hDebugTraceShmem;
HANDLE              hInterfaceThreadsOk;
bool                err_routine_close  = false;
int                 thread_errors;

// declarations for tx mailbox
HANDLE              hTxShm,  hTxMutex, hTxSemPost, hTxSemAck;
PAPPIPCMSG          pTxMsg;
// declarations for host tx mailbox
HANDLE              hHstTxShm,  hHstTxMutex, hHstTxSemPost, hHstTxSemAck;
PAPPIPCMSG          pHstTxMsg;
// declarations for rx mailbox
HANDLE              hRxShm, hRxMutex, hRxSemPost, hRxSemAck;
PAPPIPCMSG          pRxMsg;

HANDLE  hLog = NULL;
char*   pEvtStr;
char    evt_str[256];

// Compatibility stubs (not used with TCP but kept for shared code)
PIPEINST Pipe[INSTANCES];
HANDLE   hEvents[INSTANCES];
HANDLE   hPipe[INSTANCES];
LPTSTR   lpszPipename[INSTANCES];
char     pipe_str[INSTANCES][MAXIISYSNAME];

// Global socket for UDP trace
SOCKET sock = INVALID_SOCKET;

// Backend child process PID for monitoring
static pid_t g_overhead_pid = 0;
BOOL fStarted = FALSE;

// Function prototypes
void AppStartTcp();
BOOL ConsoleHandler(DWORD CEvent);
void Heartbeats();
void ErrorRoutine(BYTE *sbuf);
void DumpData(BYTE *pbuf, int len);
int  CreateTxMailbox();
int  CreateHstTxMailbox();
int  CreateRxMailbox();
void SetTime(SYSTEMTIME *dt);
void Shutdown(int action);

#ifdef _TELNET_UDP_ENABLED_
int DoUdpClient(USHORT usEndPoint);
#endif

void PrintError(LPSTR lpszRoutine, LPSTR lpszCallName, DWORD dwError);
void DoCleanup(void);

//--------------------------------------------------------
//   main
//--------------------------------------------------------

int main(int argc, char* argv[])
{
    int iRetVal;
    DWORD threadId;
    USHORT usEndPoint = 8000;
    DWORD dwStackSize = 0;

    // Ignore SIGPIPE - prevents process termination when writing
    // to a TCP socket whose remote end has closed. Without this,
    // send() on a broken connection kills the process. With this,
    // send() returns -1 with errno=EPIPE which we handle gracefully.
    signal(SIGPIPE, SIG_IGN);

    // Initialize DCH Server console
    ConsoleInit();

    RtPrintf("\n");
    RtPrintf("DCH Server starting...\n");
    RtPrintf("\n");

    // Initialize compatibility structures
    for (int i = 0; i < INSTANCES; i++)
    {
        Pipe[i].hPipeInst = INVALID_HANDLE_VALUE;
        hEvents[i] = NULL;
        hPipe[i] = NULL;
        memset(pipe_str[i], 0, MAXIISYSNAME);
    }

    // Initialize TCP client sockets
    InitTcpClientSockets();

    //----- Register event source (syslog on Linux)
    hLog = RegisterEventSource(NULL, "DCHServer");

    if (hLog == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: Register Event Source");
    }

    //----- Create signal handler (replaces Console handler)
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: Create console handler");
    }

    //----- Create IPC event for thread synchronization
    hInterfaceThreadsOk = RtCreateEvent(NULL, true, 0, "IPC Threads");

    if (hInterfaceThreadsOk == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: Create IPC event threads");
    }

    //----- Create mailboxes/start threads
    AppStartTcp();

    //----- Show started
    pEvtStr = (char*)VER1;

    ReportEvent(hLog, EVENTLOG_INFORMATION_TYPE,
                0, 0, NULL, 1, 0,
                (LPCTSTR*)&pEvtStr, NULL);

    CREATE_VER_STRING(evt_str)
    memcpy(&pShm->comm_ver, evt_str, strlen(evt_str));

    RtPrintf("%s\n", evt_str);

#ifdef _TELNET_UDP_ENABLED_
    // Setup UDP trace
    usEndPoint = (USHORT)(usEndPoint + pShm->sys_set.IsysLineSettings.this_line_id);
    RtPrintf("UDP Trace port %d\n", usEndPoint);

    iRetVal = DoUdpClient(usEndPoint);

    if (iRetVal == 0)
    {
        fStarted = TRUE;

        // Create UDP trace thread
        hUdpTraceThread = CreateThread(NULL, dwStackSize,
                                       (LPTHREAD_START_ROUTINE)UdpTraceThread,
                                       NULL, CREATE_SUSPENDED, (LPDWORD)&threadId);

        if (hUdpTraceThread != NULL)
        {
            RtSetThreadPriority(hUdpTraceThread, RT_PRIORITY_MIN);
            ResumeThread(hUdpTraceThread);
            RtPrintf("Started UdpTraceThread\n");
        }

        // Start telnet service
        TelnetMain();
    }
#endif

    RtPrintf("DCH Server running - main loop started\n");

    //----------------------- MAIN LOOP ---------------------------------------

    do
    {
        //----- Show interface is alive to backend
        Heartbeats();

        //----- Check thread status
        if (pShm->IsysLineStatus.app_threads == ALL_SYSTEMS_READY)
        {
            thread_errors = 0;
            if (!RtSetEvent(hInterfaceThreadsOk))
                RtPrintf("Set event status = %d\n", GetLastError());
        }
        else
        {
            RtResetEvent(hInterfaceThreadsOk);

            if (thread_errors % 3 == 0 || thread_errors > FATAL_ERR_CNT - 2)
                RtPrintf("app_threads = 0x%02X (need 0x%02X), errors=%d/%d\n",
                         pShm->IsysLineStatus.app_threads, ALL_SYSTEMS_READY,
                         thread_errors, FATAL_ERR_CNT);

            if (++thread_errors > FATAL_ERR_CNT) {
                logInterfaceAppFlags("Main loop: thread_errors > FATAL_ERR_CNT, calling Shutdown");
                Shutdown(PIPE_ERR_RESTART_NT);
            }
        }

        // Check for hung mailboxes
        for (int hng = 0; hng < MBX_STATUSES; hng++)
        {
            if (pShm->mbx_status[hng].hung)
            {
                sprintf(evt_str, "Mailbox hung, mbx %d", hng);
                pEvtStr = (char*)&evt_str;

                ReportEvent(hLog, EVENTLOG_ERROR_TYPE,
                            0, 0, NULL, 1, 0,
                            (LPCTSTR*)&pEvtStr, NULL);

                Shutdown(PIPE_ERR_RESTART_NT);
            }
        }

        // Monitor child process (Overhead) - check if it died unexpectedly
        if (g_overhead_pid > 0) {
            int wstatus = 0;
            pid_t wpid = waitpid(g_overhead_pid, &wstatus, WNOHANG);
            if (wpid == g_overhead_pid) {
                if (WIFEXITED(wstatus))
                    RtPrintf("WARNING: Overhead process (pid %d) exited with code %d\n",
                             g_overhead_pid, WEXITSTATUS(wstatus));
                else if (WIFSIGNALED(wstatus))
                    RtPrintf("WARNING: Overhead process (pid %d) killed by signal %d%s\n",
                             g_overhead_pid, WTERMSIG(wstatus),
                             WCOREDUMP(wstatus) ? " (core dumped)" : "");
                else
                    RtPrintf("WARNING: Overhead process (pid %d) died, status=0x%x\n",
                             g_overhead_pid, wstatus);
                g_overhead_pid = 0;
            }
        }

        Sleep(950);

    } while (pShm && (pShm->AppFlags & 1));

    // Log WHY the main loop exited
    {
        char exit_reason[256];
        snprintf(exit_reason, sizeof(exit_reason),
            "Main loop exited: pShm=%p AppFlags=%d err_routine_close=%d overhead_pid=%d",
            (void*)pShm, pShm ? pShm->AppFlags : -1,
            (int)err_routine_close, (int)g_overhead_pid);
        logInterfaceAppFlags(exit_reason);
    }

    if (!err_routine_close)
        ErrorRoutine((BYTE*)"Error: Overhead shutdown");

    ConsoleShutdown();
    return 0;
}

//--------------------------------------------------------
//  AppStartTcp - Start application with TCP
//--------------------------------------------------------

void AppStartTcp()
{
    DWORD threadId;
    DWORD dwStackSize = 0;
    int status = -1;

    thread_errors = 0;

    // Create mailboxes (POSIX shared memory + semaphores)
    if (CreateTxMailbox() == ERROR_OCCURED)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateTxMailbox");
    }

    if (CreateHstTxMailbox() == ERROR_OCCURED)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateHstTxMailbox");
    }

    if (CreateRxMailbox() == ERROR_OCCURED)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateRxMailbox");
    }

    //------------- Create shared memory
    if ((hShmem = RtCreateSharedMemory(PAGE_READWRITE, 0,
                                       sizeof(app_type::SHARE_MEMORY),
                                       "SharedMemory", (VOID**)&pShm)) == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateSharedMemory");
    }

    RtPrintf("Shared memory created (%lu bytes)\n", (unsigned long)sizeof(app_type::SHARE_MEMORY));

    if ((hDebugTraceShmem = RtCreateSharedMemory(PAGE_READWRITE, 0,
                                                  sizeof(app_type::TRACE_MEMORY),
                                                  "TraceMemory", (VOID**)&pTraceMemory)) == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateTraceMemory");
    }

    RtPrintf("Trace memory created\n");

    Sleep(1000);

    //------------- Start backend (Overhead process)
    pShm->AppFlags = RUN_APP;
    pShm->OpMode = ModeStart;

    status = SpawnProcess(DCHSERVICES_BASE "/bin/overhead");
    if (status == ERROR_OCCURED)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: Spawn app");
    }
    else
    {
        g_overhead_pid = (pid_t)status;
        RtPrintf("Backend process spawned (pid %d)\n", status);
    }

    Sleep(2000);

    //------------- Create TCP Server thread
    hTcpServer = CreateThread(NULL, dwStackSize,
                              (LPTHREAD_START_ROUTINE)TcpServer,
                              NULL, CREATE_SUSPENDED, (LPDWORD)&threadId);

    if (hTcpServer == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateThread TcpServer");
    }

    RtSetThreadPriority(hTcpServer, RT_PRIORITY_MIN + 50);
    ResumeThread(hTcpServer);
    RtPrintf("Started TCP Server thread\n");

    //------------- Create TCP Connection Manager thread
    hConnectionManager = CreateThread(NULL, dwStackSize,
                                      (LPTHREAD_START_ROUTINE)TcpConnectionManager,
                                      NULL, CREATE_SUSPENDED, (LPDWORD)&threadId);

    if (hConnectionManager == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateThread TcpConnectionManager");
    }

    RtSetThreadPriority(hConnectionManager, RT_PRIORITY_MIN + 50);
    ResumeThread(hConnectionManager);
    RtPrintf("Started TCP ConnectionManager thread\n");

    //------------- Create TCP Send Server thread
    hMbxServer = CreateThread(NULL, dwStackSize,
                              (LPTHREAD_START_ROUTINE)SendTcpMbxServer,
                              NULL, CREATE_SUSPENDED, (LPDWORD)&threadId);

    if (hMbxServer == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateThread SendTcpMbxServer");
    }

    RtSetThreadPriority(hMbxServer, RT_PRIORITY_MIN + 50);
    ResumeThread(hMbxServer);
    RtPrintf("Started TCP SendMbxServer thread\n");

    //------------- Create TCP Host Send Server thread
    hHstMbxServer = CreateThread(NULL, dwStackSize,
                                 (LPTHREAD_START_ROUTINE)SendTcpHostMbxServer,
                                 NULL, CREATE_SUSPENDED, (LPDWORD)&threadId);

    if (hHstMbxServer == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        ErrorRoutine((BYTE*)"Error: CreateThread SendTcpHostMbxServer");
    }

    RtSetThreadPriority(hHstMbxServer, RT_PRIORITY_MIN + 50);
    ResumeThread(hHstMbxServer);
    RtPrintf("Started TCP SendHostMbxServer thread\n");
}

//--------------------------------------------------------
//  Heartbeats
//--------------------------------------------------------

void Heartbeats()
{
    static int prev_app_heartbeat = 0;
    static int app_fail_count = 0;

    if (!pShm) return;

    if (prev_app_heartbeat == 0)
        prev_app_heartbeat = pShm->AppHeartbeat;

    // Show interface is alive to backend
    if (++pShm->IfServerHeartbeat > 32000)
        pShm->IfServerHeartbeat = 0;

    // Check if backend is still alive
    if (pShm->AppHeartbeat == prev_app_heartbeat)
    {
        app_fail_count++;
        RtPrintf("Heartbeat check: AppHB=%d prev=%d fail=%d pShm=%p\n",
                 pShm->AppHeartbeat, prev_app_heartbeat, app_fail_count, (void*)pShm);

        if (app_fail_count > 30) {
            logInterfaceAppFlags("Heartbeats: app_fail_count > 30, calling ErrorRoutine");
            ErrorRoutine((BYTE*)"Error: app_fail_count > 30");
        }
    }
    else
    {
        prev_app_heartbeat = pShm->AppHeartbeat;
        app_fail_count = 0;
    }
}

//--------------------------------------------------------
// ConsoleHandler - Handle signals (SIGINT, SIGTERM, etc.)
//--------------------------------------------------------

BOOL ConsoleHandler(DWORD CEvent)
{
    switch (CEvent)
    {
    case CTRL_C_EVENT:
        RtPrintf("CTRL+C received!\n");
        break;
    case CTRL_CLOSE_EVENT:
        RtPrintf("Program being closed!\n");
        ErrorRoutine((BYTE*)"Error: Close event");
        break;
    case CTRL_LOGOFF_EVENT:
        RtPrintf("User is logging off!\n");
        break;
    case CTRL_SHUTDOWN_EVENT:
        RtPrintf("System shutting down!\n");
        ErrorRoutine((BYTE*)"Error: Shutdown event");
        break;
    }
    return TRUE;
}

//--------------------------------------------------------
//  ErrorRoutine
//--------------------------------------------------------

void ErrorRoutine(BYTE *sbuf)
{
    RtPrintf("%s err\n", sbuf);

    // Log to file BEFORE setting AppFlags — this tells us if Interface cleared it
    {
        char reason[256];
        snprintf(reason, sizeof(reason), "ErrorRoutine: %s (AppFlags was %d)",
                 (const char*)sbuf, pShm ? pShm->AppFlags : -1);
        logInterfaceAppFlags(reason);
    }

    pEvtStr = (char*)sbuf;

    ReportEvent(hLog, EVENTLOG_ERROR_TYPE,
                0, 0, NULL, 1, 0,
                (LPCTSTR*)&pEvtStr, NULL);

    err_routine_close = true;

    if (pShm)
        pShm->AppFlags = 0;
    Sleep(5000);

    // Cancel threads
    TerminateThread(hMbxServer, 0);
    TerminateThread(hHstMbxServer, 0);
    TerminateThread(hTcpServer, 0);
    TerminateThread(hConnectionManager, 0);

    // Cleanup TCP connections
    CleanupTcpServer();
    CleanupTcpClient();

    ConsoleShutdown();
    ExitProcess(1);
}

//--------------------------------------------------------
//  DumpData
//--------------------------------------------------------

void DumpData(BYTE *pbuf, int len)
{
    int i;
    BYTE cs = 0;

    RtPrintf("DumpData\n");
    for (i = 0; i < len; i++)
    {
        RtPrintf("%2.2x ", *pbuf);
        cs = (BYTE)(cs + *pbuf++);
    }
    RtPrintf("\n");
}

//--------------------------------------------------------
//  SetTime
//--------------------------------------------------------

void SetTime(SYSTEMTIME *dt)
{
    SYSTEMTIME SysTime;
    GetLocalTime(&SysTime);

    memcpy(&SysTime, dt, sizeof(SYSTEMTIME));

    sprintf(evt_str, "Host date/time set %d:%d:%d %d:%d:%d",
            dt->wMonth, dt->wDay, dt->wYear,
            dt->wHour, dt->wMinute, dt->wSecond);

    pEvtStr = (char*)&evt_str;
    RtPrintf("%s\n", evt_str);

    ReportEvent(hLog, EVENTLOG_SUCCESS,
                0, 0, NULL, 1, 0,
                (LPCTSTR*)&pEvtStr, NULL);

    SetLocalTime(&SysTime);
}

//--------------------------------------------------------
//  Shutdown
//--------------------------------------------------------

void Shutdown(int action)
{
    if (action == RESTART_APP)
    {
        sprintf(evt_str, "Host initiated App restart");
        pEvtStr = (char*)&evt_str;

        ReportEvent(hLog, EVENTLOG_ERROR_TYPE,
                    0, 0, NULL, 1, 0,
                    (LPCTSTR*)&pEvtStr, NULL);

        RtPrintf("%s\n", evt_str);
        logInterfaceAppFlags("Shutdown(RESTART_APP): Host initiated App restart");

        if (pShm)
            pShm->AppFlags = 0;
        return;
    }

    switch (action)
    {
    case PIPE_ERR_RESTART_NT:
        sprintf(evt_str, "Interface flags = %d system restart",
                pShm ? pShm->IsysLineStatus.app_threads : -1);
        pEvtStr = (char*)&evt_str;

        ReportEvent(hLog, EVENTLOG_ERROR_TYPE,
                    0, 0, NULL, 1, 0,
                    (LPCTSTR*)&pEvtStr, NULL);

        RtPrintf("%s\n", evt_str);
        logInterfaceAppFlags("Shutdown(PIPE_ERR_RESTART_NT): thread status bad");

        if (pShm)
            pShm->AppFlags = 0;
        Sleep(500);

        CleanupTcpServer();
        CleanupTcpClient();

        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
        break;

    case RESTART_NT:
        sprintf(evt_str, "Host initiated system restart");
        pEvtStr = (char*)&evt_str;

        ReportEvent(hLog, EVENTLOG_ERROR_TYPE,
                    0, 0, NULL, 1, 0,
                    (LPCTSTR*)&pEvtStr, NULL);

        RtPrintf("%s\n", evt_str);
        logInterfaceAppFlags("Shutdown(RESTART_NT): Host initiated system restart");

        if (pShm)
            pShm->AppFlags = 0;
        Sleep(500);

        CleanupTcpServer();
        CleanupTcpClient();

        ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0);
        break;

    case SHUTDOWN_NT:
        sprintf(evt_str, "Host initiated system shutdown");
        pEvtStr = (char*)&evt_str;

        ReportEvent(hLog, EVENTLOG_ERROR_TYPE,
                    0, 0, NULL, 1, 0,
                    (LPCTSTR*)&pEvtStr, NULL);

        RtPrintf("%s\n", evt_str);
        logInterfaceAppFlags("Shutdown(SHUTDOWN_NT): Host initiated system shutdown");

        if (pShm)
            pShm->AppFlags = 0;
        Sleep(500);

        CleanupTcpServer();
        CleanupTcpClient();

        ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0);
        break;
    }
}

//--------------------------------------------------------
// DoCleanup
//--------------------------------------------------------

void DoCleanup(void)
{
    if (sock != INVALID_SOCKET)
    {
        close(sock);
        sock = INVALID_SOCKET;
    }
}

#ifdef _TELNET_UDP_ENABLED_
//--------------------------------------------------------
// DoUdpClient - Setup UDP broadcast for trace
//--------------------------------------------------------

int DoUdpClient(USHORT usEndPoint)
{
    INT err = ERROR_OCCURED;
    CHAR achMessage[80];
    int fBroadcast = 1;

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock == INVALID_SOCKET)
    {
        RtPrintf("DoUdpClient socket error %d\n", errno);
        return err;
    }

    err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                     (char*)&fBroadcast, sizeof(fBroadcast));

    if (err < 0)
    {
        RtPrintf("DoUdpClient setsockopt error %d\n", errno);
        return ERROR_OCCURED;
    }

    saUdpCli.sin_family = AF_INET;
    saUdpCli.sin_addr.s_addr = htonl(INADDR_ANY);
    saUdpCli.sin_port = htons(0);

    err = bind(sock, (SOCKADDR*)&saUdpCli, sizeof(SOCKADDR_IN));

    if (err < 0)
    {
        RtPrintf("DoUdpClient bind error %d\n", errno);
        return ERROR_OCCURED;
    }

    saUdpServ.sin_family = AF_INET;
    saUdpServ.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    saUdpServ.sin_port = htons(usEndPoint);

    strcpy(achMessage, "DCH Server Interface started");

    err = sendto(sock, achMessage, strlen(achMessage), 0,
                 (SOCKADDR*)&saUdpServ, sizeof(SOCKADDR_IN));

    if (err < 0)
    {
        // ENETUNREACH (101) = no default route, normal on isolated networks.
        // Socket is still valid - UDP tracing will work for local subnet.
        if (errno == ENETUNREACH)
            RtPrintf("DoUdpClient: no route for broadcast (OK on isolated network)\n");
        else
            RtPrintf("DoUdpClient sendto error %d\n", errno);
    }

    return NO_ERRORS;
}
#endif

void PrintError(LPSTR lpszRoutine, LPSTR lpszCallName, DWORD dwError)
{
    RtPrintf("The Call to %s() in routine() %s failed with error %d\n",
             lpszCallName, lpszRoutine, dwError);
    DoCleanup();
    exit(1);
}
