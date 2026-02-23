//////////////////////////////////////////////////////////////////////
// Main.cpp - Linux port of Overhead Main.cpp
//
// Entry point for the Overhead (backend) process.
// Ported from Windows RTX to Linux PREEMPT_RT.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include <sys/syscall.h>

#undef  _FILE_
#define _FILE_          "Main.cpp"

char    main_err_buf [256];
char    date_dt_str[MAXDATETIMESTR];

void CheckHeartbeats();
static void ShutdownHandler(PVOID unused, LONG reason);

//--------------------------------------------------------
// Diagnostic sentinels for crash analysis
//
// These globals are OUTSIDE the overhead class so a class
// member overflow cannot corrupt them. They let the crash
// handler determine whether app->pShm was overwritten vs
// the mmap region being invalidated.
//--------------------------------------------------------
volatile void* g_sentinel_pShm  = NULL;  // set once after InitMem
volatile void* g_sentinel_app   = NULL;  // set once after new overhead
volatile int   g_sentinel_magic = 0;     // set to 0xDEADBEEF after init
volatile int   g_shm_fd         = -1;    // fd for SharedMemory (for proc/maps check)

//--------------------------------------------------------
//       main():
//--------------------------------------------------------

// Debug: track why process exits and crashes
#include <execinfo.h>
#include <ucontext.h>

