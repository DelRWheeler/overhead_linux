//////////////////////////////////////////////////////////////////////
// Main.cpp - Linux port of Overhead Main.cpp
//
// Entry point for the Overhead (backend) process.
// Ported from Windows RTX to Linux PREEMPT_RT.
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_          "Main.cpp"

char    main_err_buf [256];
char    date_dt_str[MAXDATETIMESTR];

void CheckHeartbeats();
static void ShutdownHandler(PVOID unused, LONG reason);

//--------------------------------------------------------
//       main():
//--------------------------------------------------------

// Debug: track why process exits
#include <execinfo.h>
static void overhead_atexit_handler()
{
    // Write to a KNOWN file to ensure we see this even if stderr is lost
    int efd = open("/tmp/overhead_exit.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "\n*** OVERHEAD PROCESS EXITING (atexit handler, pid=%d) ***\n", getpid());
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    // Print backtrace
    void* bt[32];
    int bt_size = backtrace(bt, 32);
    backtrace_symbols_fd(bt, bt_size, STDERR_FILENO);
    if (efd >= 0) backtrace_symbols_fd(bt, bt_size, efd);

    fsync(STDERR_FILENO);
    if (efd >= 0) { fsync(efd); close(efd); }
}

int main(int argc, char* argv[])
{
    time_t      long_time;
    struct tm   *ptime;

    atexit(overhead_atexit_handler);

    // Initialize console output (shared with Interface via console.h)
    ConsoleInit();

    RtPrintf("\n\n\nInitializing...\n\n");

//------------- Attach the shutdown handler

    if ((hShutdown = RtAttachShutdownHandler(NULL, 0, ShutdownHandler, NULL, RT_PRIORITY_MAX - 2)) == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtExitProcess(1);
    }

//------------- Create overhead instance and set a pointer to it

// A pointer is needed because the create task's task needs to be
// static to be able to use it. Static function can't reference
// non static members, so the pointer is used to get around this
// problem.

    if ((app = new overhead) == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtExitProcess(1);
    }

    Sleep (1000);
    app->initialize();

//------------- Show running

    Sleep (3000);

    app->bVersionMismatch = false;

    CREATE_VER_STRING(main_err_buf)

    // Board detection messages - on Linux we just report the platform
    RtPrintf("Running on Linux PREEMPT_RT\n");

    RtPrintf("%s",main_err_buf);
    memcpy(&app->pShm->app_ver, main_err_buf, strlen(main_err_buf));

//----- Get startup time, convert & print

    time( &long_time );
    ptime = localtime( &long_time );
    GetDateStr(date_dt_str);
    RtPrintf("Started %s\n\n", date_dt_str );

    // Start the debug shell interface to telnet server
#ifdef  _TELNET_UDP_ENABLED_
    app->InitDebugShell();
#endif

//------------- Loop until RUN_APP is set to FALSE

    while (app->pShm->AppFlags & RUN_APP)
    {
        CheckHeartbeats();
        Sleep (1000);
    }

    // don't lose any records
    app->SaveDropRecords(true);

//----- Get shutdown time, convert & print

    GetDateStr(date_dt_str);
    RtPrintf("Overhead RTOS Shutdown %s\n\n", date_dt_str );

    RtCancelTimer(hAppTimer, NULL);
    RtCancelTimer(hGpTimer, NULL);
    RtCloseHandle(hDrpMgr);
    RtCloseHandle(hDebug);
    // Don't leave any outputs on
    app->ClearOutputs();
    RtCloseHandle(hShmem);
    // hShutdown is a static marker, don't close it

    ConsoleShutdown();
    return 0;
}

//--------------------------------------------------------
//  CheckHeartbeats
//--------------------------------------------------------

void CheckHeartbeats()
{
    static int old_heartbeat    = 0;
    static int fail_count       = 0;
    static int IfDeadCount      = 0;
    static int IfLastCount      = 0;

//----- Watch GpTimer heartbeat. If it isn't working, communications is
//      probably hung. The problem should not happen except maybe at
//      startup.

    if (app->pShm->AppHeartbeat == old_heartbeat)
    {
        if (fail_count++ >= DEAD_COUNT)
        {
            GetDateStr(date_dt_str);
            RtPrintf("%s file %s line %d\n", date_dt_str, _FILE_, __LINE__);
            RtPrintf("\n****************************************************\n");
            RtPrintf("*  Overhead not responding, overhead shutdown       *\n");
            RtPrintf("****************************************************\n");
            app->pShm->AppFlags = 0;
        }

        if (fail_count > 5)
            RtPrintf("Overhead not responding, Dying count %d RTOS will shutdown at %d, last count was %d\n",
                      fail_count, DEAD_COUNT, app->pShm->AppHeartbeat);
    }
    else
    {
        fail_count    = 1;
        old_heartbeat = app->pShm->AppHeartbeat;

        if (app->pShm->AppHeartbeat > 32000)
            app->pShm->AppHeartbeat = 0;
    }

//----- Watch Interface heartbeat.
//      Only enforce if interface was previously running (heartbeat > 0)

    if (app->pShm->IfServerHeartbeat == IfLastCount)
    {
        // Don't kill overhead if interface never started (heartbeat still 0)
        if (IfLastCount == 0 && app->pShm->IfServerHeartbeat == 0)
        {
            IfDeadCount = 0;  // Reset - interface hasn't connected yet
        }
        else if (IfDeadCount++ >= DEAD_COUNT)
        {
            RtPrintf("%s file %s line %d\n", date_dt_str, _FILE_, __LINE__);
            RtPrintf("\n****************************************************\n");
            RtPrintf("* Interface not responding, overhead shutdown       *\n");
            RtPrintf("****************************************************\n");
            app->pShm->AppFlags = 0;
        }

        if (IfDeadCount > 5)
            RtPrintf("Interface not responding, Dying count %d RTOS will shutdown at %d, last count was %d\n",
                      IfDeadCount, DEAD_COUNT, app->pShm->IfServerHeartbeat);
    }
    else
    {
        IfDeadCount = 1;
        IfLastCount = app->pShm->IfServerHeartbeat;

        if (app->pShm->IfServerHeartbeat > 32000)
            app->pShm->IfServerHeartbeat = 0;
    }
}

//--------------------------------------------------------
//  ShutdownHandler
//--------------------------------------------------------

void ShutdownHandler(PVOID unused, LONG reason)
{
    (void)unused;
    char    *s;

    switch (reason)
    {
        case RT_SHUTDOWN_NT_SYSTEM_SHUTDOWN:
            s = (char*)"System shutdown signal";
            break;

        case RT_SHUTDOWN_NT_STOP:
            s = (char*)"Stop signal";
            break;

        default:
            s = (char*)"UNKNOWN";
    }

    RtPrintf("SHUTDOWN HANDLER: Received \"%s\" notification.\n", s);
    // don't lose any records
    app->saveDrpRecs = true;
    Sleep(500);
    app->ClearOutputs();
    app->pShm->AppFlags = 0;
    Sleep(3000);
}
