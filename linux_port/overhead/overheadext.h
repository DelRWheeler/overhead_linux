#pragma once

#include "types.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

//--------------------------------------------------------
//  External Variables
//--------------------------------------------------------

extern overhead*     app;
extern io*           iop;
extern io*           iop_2;
extern io*           ios;
extern LoadCell*     ld_cel;
extern InterSystems* Isys;
extern DropManager*  drpman;
extern Simulator*    sim;
extern const BYTE    Mask[];
extern HANDLE        hDrpMgr;
extern HANDLE        hShmem;
extern HANDLE        hAppTimer;
extern HANDLE        hDebug;
extern HANDLE        hGpSend;
extern HANDLE        hGpTimer;
extern HANDLE        hShutdown;
extern HANDLE        hAppMbx;
extern HANDLE        hAppRx;
extern HANDLE        hIsys;
extern HANDLE        hIsysRx;
extern HANDLE        hIsysMbx;
extern HANDLE        hSimTimer;
extern HANDLE        hInterfaceThreadsOk;
extern HANDLE        lbl_mutex;
extern HANDLE        hLoadCellInitialized[MAXSCALES];

// space for file read/write operations
extern _LARGE_INTEGER fbuf_addr;
extern char* file_buf;
extern void* bbram_addr;     // battery backed ram address

extern char trc_buf_desc[MAXTRCBUFFERS][MAX_DBG_DESC];

// Debug
extern UINT             TraceMask;
extern trcbuf           trc_buf       [MAXTRCBUFFERS];     // each thread must have a trace buffer
extern tmptrcbuf        tmp_trc_buf   [MAXTRCBUFFERS];     // for parts to be catinated
extern trace_ctrl       trc           [MAXTRCBUFFERS];

//--------------------------------------------------------
// pShm sentinel diagnostics (set in Main.cpp after init)
//--------------------------------------------------------
extern volatile void* g_sentinel_pShm;
extern volatile int   g_sentinel_magic;

// Write comprehensive diagnostics when pShm invalidation is detected.
// Called once, writes to /tmp/overhead_pshm_diag.log
static inline void dumpPShmDiagnostics(const char* reason)
{
    int fd = open("/tmp/overhead_pshm_diag.log",
                  O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;

    char buf[4096];
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    int len = snprintf(buf, sizeof(buf),
        "\n=== pShm INVALIDATION DETECTED ===\n"
        "Time: %04d-%02d-%02d %02d:%02d:%02d.%03ld\n"
        "Reason: %s\n"
        "Thread: %lu\n"
        "app:              %p\n"
        "app->pShm:        %p\n"
        "g_sentinel_pShm:  %p\n"
        "g_sentinel_magic: 0x%X\n"
        "Parent PID:       %d\n\n",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000000,
        reason,
        (unsigned long)pthread_self(),
        (void*)app,
        app ? (void*)app->pShm : NULL,
        (void*)g_sentinel_pShm,
        (int)g_sentinel_magic,
        (int)getppid());
    write(fd, buf, len);
    write(STDERR_FILENO, buf, len);

    // Dump /proc/self/maps for SharedMemory entries
    len = snprintf(buf, sizeof(buf), "--- /proc/self/maps (shm entries) ---\n");
    write(fd, buf, len);

    int mfd = open("/proc/self/maps", O_RDONLY);
    if (mfd >= 0) {
        char mapbuf[8192];
        int nr;
        bool found_shm = false;
        while ((nr = read(mfd, mapbuf, sizeof(mapbuf) - 1)) > 0) {
            mapbuf[nr] = '\0';
            char* line = mapbuf;
            while (line && *line) {
                char* nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                if (strstr(line, "SharedMemory") || strstr(line, "/dev/shm/")) {
                    int ll = strlen(line);
                    line[ll] = '\n';
                    write(fd, line, ll + 1);
                    line[ll] = '\0';
                    found_shm = true;
                }
                if (nl) { *nl = '\n'; line = nl + 1; }
                else break;
            }
        }
        close(mfd);
        if (!found_shm) {
            len = snprintf(buf, sizeof(buf), "(NO SharedMemory entries found in maps!)\n");
            write(fd, buf, len);
        }
    }

    // Check /dev/shm/SharedMemory file
    struct stat st;
    if (stat("/dev/shm/SharedMemory", &st) == 0) {
        len = snprintf(buf, sizeof(buf),
            "/dev/shm/SharedMemory: size=%ld, inode=%ld\n",
            (long)st.st_size, (long)st.st_ino);
    } else {
        len = snprintf(buf, sizeof(buf),
            "/dev/shm/SharedMemory: NOT FOUND (errno=%d)\n", errno);
    }
    write(fd, buf, len);

    len = snprintf(buf, sizeof(buf), "=== END DIAGNOSTICS ===\n\n");
    write(fd, buf, len);

    fsync(fd);
    close(fd);
}

// Returns true if app->pShm appears safe to dereference.
// Checks sentinel value (pointer corruption) and msync (mapping validity).
static inline bool isPShmValid()
{
    if (app == NULL) return false;
    if (app->pShm == NULL) return false;

    // If sentinels haven't been set yet (early init), allow access
    if (g_sentinel_magic != (int)0xDEADBEEF) return true;

    // Check 1: pShm pointer corruption (class member overflow)
    if ((void*)app->pShm != (void*)g_sentinel_pShm)
    {
        static bool logged = false;
        if (!logged) {
            logged = true;
            RtPrintf("\n!!! CRITICAL: app->pShm POINTER CORRUPTED !!!\n");
            dumpPShmDiagnostics("POINTER CORRUPTED: app->pShm != sentinel");
        }
        return false;
    }

    // Check 2: mmap validity — msync returns ENOMEM if address not mapped
    // Page-align the address for msync
    void* page_addr = (void*)((uintptr_t)app->pShm & ~(uintptr_t)0xFFF);
    if (msync(page_addr, 4096, MS_ASYNC) != 0)
    {
        static bool logged = false;
        if (!logged) {
            logged = true;
            RtPrintf("\n!!! CRITICAL: pShm MAPPING INVALID (msync failed, errno=%d) !!!\n", errno);
            dumpPShmDiagnostics("MAPPING INVALID: msync failed");
        }
        return false;
    }

    return true;
}