static void overhead_crash_handler(int sig, siginfo_t *info, void *ctx)
{
    // Write to BOTH stdout and stderr and a file - ensure something gets captured
    const char marker[] = "\n!!! OVERHEAD CRASH HANDLER ENTERED !!!\n";
    write(STDOUT_FILENO, marker, sizeof(marker) - 1);
    write(STDERR_FILENO, marker, sizeof(marker) - 1);

    ucontext_t *uc = (ucontext_t *)ctx;
    int efd = open("/tmp/overhead_crash.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char buf[1024];
    int len;

    // === Basic crash info ===
    len = snprintf(buf, sizeof(buf),
        "\n*** OVERHEAD CRASHED (signal %d, pid=%d, tid=%d) ***\n"
        "Fault address: %p\n"
        "RIP: 0x%llx\n"
        "RSP: 0x%llx\n",
        sig, getpid(), (int)syscall(SYS_gettid), info->si_addr,
        (unsigned long long)uc->uc_mcontext.gregs[REG_RIP],
        (unsigned long long)uc->uc_mcontext.gregs[REG_RSP]);
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    // === Sentinel diagnostics ===
    len = snprintf(buf, sizeof(buf),
        "\n--- SENTINEL DIAGNOSTICS ---\n"
        "g_sentinel_magic: 0x%X (expect 0xDEADBEEF)\n"
        "g_sentinel_app:   %p\n"
        "g_sentinel_pShm:  %p\n"
        "g_shm_fd:         %d\n",
        g_sentinel_magic,
        g_sentinel_app,
        g_sentinel_pShm,
        g_shm_fd);
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    // === Try to read current app and app->pShm ===
    // Use volatile reads to prevent optimization
    volatile overhead* cur_app = app;
    len = snprintf(buf, sizeof(buf),
        "app (current):    %p (sentinel match: %s)\n",
        (void*)cur_app,
        (cur_app == (overhead*)g_sentinel_app) ? "YES" : "NO");
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    if (cur_app != NULL) {
        // Try to read pShm from app — this might fault if app is corrupted
        // but since we're already in a crash handler with SA_RESETHAND,
        // a double fault would just kill us with default handler (acceptable)
        volatile void* cur_pShm = cur_app->pShm;
        len = snprintf(buf, sizeof(buf),
            "app->pShm:        %p (sentinel match: %s)\n",
            (void*)cur_pShm,
            (cur_pShm == g_sentinel_pShm) ? "YES" : "NO");
        write(STDOUT_FILENO, buf, len);
        write(STDERR_FILENO, buf, len);
        if (efd >= 0) write(efd, buf, len);

        // Try to read AppFlags from pShm to test if mapping is valid
        if (cur_pShm != NULL) {
            // Attempt to read the first int (AppFlags) from the mapping
            volatile int flags = ((volatile app_type::SHARE_MEMORY*)cur_pShm)->AppFlags;
            len = snprintf(buf, sizeof(buf),
                "pShm->AppFlags:   %d (mapping VALID)\n", flags);
            write(STDOUT_FILENO, buf, len);
            write(STDERR_FILENO, buf, len);
            if (efd >= 0) write(efd, buf, len);
        }
    }

    // === Check /proc/self/maps for SharedMemory mapping ===
    len = snprintf(buf, sizeof(buf), "\n--- /proc/self/maps (SharedMemory) ---\n");
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    int mfd = open("/proc/self/maps", O_RDONLY);
    if (mfd >= 0) {
        char mapbuf[8192];
        int nr;
        while ((nr = read(mfd, mapbuf, sizeof(mapbuf) - 1)) > 0) {
            mapbuf[nr] = '\0';
            // Search for lines containing "SharedMemory" or the fault address range
            char* line = mapbuf;
            while (line && *line) {
                char* nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                // Print lines with SharedMemory or any shm mapping
                if (strstr(line, "SharedMemory") || strstr(line, "/dev/shm/")) {
                    int ll = strlen(line);
                    line[ll] = '\n';
                    write(STDOUT_FILENO, line, ll + 1);
                    write(STDERR_FILENO, line, ll + 1);
                    if (efd >= 0) write(efd, line, ll + 1);
                    line[ll] = '\0';  // restore for next iteration
                }
                if (nl) { *nl = '\n'; line = nl + 1; }
                else break;
            }
        }
        close(mfd);
    }

    // === Check if Interface parent process is still alive ===
    pid_t ppid = getppid();
    len = snprintf(buf, sizeof(buf),
        "\n--- PARENT PROCESS ---\n"
        "Parent PID: %d (1 = reparented to init = parent died)\n",
        (int)ppid);
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    // === Check SharedMemory file ===
    struct stat shm_stat;
    if (stat("/dev/shm/SharedMemory", &shm_stat) == 0) {
        len = snprintf(buf, sizeof(buf),
            "/dev/shm/SharedMemory: size=%ld, inode=%ld\n",
            (long)shm_stat.st_size, (long)shm_stat.st_ino);
    } else {
        len = snprintf(buf, sizeof(buf),
            "/dev/shm/SharedMemory: NOT FOUND (errno=%d: %s)\n",
            errno, strerror(errno));
    }
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    // === Backtrace ===
    len = snprintf(buf, sizeof(buf), "\nBacktrace:\n");
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

    void* bt[32];
    int bt_size = backtrace(bt, 32);
    backtrace_symbols_fd(bt, bt_size, STDOUT_FILENO);
    backtrace_symbols_fd(bt, bt_size, STDERR_FILENO);
    if (efd >= 0) backtrace_symbols_fd(bt, bt_size, efd);

    fsync(STDOUT_FILENO);
    fsync(STDERR_FILENO);
    if (efd >= 0) { fsync(efd); close(efd); }
    _exit(128 + sig);
}

static void overhead_atexit_handler()
{
    int efd = open("/tmp/overhead_exit.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "\n*** OVERHEAD PROCESS EXITING (atexit handler, pid=%d) ***\n", getpid());
    write(STDERR_FILENO, buf, len);
    if (efd >= 0) write(efd, buf, len);

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

    // Install alternate signal stack so crash handler works even on stack overflow
    {
        static char altstack_buf[32768]; // SIGSTKSZ + extra for backtrace
        stack_t ss;
        ss.ss_sp = altstack_buf;
        ss.ss_size = sizeof(altstack_buf);
        ss.ss_flags = 0;
        sigaltstack(&ss, NULL);
    }

    // Install crash signal handler for debugging
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = overhead_crash_handler;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGFPE, &sa, NULL);
    }

    // Enable direct port I/O access (requires root, must be done before any I/O)
    if (iopl(3) != 0) {
        fprintf(stderr, "FATAL: iopl(3) failed: %s (need root?)\n", strerror(errno));
        return 1;
    }

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

    // Store sentinel BEFORE any threads start
    g_sentinel_app = (void*)app;

    Sleep (1000);
    app->initialize();

    // Store pShm sentinel AFTER initialize (which calls InitMem)
    g_sentinel_pShm = (void*)app->pShm;
    g_sentinel_magic = 0xDEADBEEF;

    // Save the SharedMemory fd for diagnostics
    if (hShmem) {
        PlatformHandle* ph = (PlatformHandle*)hShmem;
        if (ph->type == HANDLE_SHM)
            g_shm_fd = ph->shm.fd;
    }

    RtPrintf("=== SENTINEL VALUES ===\n");
    RtPrintf("  app            = %p\n", (void*)app);
    RtPrintf("  app->pShm      = %p\n", (void*)app->pShm);
    RtPrintf("  g_sentinel_app = %p\n", g_sentinel_app);
    RtPrintf("  g_sentinel_pShm= %p\n", g_sentinel_pShm);
    RtPrintf("  g_shm_fd       = %d\n", g_shm_fd);
    RtPrintf("  sizeof(SHARE_MEMORY) = %lu\n", (unsigned long)sizeof(app_type::SHARE_MEMORY));
    RtPrintf("=======================\n");

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

    // Clear outputs before exiting
    app->ClearOutputs();

    // CRITICAL FIX: On Linux, RtCloseHandle(hShmem) calls munmap() which
    // invalidates pShm.  But GpSendThread, DropManager, DebugThread, and
    // timer threads are still running and accessing pShm.  This caused
    // SIGSEGV crashes (all threads fault on the unmapped shared memory).
    //
    // On Windows RTX, RtExitProcess() kills all threads atomically.
    // On Linux, we must use _exit() which terminates all threads and
    // releases all resources (mmap, fds, etc.) in one atomic step.
    // Do NOT call RtCloseHandle(hShmem) here — let _exit() clean up.
    ConsoleShutdown();
    _exit(0);
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

    // Guard: skip heartbeat check if pShm is invalid
    if (!isPShmValid()) return;

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
