/*
 * platform.h - Linux/POSIX compatibility layer for RTX/Windows APIs
 *
 * This header provides type definitions and wrapper functions that map
 * the RTX RTOS and Windows APIs used by the Overhead system to their
 * Linux/POSIX equivalents. This allows the application code to remain
 * largely unchanged while running on Linux with the PREEMPT_RT kernel.
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <ctype.h>
#include <execinfo.h>
#include <sys/io.h>     // ioperm(), inb(), outb() for direct port I/O on x86

// Forward declaration of console output function
extern int RtPrintf(const char* fmt, ...);

//------------------------------------------------------------------------
// Windows/RTX Type Definitions
//------------------------------------------------------------------------

typedef unsigned char       byte;
typedef unsigned char       BYTE;
typedef unsigned char       UCHAR;
typedef unsigned short      WORD;
typedef unsigned short      USHORT;
typedef unsigned int        UINT;
typedef unsigned int        ULONG;   // Must be 4 bytes to match Windows ABI
typedef unsigned int        DWORD;   // Must be 4 bytes to match Windows ABI
typedef int                 LONG;    // Must be 4 bytes to match Windows ABI
typedef int                 BOOL;
typedef int                 INT;
typedef double              RWORD;
typedef unsigned char       DBOOL;
typedef char                CHAR;
typedef char*               LPSTR;
typedef const char*         LPCSTR;
typedef const char*         LPCTSTR;
typedef void                VOID;
typedef void*               PVOID;
typedef void*               LPVOID;
typedef BYTE*               LPBYTE;
typedef DWORD*              LPDWORD;
typedef LONG*               LPLONG;
typedef char*               PCHAR;
typedef char*               LPTSTR;
typedef UCHAR*              PUCHAR;
typedef USHORT*             PUSHORT;
typedef ULONG*              PULONG;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#define ON    1
#define OFF   0

// Handle type - wraps various POSIX primitives
typedef void* HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define NULL_HANDLE          ((HANDLE)0)

// Socket compatibility
typedef int SOCKET;
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)
#define SOCKET_ERROR_CODE errno
#define SD_BOTH         SHUT_RDWR
#define SOCKADDR        struct sockaddr
#define SOCKADDR_IN     struct sockaddr_in

// WinSock error codes mapped to errno equivalents
#define WSAEWOULDBLOCK  EWOULDBLOCK
#define WSAEINPROGRESS  EINPROGRESS

// Windows constants
#define INFINITE            0xFFFFFFFF
#define WAIT_OBJECT_0       0
#define WAIT_FAILED         0xFFFFFFFF
#define WAIT_TIMEOUT        0x00000102
#define WAIT100MS           100
// ERROR_OCCURED is defined in overheadconst.h
#ifndef ERROR_OCCURED
#define ERROR_OCCURED       (-1)
#endif
#define NO_ERRORS           0
#define NO_ERROR            0
#define PAGE_READWRITE      0
#define SYNCHRONIZE         0
#define MAX_PATH            260

// Windows min/max macros
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// File operations
static inline int DeleteFile(const char* path) {
    return (remove(path) == 0) ? 1 : -1;
}

// Error handling
#define GetLastError()      errno
#define SetLastError(e)     (errno = (e))
#define ERROR_ALREADY_EXISTS EEXIST

// RTX priority defines
#define RT_PRIORITY_MAX     99
#define RT_PRIORITY_MIN     1

// RTX calling convention (no-op on Linux)
#define RTAPI
#define RTFCNDCL
#define RTBASEAPI
#define __stdcall
#define WINAPI
#define _cdecl

// Thread creation flags
#define CREATE_SUSPENDED    0x00000004

// OVERLAPPED structure (stub for compatibility)
typedef struct {
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

// SECURITY_ATTRIBUTES (stub)
typedef struct {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

// LARGE_INTEGER
typedef union {
    struct {
        ULONG LowPart;
        LONG  HighPart;
    };
    long long QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

// SYSTEMTIME (for SetTime compatibility)
typedef struct {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME;

// Windows Service types (stubs - not used on Linux)
typedef void* SC_HANDLE;
typedef struct {
    DWORD dwCurrentState;
} SERVICE_STATUS;
#define SERVICE_STOP_PENDING    0x00000003
#define SERVICE_STOPPED         0x00000001

// Console event types
#define CTRL_C_EVENT        0
#define CTRL_BREAK_EVENT    1
#define CTRL_CLOSE_EVENT    2
#define CTRL_LOGOFF_EVENT   5
#define CTRL_SHUTDOWN_EVENT 6

// Event log types (mapped to syslog)
#define EVENTLOG_INFORMATION_TYPE LOG_INFO
#define EVENTLOG_ERROR_TYPE       LOG_ERR
#define EVENTLOG_SUCCESS          LOG_INFO

//------------------------------------------------------------------------
// Handle Management
//------------------------------------------------------------------------

// Handle types for our wrapper
enum HandleType {
    HANDLE_SEM,         // Named semaphore
    HANDLE_SHM,         // Shared memory
    HANDLE_EVENT,       // Manual-reset event (using eventfd + flag)
    HANDLE_THREAD,      // Thread
    HANDLE_EVENTLOG,    // Event log (syslog)
};

typedef struct {
    enum HandleType type;
    union {
        sem_t*      sem;        // For semaphores and mutexes
        struct {
            int     fd;         // Shared memory fd
            void*   addr;       // Mapped address
            size_t  size;       // Mapped size
            char    name[128];  // Shared memory name
        } shm;
        struct {
            pthread_mutex_t mutex;
            pthread_cond_t  cond;
            int             signaled;
            int             manual_reset;
            int             notify_fd;  // eventfd for poll()-based WaitForMultipleObjects
        } event;
        pthread_t   thread;
    };
} PlatformHandle;

//------------------------------------------------------------------------
// Shared Memory Functions
//------------------------------------------------------------------------

static inline HANDLE RtCreateSharedMemory(DWORD protect, DWORD sizeHigh,
                                           DWORD sizeLow, const char* name,
                                           void** location)
{
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", name);

    // Replace spaces with underscores in name
    for (char* p = shm_name + 1; *p; p++) {
        if (*p == ' ') *p = '_';
    }

    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        if (errno == EEXIST) {
            fd = shm_open(shm_name, O_RDWR, 0666);
            if (fd < 0) return NULL;
            SetLastError(ERROR_ALREADY_EXISTS);
        } else {
            return NULL;
        }
    }

    size_t size = (size_t)sizeLow;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return NULL;
    }

    void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SHM;
    h->shm.fd = fd;
    h->shm.addr = addr;
    h->shm.size = size;
    strncpy(h->shm.name, shm_name, sizeof(h->shm.name) - 1);

    *location = addr;
    return (HANDLE)h;
}

// Default shared memory size for auto-creation (1MB covers SHARE_MEMORY struct)
#define _SHM_DEFAULT_SIZE_ (1024 * 1024)

static inline HANDLE RtOpenSharedMemory(DWORD access, BOOL inherit,
                                         const char* name, void** location)
{
    char shm_name[256];
    snprintf(shm_name, sizeof(shm_name), "/%s", name);

    for (char* p = shm_name + 1; *p; p++) {
        if (*p == ' ') *p = '_';
    }

    // Try to open existing first
    int fd = shm_open(shm_name, O_RDWR, 0666);
    bool created = false;
    if (fd < 0) {
        // Shared memory doesn't exist yet - create it
        fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        if (fd < 0) return NULL;
        created = true;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }

    size_t size = st.st_size;
    if (created || size == 0) {
        size = _SHM_DEFAULT_SIZE_;
        if (ftruncate(fd, size) < 0) {
            close(fd);
            return NULL;
        }
    }

    void* addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    if (created) {
        memset(addr, 0, size);
    }

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SHM;
    h->shm.fd = fd;
    h->shm.addr = addr;
    h->shm.size = size;
    strncpy(h->shm.name, shm_name, sizeof(h->shm.name) - 1);

    *location = addr;
    return (HANDLE)h;
}

//------------------------------------------------------------------------
// Semaphore Functions
//------------------------------------------------------------------------

static inline HANDLE RtCreateSemaphore(void* attr, LONG initial, LONG max,
                                        const char* name)
{
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "/%s", name);

    for (char* p = sem_name + 1; *p; p++) {
        if (*p == ' ') *p = '_';
    }

    sem_t* s = sem_open(sem_name, O_CREAT, 0666, (unsigned int)initial);
    if (s == SEM_FAILED) return NULL;

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SEM;
    h->sem = s;
    return (HANDLE)h;
}

static inline HANDLE RtOpenSemaphore(DWORD access, BOOL inherit, const char* name)
{
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "/%s", name);

    for (char* p = sem_name + 1; *p; p++) {
        if (*p == ' ') *p = '_';
    }

    sem_t* s = sem_open(sem_name, 0);
    if (s == SEM_FAILED) return NULL;

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SEM;
    h->sem = s;
    return (HANDLE)h;
}

static inline BOOL RtReleaseSemaphore(HANDLE handle, LONG count, LPLONG prev)
{
    if (!handle) { RtPrintf("RtReleaseSemaphore: NULL handle\n"); return FALSE; }
    PlatformHandle* h = (PlatformHandle*)handle;
    if (h->type != HANDLE_SEM) { RtPrintf("RtReleaseSemaphore: wrong type %d\n", h->type); return FALSE; }

    for (LONG i = 0; i < count; i++) {
        int val = -1;
        sem_getvalue(h->sem, &val);
        if (sem_post(h->sem) < 0) {
            RtPrintf("RtReleaseSemaphore: sem_post failed errno=%d val=%d\n", errno, val);
            return FALSE;
        }
    }
    return TRUE;
}

//------------------------------------------------------------------------
// Mutex Functions (implemented as binary semaphores)
//------------------------------------------------------------------------

static inline HANDLE RtCreateMutex(void* attr, BOOL initialOwner, const char* name)
{
    if (!name) {
        RtPrintf("ERROR: RtCreateMutex called with NULL name!\n");
        return NULL;
    }
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "/mtx_%s", name);

    for (char* p = sem_name + 1; *p; p++) {
        if (*p == ' ') *p = '_';
    }

    // Mutex starts unlocked (1) unless initially owned
    unsigned int initial = initialOwner ? 0 : 1;
    sem_t* s = sem_open(sem_name, O_CREAT, 0666, initial);
    if (s == SEM_FAILED) return NULL;

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SEM;
    h->sem = s;
    return (HANDLE)h;
}

static inline HANDLE RtOpenMutex(DWORD access, BOOL inherit, const char* name)
{
    if (!name) {
        RtPrintf("ERROR: RtOpenMutex called with NULL name!\n");
        return NULL;
    }
    char sem_name[256];
    snprintf(sem_name, sizeof(sem_name), "/mtx_%s", name);

    for (char* p = sem_name + 1; *p; p++) {
        if (*p == ' ') *p = '_';
    }

    sem_t* s = sem_open(sem_name, 0);
    if (s == SEM_FAILED) return NULL;

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SEM;
    h->sem = s;
    return (HANDLE)h;
}

static inline BOOL RtReleaseMutex(HANDLE handle)
{
    if (!handle) return FALSE;
    PlatformHandle* h = (PlatformHandle*)handle;
    if (h->type != HANDLE_SEM) return FALSE;
    return (sem_post(h->sem) == 0) ? TRUE : FALSE;
}

//------------------------------------------------------------------------
// Event Functions
//------------------------------------------------------------------------

static inline HANDLE RtCreateEvent(void* attr, BOOL manualReset,
                                    BOOL initialState, const char* name)
{
    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_EVENT;

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutex_init(&h->event.mutex, &mattr);
    pthread_mutexattr_destroy(&mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_cond_init(&h->event.cond, &cattr);
    pthread_condattr_destroy(&cattr);

    h->event.signaled = initialState ? 1 : 0;
    h->event.manual_reset = manualReset ? 1 : 0;

    // Create eventfd for poll()-based WaitForMultipleObjects (zero CPU when idle)
    h->event.notify_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    return (HANDLE)h;
}

// CreateEvent alias
#define CreateEvent(a, b, c, d) RtCreateEvent(a, b, c, d)

static inline BOOL RtSetEvent(HANDLE handle)
{
    if (!handle) return FALSE;
    PlatformHandle* h = (PlatformHandle*)handle;
    if (h->type != HANDLE_EVENT) return FALSE;

    pthread_mutex_lock(&h->event.mutex);
    h->event.signaled = 1;
    pthread_cond_broadcast(&h->event.cond);
    pthread_mutex_unlock(&h->event.mutex);

    // Wake up any poll()-based waiters (WaitForMultipleObjects)
    if (h->event.notify_fd >= 0) {
        uint64_t val = 1;
        write(h->event.notify_fd, &val, sizeof(val));
    }

    return TRUE;
}

#define SetEvent(h) RtSetEvent(h)

static inline BOOL RtResetEvent(HANDLE handle)
{
    if (!handle) return FALSE;
    PlatformHandle* h = (PlatformHandle*)handle;
    if (h->type != HANDLE_EVENT) return FALSE;

    pthread_mutex_lock(&h->event.mutex);
    h->event.signaled = 0;
    // Drain eventfd so poll() won't see stale data
    if (h->event.notify_fd >= 0) {
        uint64_t val;
        read(h->event.notify_fd, &val, sizeof(val));
    }
    pthread_mutex_unlock(&h->event.mutex);
    return TRUE;
}

//------------------------------------------------------------------------
// Wait Functions
//------------------------------------------------------------------------

static inline DWORD RtWaitForSingleObject(HANDLE handle, DWORD milliseconds)
{
    if (!handle) return WAIT_FAILED;
    PlatformHandle* h = (PlatformHandle*)handle;

    switch (h->type) {
    case HANDLE_SEM: {
        if (milliseconds == INFINITE) {
            if (sem_wait(h->sem) < 0) return WAIT_FAILED;
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += milliseconds / 1000;
            ts.tv_nsec += (milliseconds % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            if (sem_timedwait(h->sem, &ts) < 0) {
                return (errno == ETIMEDOUT) ? WAIT_TIMEOUT : WAIT_FAILED;
            }
        }
        return WAIT_OBJECT_0;
    }
    case HANDLE_EVENT: {
        pthread_mutex_lock(&h->event.mutex);
        if (milliseconds == INFINITE) {
            while (!h->event.signaled) {
                pthread_cond_wait(&h->event.cond, &h->event.mutex);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += milliseconds / 1000;
            ts.tv_nsec += (milliseconds % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            while (!h->event.signaled) {
                int rc = pthread_cond_timedwait(&h->event.cond, &h->event.mutex, &ts);
                if (rc == ETIMEDOUT) {
                    pthread_mutex_unlock(&h->event.mutex);
                    return WAIT_TIMEOUT;
                }
            }
        }
        if (!h->event.manual_reset) {
            h->event.signaled = 0;
            // Drain eventfd so poll() won't see stale data
            if (h->event.notify_fd >= 0) {
                uint64_t val;
                read(h->event.notify_fd, &val, sizeof(val));
            }
        }
        pthread_mutex_unlock(&h->event.mutex);
        return WAIT_OBJECT_0;
    }
    default:
        return WAIT_FAILED;
    }
}

//------------------------------------------------------------------------
// Handle Cleanup
//------------------------------------------------------------------------

static inline BOOL RtCloseHandle(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return FALSE;
    PlatformHandle* h = (PlatformHandle*)handle;

    switch (h->type) {
    case HANDLE_SEM:
        sem_close(h->sem);
        break;
    case HANDLE_SHM:
        if (h->shm.addr && h->shm.size > 0)
            munmap(h->shm.addr, h->shm.size);
        if (h->shm.fd >= 0)
            close(h->shm.fd);
        break;
    case HANDLE_EVENT:
        if (h->event.notify_fd >= 0) close(h->event.notify_fd);
        pthread_mutex_destroy(&h->event.mutex);
        pthread_cond_destroy(&h->event.cond);
        break;
    default:
        break;
    }

    free(h);
    return TRUE;
}

#define CloseHandle(h) RtCloseHandle(h)

//------------------------------------------------------------------------
// Thread Functions
//------------------------------------------------------------------------

typedef void* (*LPTHREAD_START_ROUTINE)(void*);

typedef struct {
    LPTHREAD_START_ROUTINE func;
    void* arg;
    int priority;
    int suspended;
    pthread_t thread;
    pthread_mutex_t start_mutex;
    pthread_cond_t  start_cond;
    int             started;
} ThreadInfo;

static void* _thread_wrapper(void* arg)
{
    ThreadInfo* ti = (ThreadInfo*)arg;

    // If created suspended, wait for resume
    if (ti->suspended) {
        pthread_mutex_lock(&ti->start_mutex);
        while (!ti->started) {
            pthread_cond_wait(&ti->start_cond, &ti->start_mutex);
        }
        pthread_mutex_unlock(&ti->start_mutex);
    }

    // Set RT priority if configured
    if (ti->priority > 0) {
        struct sched_param param;
        param.sched_priority = ti->priority;
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    }

    ti->func(ti->arg);
    return NULL;
}

static inline HANDLE CreateThread(void* attr, DWORD stackSize,
                                   LPTHREAD_START_ROUTINE func,
                                   void* arg, DWORD flags, DWORD* threadId)
{
    ThreadInfo* ti = (ThreadInfo*)calloc(1, sizeof(ThreadInfo));
    ti->func = func;
    ti->arg = arg;
    ti->suspended = (flags & CREATE_SUSPENDED) ? 1 : 0;
    ti->started = ti->suspended ? 0 : 1;
    pthread_mutex_init(&ti->start_mutex, NULL);
    pthread_cond_init(&ti->start_cond, NULL);

    pthread_attr_t pattr;
    pthread_attr_init(&pattr);
    // Original RTX code used 16KB stacks, but Linux pthreads need more:
    // - RtPrintf uses 2KB stack per call (char msg[2048])
    // - Signal handlers need stack space for ucontext_t (~800 bytes)
    // - Minimum 256KB to be safe for nested function calls
    {
        size_t minStack = 256 * 1024; // 256KB minimum
        size_t useStack = (stackSize > minStack) ? stackSize : minStack;
        pthread_attr_setstacksize(&pattr, useStack);
    }

    if (pthread_create(&ti->thread, &pattr, _thread_wrapper, ti) != 0) {
        pthread_attr_destroy(&pattr);
        free(ti);
        return NULL;
    }

    pthread_attr_destroy(&pattr);

    if (threadId) *threadId = (DWORD)ti->thread;

    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_THREAD;
    h->thread = ti->thread;

    // Store ThreadInfo pointer in a way we can retrieve it
    // Use thread-local or just keep the pointer
    // For simplicity, we'll store ti as the handle itself
    free(h);
    return (HANDLE)ti;
}

static inline DWORD ResumeThread(HANDLE handle)
{
    if (!handle) return (DWORD)-1;
    ThreadInfo* ti = (ThreadInfo*)handle;
    pthread_mutex_lock(&ti->start_mutex);
    ti->started = 1;
    pthread_cond_signal(&ti->start_cond);
    pthread_mutex_unlock(&ti->start_mutex);
    return 0;
}

static inline BOOL RtSetThreadPriority(HANDLE handle, int priority)
{
    if (!handle) return FALSE;
    ThreadInfo* ti = (ThreadInfo*)handle;
    ti->priority = priority;

    // If thread is already running, set priority now
    if (ti->started) {
        struct sched_param param;
        param.sched_priority = priority;
        // Try SCHED_FIFO first (requires RT privileges)
        if (pthread_setschedparam(ti->thread, SCHED_FIFO, &param) == 0)
            return TRUE;
        // Fall back to SCHED_RR
        if (pthread_setschedparam(ti->thread, SCHED_RR, &param) == 0)
            return TRUE;
        // If no RT privileges, just set nice level and continue
        RtPrintf("Warning: RT scheduling not available for thread (need root or RT privileges)\n");
        return TRUE;  // Don't fail - thread still runs, just without RT priority
    }
    return TRUE;
}

static inline BOOL TerminateThread(HANDLE handle, DWORD exitCode)
{
    if (!handle) return FALSE;
    ThreadInfo* ti = (ThreadInfo*)handle;
    pthread_cancel(ti->thread);
    return TRUE;
}

//------------------------------------------------------------------------
// Socket Compatibility
//------------------------------------------------------------------------

static inline int closesocket(SOCKET s)
{
    return close(s);
}

static inline int WSAGetLastError(void)
{
    return errno;
}

// No-ops for WinSock init/cleanup
typedef struct { WORD wVersion; WORD wHighVersion; } WSADATA;

static inline int WSAStartup(WORD version, WSADATA* data)
{
    if (data) {
        data->wVersion = version;
        data->wHighVersion = version;
    }
    return 0;
}

static inline int WSACleanup(void)
{
    return 0;
}

// ioctlsocket replacement
#ifndef FIONBIO
#define FIONBIO 0x5421
#endif

static inline int ioctlsocket(SOCKET s, long cmd, unsigned long* argp)
{
    if (cmd == FIONBIO) {
        int flags = fcntl(s, F_GETFL, 0);
        if (flags < 0) return -1;
        if (*argp)
            flags |= O_NONBLOCK;
        else
            flags &= ~O_NONBLOCK;
        return fcntl(s, F_SETFL, flags);
    }
    return ioctl(s, cmd, argp);
}

// String functions
#define lstrcpy(d, s)   strcpy(d, s)
#define lstrlen(s)      strlen(s)

//------------------------------------------------------------------------
// Sleep
//------------------------------------------------------------------------

static inline void Sleep(DWORD milliseconds)
{
    usleep(milliseconds * 1000);
}

//------------------------------------------------------------------------
// Process Control
//------------------------------------------------------------------------

static inline void ExitProcess(int code)
{
    exit(code);
}

// Spawn replacement - fork and exec
// Child process inherits parent's stdout/stderr so both interface and
// overhead output appear in the same VTE terminal (matching RTX behavior
// where both RTSS processes shared the server console window)
static inline int SpawnProcess(const char* path)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        // Child inherits stdout/stderr from parent (the VTE terminal PTY)
        execl(path, path, (char*)NULL);
        _exit(1);
    }
    return (int)pid;
}

//------------------------------------------------------------------------
// Event Logging (maps to syslog)
//------------------------------------------------------------------------

static inline HANDLE RegisterEventSource(void* server, const char* source)
{
    openlog(source, LOG_PID | LOG_NDELAY, LOG_DAEMON);
    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_EVENTLOG;
    return (HANDLE)h;
}

static inline BOOL ReportEvent(HANDLE hLog, int type, int cat, int id,
                                void* sid, int numStrings, int dataSize,
                                const char** strings, void* data)
{
    if (numStrings > 0 && strings && strings[0]) {
        syslog(type, "%s", strings[0]);
    }
    return TRUE;
}

//------------------------------------------------------------------------
// Signal Handler (replaces SetConsoleCtrlHandler)
//------------------------------------------------------------------------

typedef BOOL (*ConsoleHandlerFunc)(DWORD);
static ConsoleHandlerFunc g_consoleHandler = NULL;

static void _signal_handler(int sig)
{
    if (g_consoleHandler) {
        switch (sig) {
        case SIGINT:
            g_consoleHandler(CTRL_C_EVENT);
            break;
        case SIGTERM:
            g_consoleHandler(CTRL_CLOSE_EVENT);
            break;
        case SIGHUP:
            g_consoleHandler(CTRL_LOGOFF_EVENT);
            break;
        }
    }
}

// Catch-all handler for unexpected signals that would kill the process
static void _unexpected_signal_handler(int sig, siginfo_t* info, void* context)
{
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "\n*** UNEXPECTED SIGNAL: %d at address %p (pid=%d) ***\n",
        sig, info ? info->si_addr : NULL, getpid());
    write(STDERR_FILENO, buf, len);
    fflush(stdout);
    fflush(stderr);
    _exit(128 + sig);
}

static inline BOOL SetConsoleCtrlHandler(ConsoleHandlerFunc handler, BOOL add)
{
    if (add) {
        g_consoleHandler = handler;
        signal(SIGINT, _signal_handler);
        signal(SIGTERM, _signal_handler);
        signal(SIGHUP, _signal_handler);

        // Ignore SIGPIPE - common cause of silent process death on Linux
        signal(SIGPIPE, SIG_IGN);

        // Install catch-all handler for other fatal signals
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = _unexpected_signal_handler;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGALRM, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
        sigaction(SIGXCPU, &sa, NULL);
        sigaction(SIGXFSZ, &sa, NULL);
        sigaction(SIGVTALRM, &sa, NULL);
        sigaction(SIGPROF, &sa, NULL);
    }
    return TRUE;
}

//------------------------------------------------------------------------
// System Time
//------------------------------------------------------------------------

static inline void GetLocalTime(SYSTEMTIME* st)
{
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    st->wYear = t->tm_year + 1900;
    st->wMonth = t->tm_mon + 1;
    st->wDay = t->tm_mday;
    st->wDayOfWeek = t->tm_wday;
    st->wHour = t->tm_hour;
    st->wMinute = t->tm_min;
    st->wSecond = t->tm_sec;
    st->wMilliseconds = 0;
}

static inline BOOL SetLocalTime(SYSTEMTIME* st)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = st->wYear - 1900;
    t.tm_mon = st->wMonth - 1;
    t.tm_mday = st->wDay;
    t.tm_hour = st->wHour;
    t.tm_min = st->wMinute;
    t.tm_sec = st->wSecond;

    time_t newtime = mktime(&t);
    struct timespec ts;
    ts.tv_sec = newtime;
    ts.tv_nsec = st->wMilliseconds * 1000000;
    return (clock_settime(CLOCK_REALTIME, &ts) == 0) ? TRUE : FALSE;
}

//------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------

// __int64 compatibility
#define __int64 long long

// Shutdown flags (used in the app but implemented differently on Linux)
#define EWX_REBOOT    0
#define EWX_SHUTDOWN  1
#define EWX_FORCE     0

static inline BOOL ExitWindowsEx(UINT flags, DWORD reason)
{
    if (flags == EWX_REBOOT) {
        RtPrintf("Service restart requested (was system reboot on XP)\n");
    } else {
        RtPrintf("Service shutdown requested (was system shutdown on XP)\n");
    }
    // On XP this rebooted the PC; on Linux we just exit and let systemd restart us.
    _exit(1);
    return TRUE;
}

// Service control (stubs - not used on Linux, we use systemd)
#define IF_SERVICE_CONTROL_ALIVE 200

// Pipe-related constants (kept for compatibility, not used with TCP)
#define PIPE_READMODE_MESSAGE 0

// RPC exception handling (no-op on Linux)
#define RpcTryExcept     {
#define RpcExcept(x)     } if(0) {
#define RpcEndExcept     }
#define RpcExceptionCode() 0

// Token/Privilege (no-op on Linux - we use sudo/capabilities)
typedef void* TOKEN_PRIVILEGES;
typedef void* LUID;
#define SE_SHUTDOWN_NAME    "shutdown"
#define SE_PRIVILEGE_ENABLED 0
#define TOKEN_ADJUST_PRIVILEGES 0
#define TOKEN_QUERY 0

static inline BOOL OpenProcessToken(void* proc, DWORD access, void* token) { return TRUE; }
static inline BOOL LookupPrivilegeValue(void* sys, const char* name, void* luid) { return TRUE; }
static inline BOOL AdjustTokenPrivileges(void* token, BOOL disable, void* tp,
                                          DWORD len, void* prev, void* retLen) { return TRUE; }
static inline void* GetCurrentProcess(void) { return NULL; }

// PHANDLER_ROUTINE compatibility
typedef ConsoleHandlerFunc PHANDLER_ROUTINE;

// WaitForMultipleObjects for events (simplified)
// RtWaitForMultipleObjects is the RTX variant - same signature
#define RtWaitForMultipleObjects WaitForMultipleObjects

static inline DWORD WaitForMultipleObjects(DWORD count, HANDLE* handles,
                                            BOOL waitAll, DWORD ms)
{
    // First check if any event is already signaled (fast path)
    for (DWORD i = 0; i < count; i++) {
        PlatformHandle* h = (PlatformHandle*)handles[i];
        if (h && h->type == HANDLE_EVENT) {
            pthread_mutex_lock(&h->event.mutex);
            int sig = h->event.signaled;
            if (sig && !h->event.manual_reset) {
                // Auto-reset: clear signaled state on consume (matches Windows behavior)
                h->event.signaled = 0;
                // Drain the eventfd so poll() won't see stale data
                if (h->event.notify_fd >= 0) {
                    uint64_t val;
                    read(h->event.notify_fd, &val, sizeof(val));
                }
            }
            pthread_mutex_unlock(&h->event.mutex);
            if (sig) return WAIT_OBJECT_0 + i;
        }
    }

    // Build pollfd array from event notify_fds
    struct pollfd fds[count];
    for (DWORD i = 0; i < count; i++) {
        PlatformHandle* h = (PlatformHandle*)handles[i];
        if (h && h->type == HANDLE_EVENT && h->event.notify_fd >= 0) {
            fds[i].fd = h->event.notify_fd;
            fds[i].events = POLLIN;
        } else {
            fds[i].fd = -1;  // poll() ignores negative fds
            fds[i].events = 0;
        }
        fds[i].revents = 0;
    }

    // Block in poll() — zero CPU while waiting, proper timeout
    int timeout_ms = (ms == INFINITE) ? -1 : (int)ms;
    int ret = poll(fds, count, timeout_ms);

    if (ret == 0) return WAIT_TIMEOUT;
    if (ret < 0) return (errno == EINTR) ? WAIT_TIMEOUT : WAIT_FAILED;

    // Check which event was signaled
    for (DWORD i = 0; i < count; i++) {
        if (fds[i].revents & POLLIN) {
            // Drain the eventfd
            uint64_t val;
            read(fds[i].fd, &val, sizeof(val));

            PlatformHandle* h = (PlatformHandle*)handles[i];
            pthread_mutex_lock(&h->event.mutex);
            int sig = h->event.signaled;
            if (sig && !h->event.manual_reset) {
                // Auto-reset: clear signaled state on consume
                h->event.signaled = 0;
            }
            pthread_mutex_unlock(&h->event.mutex);
            if (sig) return WAIT_OBJECT_0 + i;
        }
    }

    return WAIT_TIMEOUT;
}

//------------------------------------------------------------------------
// RTX Timer Functions (POSIX timer_create/timer_settime)
//------------------------------------------------------------------------

#define CLOCK_FASTEST 0  // RTX HAL Timer → POSIX CLOCK_MONOTONIC

// Timer handle using a dedicated thread with nanosleep loop
// (More reliable than SIGEV_THREAD which can silently fail)
typedef struct {
    timer_t         timerid;     // unused but kept for structure compat
    pthread_t       thread;
    void            (*callback)(PVOID);
    PVOID           context;
    int             priority;
    volatile int    active;
    struct itimerspec interval;
} TimerHandle;

static void _timer_crash_handler(int sig, siginfo_t* info, void* ctx)
{
    // Timer callback crashed - log and continue
    // (don't let one bad callback kill the timer)
    RtPrintf("TIMER CRASH: signal %d at addr %p\n", sig,
             info ? info->si_addr : NULL);
}

static void* _timer_thread_func(void* arg)
{
    TimerHandle* th = (TimerHandle*)arg;

    // Set real-time scheduling to prevent CPU starvation from other threads.
    // GP_TIMER heartbeat must never be starved or CheckHeartbeats kills the process.
    {
        struct sched_param sp;
        sp.sched_priority = (th->priority > 0) ? th->priority : 10;
        if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0) {
            // Fallback: at least try to raise niceness
            // (requires CAP_SYS_NICE or AmbientCapabilities in systemd service)
        }
    }

    // Wait for initial expiration
    struct timespec delay;
    delay.tv_sec = th->interval.it_value.tv_sec;
    delay.tv_nsec = th->interval.it_value.tv_nsec;
    nanosleep(&delay, NULL);

    // Periodic loop
    struct timespec period;
    period.tv_sec = th->interval.it_interval.tv_sec;
    period.tv_nsec = th->interval.it_interval.tv_nsec;

    // Disable thread cancellation - timer threads must not be killed by pthread_cancel
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    while (th->active) {
        if (th->callback) {
            th->callback(th->context);
        }
        if (!th->active) break;
        nanosleep(&period, NULL);
    }

    // Timer thread exiting — log this (should never happen during normal operation)
    {
        int tfd = open("/tmp/overhead_appflags.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (tfd >= 0) {
            char tbuf[256];
            struct timespec tts;
            clock_gettime(CLOCK_REALTIME, &tts);
            struct tm ttm;
            localtime_r(&tts.tv_sec, &ttm);
            int tlen = snprintf(tbuf, sizeof(tbuf),
                "[%02d:%02d:%02d.%03ld] PID=%d TIMER THREAD EXITING: active=%d callback=%p\n---\n",
                ttm.tm_hour, ttm.tm_min, ttm.tm_sec, tts.tv_nsec / 1000000,
                getpid(), th->active, (void*)th->callback);
            write(tfd, tbuf, tlen);
            fsync(tfd);
            close(tfd);
        }
    }

    return NULL;
}

static inline HANDLE RtCreateTimer(void* security, DWORD stackSize,
                                    void (*handler)(PVOID), PVOID context,
                                    int priority, int clock)
{
    TimerHandle* th = (TimerHandle*)calloc(1, sizeof(TimerHandle));
    if (!th) return NULL;

    th->callback = handler;
    th->context = context;
    th->priority = priority;
    th->active = 0;
    // Thread will be created when RtSetTimerRelative is called
    return (HANDLE)th;
}

// RTX timer intervals are in 100ns units
static inline BOOL RtSetTimerRelative(HANDLE handle, LARGE_INTEGER* expiration,
                                       LARGE_INTEGER* interval)
{
    if (!handle) return FALSE;
    TimerHandle* th = (TimerHandle*)handle;

    // Convert 100ns units to seconds/nanoseconds
    long long exp_ns = expiration->QuadPart * 100;  // 100ns → ns
    long long int_ns = interval->QuadPart * 100;

    struct itimerspec its;
    its.it_value.tv_sec = exp_ns / 1000000000LL;
    its.it_value.tv_nsec = exp_ns % 1000000000LL;
    its.it_interval.tv_sec = int_ns / 1000000000LL;
    its.it_interval.tv_nsec = int_ns % 1000000000LL;

    th->active = 1;
    th->interval = its;

    // Start dedicated timer thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&th->thread, &attr, _timer_thread_func, th) != 0) {
        th->active = 0;
        pthread_attr_destroy(&attr);
        return FALSE;
    }
    pthread_attr_destroy(&attr);

    return TRUE;
}

static inline BOOL RtCancelTimer(HANDLE handle, void* unused)
{
    if (!handle) return FALSE;
    TimerHandle* th = (TimerHandle*)handle;
    th->active = 0;
    // Thread will exit on next loop iteration after seeing active=0
    // Don't free - thread may still be running briefly
    return TRUE;
}

//------------------------------------------------------------------------
// RTX Shutdown Handler
//------------------------------------------------------------------------

#define RT_SHUTDOWN_NT_SYSTEM_SHUTDOWN  1
#define RT_SHUTDOWN_NT_STOP            2

typedef void (*ShutdownHandlerFunc)(PVOID, LONG);
static ShutdownHandlerFunc g_shutdownHandler = NULL;
static PVOID g_shutdownContext = NULL;

static void _shutdown_signal_handler(int sig)
{
    if (g_shutdownHandler) {
        LONG reason = (sig == SIGTERM) ? RT_SHUTDOWN_NT_SYSTEM_SHUTDOWN : RT_SHUTDOWN_NT_STOP;
        g_shutdownHandler(g_shutdownContext, reason);
    }
}

static inline HANDLE RtAttachShutdownHandler(void* security, DWORD stackSize,
                                              ShutdownHandlerFunc handler,
                                              PVOID context, int priority)
{
    g_shutdownHandler = handler;
    g_shutdownContext = context;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _shutdown_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Ignore SIGPIPE - prevents silent process death from broken pipes/sockets
    signal(SIGPIPE, SIG_IGN);

    // Install catch-all for other unexpected fatal signals
    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_sigaction = _unexpected_signal_handler;
    sa2.sa_flags = SA_SIGINFO;
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGALRM, &sa2, NULL);
    sigaction(SIGUSR1, &sa2, NULL);
    sigaction(SIGUSR2, &sa2, NULL);
    sigaction(SIGXCPU, &sa2, NULL);
    sigaction(SIGXFSZ, &sa2, NULL);
    sigaction(SIGVTALRM, &sa2, NULL);
    sigaction(SIGPROF, &sa2, NULL);

    // Return a non-NULL handle to indicate success
    static int shutdown_handle_marker = 1;
    return (HANDLE)&shutdown_handle_marker;
}

//------------------------------------------------------------------------
// Port I/O Functions
//------------------------------------------------------------------------

static inline BOOL RtEnablePortIo(PUCHAR addr, ULONG bytes)
{
    // Real hardware: use iopl(3) for full port access.
    // ioperm() doesn't survive execvp(), but iopl() does.
    // Only need to call once - subsequent calls are harmless.
    (void)addr;
    (void)bytes;
    static int iopl_done = 0;
    if (!iopl_done) {
        if (iopl(3) != 0) {
            RtPrintf("ERROR: iopl(3) failed: %s (need root?)\n", strerror(errno));
            return FALSE;
        }
        iopl_done = 1;
        RtPrintf("Port I/O access enabled (iopl)\n");
    }
    return TRUE;
}

static inline void RtWritePortUchar(PUCHAR addr, UCHAR value)
{
    outb(value, (unsigned short)(uintptr_t)addr);
}

static inline UCHAR RtReadPortUchar(PUCHAR addr)
{
    return inb((unsigned short)(uintptr_t)addr);
}

//------------------------------------------------------------------------
// Memory Allocation Functions
//------------------------------------------------------------------------

static inline void* RtAllocateContiguousMemory(ULONG size, LARGE_INTEGER maxAddr)
{
    // On Linux, just use malloc - contiguous memory isn't special in userspace
    void* p = malloc(size);
    if (p) memset(p, 0, size);
    return p;
}

static inline void* RtAllocateLockedMemory(ULONG size)
{
    void* p = malloc(size);
    if (p) {
        memset(p, 0, size);
        mlock(p, size);  // Lock pages into RAM
    }
    return p;
}

static inline void RtFreeLockedMemory(void* ptr)
{
    if (ptr) free(ptr);
}

static inline void* RtMapMemory(LARGE_INTEGER base, ULONG size, int flags)
{
    // Physical memory mapping - stub on ARM (no battery-backed SRAM)
    (void)base;
    (void)size;
    (void)flags;
    return NULL;
}

static inline void RtUnmapMemory(void* addr)
{
    // Stub
    (void)addr;
}

//------------------------------------------------------------------------
// File I/O Functions (Windows CreateFile/ReadFile/WriteFile → POSIX)
//------------------------------------------------------------------------

// File creation disposition flags
#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5

// Access flags
#define GENERIC_READ        0x80000000
#define GENERIC_WRITE       0x40000000

// File attribute
#define FILE_ATTRIBUTE_NORMAL 0

// Seek methods
#define FILE_BEGIN   SEEK_SET
#define FILE_END     SEEK_END

// LEN_UNKNOWN defined in overheadconst.h (value 99) for Read_File method parameter

static inline HANDLE CreateFile(const char* filename, DWORD access,
                                 DWORD shareMode, void* security,
                                 DWORD disposition, DWORD flags, void* tmpl)
{
    int oflags = 0;

    // Access mode
    if ((access & GENERIC_READ) && (access & GENERIC_WRITE))
        oflags = O_RDWR;
    else if (access & GENERIC_WRITE)
        oflags = O_WRONLY;
    else
        oflags = O_RDONLY;

    // Disposition
    switch (disposition) {
    case CREATE_NEW:
        oflags |= O_CREAT | O_EXCL;
        break;
    case CREATE_ALWAYS:
        oflags |= O_CREAT | O_TRUNC;
        break;
    case OPEN_EXISTING:
        // No extra flags
        break;
    case OPEN_ALWAYS:
        oflags |= O_CREAT;
        break;
    case TRUNCATE_EXISTING:
        oflags |= O_TRUNC;
        break;
    }

    int fd = open(filename, oflags, 0666);
    if (fd < 0) return INVALID_HANDLE_VALUE;

    // Store fd in a PlatformHandle (reuse SHM type for fd storage)
    PlatformHandle* h = (PlatformHandle*)calloc(1, sizeof(PlatformHandle));
    h->type = HANDLE_SHM;  // Reuse for file handles
    h->shm.fd = fd;
    h->shm.addr = NULL;
    h->shm.size = 0;
    return (HANDLE)h;
}

static inline BOOL ReadFile(HANDLE handle, void* buffer, DWORD bytesToRead,
                             DWORD* bytesRead, void* overlapped)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return FALSE;
    PlatformHandle* h = (PlatformHandle*)handle;
    ssize_t n = read(h->shm.fd, buffer, bytesToRead);
    if (n < 0) {
        if (bytesRead) *bytesRead = 0;
        return FALSE;
    }
    if (bytesRead) *bytesRead = (DWORD)n;
    return TRUE;
}

static inline BOOL WriteFile(HANDLE handle, const void* buffer, DWORD bytesToWrite,
                              DWORD* bytesWritten, void* overlapped)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return FALSE;
    PlatformHandle* h = (PlatformHandle*)handle;
    ssize_t n = write(h->shm.fd, buffer, bytesToWrite);
    if (n < 0) {
        if (bytesWritten) *bytesWritten = 0;
        return FALSE;
    }
    if (bytesWritten) *bytesWritten = (DWORD)n;
    return TRUE;
}

static inline DWORD SetFilePointer(HANDLE handle, LONG distLow,
                                    LONG* distHigh, DWORD method)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return (DWORD)-1;
    PlatformHandle* h = (PlatformHandle*)handle;
    off_t offset = distLow;
    off_t result = lseek(h->shm.fd, offset, method);
    return (result < 0) ? (DWORD)-1 : (DWORD)result;
}

//------------------------------------------------------------------------
// Exception Handling (structured exception handling → signal handlers)
//------------------------------------------------------------------------

typedef struct {
    DWORD ExceptionCode;
    DWORD ExceptionFlags;
    void* ExceptionAddress;
} EXCEPTION_RECORD;

typedef struct {
    EXCEPTION_RECORD* ExceptionRecord;
} EXCEPTION_POINTERS;

#define EXCEPTION_CONTINUE_SEARCH 0

typedef LONG (*LPTOP_LEVEL_EXCEPTION_FILTER)(EXCEPTION_POINTERS*);
static LPTOP_LEVEL_EXCEPTION_FILTER g_exceptionFilter = NULL;

static void _segfault_handler(int sig, siginfo_t* info, void* context)
{
    const char marker[] = "\n!!! SEGFAULT HANDLER ENTERED !!!\n";
    write(STDOUT_FILENO, marker, sizeof(marker) - 1);
    write(STDERR_FILENO, marker, sizeof(marker) - 1);

    // Use write() which is async-signal-safe to ensure crash is visible
    const char* sig_name = "UNKNOWN";
    switch(sig) {
        case SIGSEGV: sig_name = "SIGSEGV"; break;
        case SIGBUS:  sig_name = "SIGBUS";  break;
        case SIGFPE:  sig_name = "SIGFPE";  break;
        case SIGABRT: sig_name = "SIGABRT"; break;
        case SIGTRAP: sig_name = "SIGTRAP"; break;
    }
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "\n*** CRASH: Signal %s (%d) at address %p ***\n",
        sig_name, sig, info ? info->si_addr : NULL);
    write(STDOUT_FILENO, buf, len);
    write(STDERR_FILENO, buf, len);

    // Also write crash info + backtrace to a dedicated crash file
    int crashfd = open("/tmp/overhead_crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (crashfd >= 0) {
        write(crashfd, buf, len);
    }
    // Write backtrace to all outputs
    void* bt[32];
    int bt_size = backtrace(bt, 32);
    backtrace_symbols_fd(bt, bt_size, STDOUT_FILENO);
    backtrace_symbols_fd(bt, bt_size, STDERR_FILENO);
    if (crashfd >= 0) {
        backtrace_symbols_fd(bt, bt_size, crashfd);
        close(crashfd);
    }

    if (g_exceptionFilter) {
        static EXCEPTION_RECORD rec;
        static EXCEPTION_POINTERS ptrs;
        rec.ExceptionCode = (DWORD)sig;
        rec.ExceptionFlags = 0;
        rec.ExceptionAddress = info ? info->si_addr : NULL;
        ptrs.ExceptionRecord = &rec;
        g_exceptionFilter(&ptrs);
    }

    // Flush stdio buffers so RtPrintf output from exception filter is visible
    fflush(stdout);
    fflush(stderr);
    _exit(1);
}

static inline LPTOP_LEVEL_EXCEPTION_FILTER
SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter)
{
    LPTOP_LEVEL_EXCEPTION_FILTER prev = g_exceptionFilter;
    g_exceptionFilter = filter;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = _segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);

    return prev;
}

//------------------------------------------------------------------------
// Process Functions
//------------------------------------------------------------------------

static inline void RtExitProcess(int code)
{
    exit(code);
}

//------------------------------------------------------------------------
// RTX Event - Open existing named event
//------------------------------------------------------------------------

static inline HANDLE RtOpenEvent(void* attr, BOOL inherit, const char* name)
{
    // Events are process-local in our implementation
    // The Interface creates the event, Overhead opens it
    // Both sides synchronize through shared memory (app_threads flag)
    // Must be manual-reset to allow multiple waiting threads to proceed
    return RtCreateEvent(attr, TRUE, 0, name);
}

//------------------------------------------------------------------------
// Shared Memory Access Flags
//------------------------------------------------------------------------

#ifndef SHM_MAP_WRITE
#define SHM_MAP_WRITE 0x0002
#endif

#ifndef SHM_MAP_READ
#define SHM_MAP_READ  0x0004
#endif

//------------------------------------------------------------------------
// RTX Interrupt Functions (stubs - not used on Linux, kernel handles IRQs)
//------------------------------------------------------------------------

// Bus types used by RtAttachInterruptVector
#define Isa   0
#define Pci   1

// RtAttachInterruptVector - matches RTX 9-parameter signature
static inline HANDLE RtAttachInterruptVector(void* attr, DWORD stackSize,
                                              void* isrFunc, void* context,
                                              DWORD priority, DWORD busType,
                                              DWORD busNumber, DWORD irq,
                                              DWORD vector)
{
    // Stub - serial ports on Linux use kernel drivers via /dev/ttyS*
    (void)attr; (void)stackSize; (void)isrFunc; (void)context;
    (void)priority; (void)busType; (void)busNumber; (void)irq; (void)vector;
    return NULL;
}

static inline void RtReleaseInterruptVector(HANDLE handle)
{
    // Stub
    (void)handle;
}

// Port I/O disable (counterpart to RtEnablePortIo)
static inline BOOL RtDisablePortIo(PUCHAR addr, ULONG bytes)
{
    // Stub - no direct port I/O on Linux/ARM
    (void)addr; (void)bytes;
    return TRUE;
}

// Clock types for RTX timer API (used in serial timer code)
#define CLOCK_1  1  // System clock (millisecond)
#define CLOCK_2  2  // HAL real-time clock (microsecond)

//------------------------------------------------------------------------
// LARGE_INTEGER alias for _LARGE_INTEGER
//------------------------------------------------------------------------

typedef LARGE_INTEGER _LARGE_INTEGER;

//------------------------------------------------------------------------
// Additional Wait/Timing Defines
//------------------------------------------------------------------------

#ifndef WAIT200MS
#define WAIT200MS 200
#endif

#ifndef WAIT50MS
#define WAIT50MS  50
#endif

//------------------------------------------------------------------------
// Windows PROCESS_INFORMATION (stub)
//------------------------------------------------------------------------

typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION;

//------------------------------------------------------------------------
// Additional compatibility defines
//------------------------------------------------------------------------

// _spawnl compatibility
#define _P_NOWAIT 1
static inline int _spawnl(int mode, const char* path, ...) {
    return SpawnProcess(path);
}

#endif // _PLATFORM_H_
