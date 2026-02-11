/*
 * console.cpp - DCH Server Console Window
 *
 * Provides a terminal-based console display that mimics the RTX server
 * window. All RtPrintf calls are routed here with timestamps, color
 * coding, and a status header.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#include "console.h"

// ANSI color codes
#define ANSI_RESET      "\033[0m"
#define ANSI_RED        "\033[1;31m"
#define ANSI_GREEN      "\033[1;32m"
#define ANSI_YELLOW     "\033[1;33m"
#define ANSI_CYAN       "\033[1;36m"
#define ANSI_WHITE      "\033[1;37m"
#define ANSI_DIM        "\033[0;37m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_BLUE       "\033[1;34m"

// Thread safety for console output
static pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;
static int console_initialized = 0;
static FILE* log_file = NULL;

//--------------------------------------------------------
// Get timestamp string
//--------------------------------------------------------
static void GetTimestamp(char* buf, size_t len)
{
    struct timespec ts;
    struct tm* t;

    clock_gettime(CLOCK_REALTIME, &ts);
    t = localtime(&ts.tv_sec);

    snprintf(buf, len, "%02d:%02d:%02d.%03ld",
             t->tm_hour, t->tm_min, t->tm_sec,
             ts.tv_nsec / 1000000);
}

//--------------------------------------------------------
// Detect severity from message content
//--------------------------------------------------------
static enum ConsoleSeverity DetectSeverity(const char* msg)
{
    if (strstr(msg, "Error") || strstr(msg, "error") ||
        strstr(msg, "ERROR") || strstr(msg, "err\n") ||
        strstr(msg, "failed") || strstr(msg, "Failed"))
        return CON_ERROR;

    if (strstr(msg, "Warning") || strstr(msg, "warning") ||
        strstr(msg, "WARNING"))
        return CON_WARNING;

    if (strstr(msg, "started") || strstr(msg, "Started") ||
        strstr(msg, "Connected") || strstr(msg, "listening") ||
        strstr(msg, "Version") || strstr(msg, "version"))
        return CON_STARTUP;

    return CON_INFO;
}

//--------------------------------------------------------
// Get color for severity
//--------------------------------------------------------
static const char* GetColor(enum ConsoleSeverity severity)
{
    switch (severity) {
    case CON_ERROR:   return ANSI_RED;
    case CON_WARNING: return ANSI_YELLOW;
    case CON_STARTUP: return ANSI_GREEN;
    case CON_DEBUG:   return ANSI_DIM;
    case CON_INFO:
    default:          return ANSI_RESET;
    }
}

//--------------------------------------------------------
// ConsoleInit - Initialize the DCH Server console
//--------------------------------------------------------
void ConsoleInit(void)
{
    pthread_mutex_lock(&console_mutex);

    if (!console_initialized) {
        // Clear screen and set title
        printf("\033[2J\033[H");  // Clear screen, home cursor

        // Print banner
        printf(ANSI_BLUE);
        printf("=========================================================\n");
        printf("          D C H   S E R V E R                            \n");
        printf("=========================================================\n");
        printf(ANSI_DIM);
        printf("  Overhead Sizing System - Linux RT Controller\n");

        // Get current time for startup
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        printf("  Started: %04d-%02d-%02d %02d:%02d:%02d\n",
               t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
               t->tm_hour, t->tm_min, t->tm_sec);

        printf(ANSI_BLUE);
        printf("---------------------------------------------------------\n");
        printf(ANSI_RESET);
        fflush(stdout);

        // Open log file
        #ifdef DCHSERVICES_BASE
        log_file = fopen(DCHSERVICES_BASE "/logs/dchserver.log", "a");
#else
        log_file = fopen("/opt/dchservices/logs/dchserver.log", "a");
#endif
        if (log_file) {
            fprintf(log_file, "\n=== DCH Server started %04d-%02d-%02d %02d:%02d:%02d ===\n",
                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
            fflush(log_file);
        }

        console_initialized = 1;
    }

    pthread_mutex_unlock(&console_mutex);
}

//--------------------------------------------------------
// ConsoleShutdown - Clean up the console
//--------------------------------------------------------
void ConsoleShutdown(void)
{
    pthread_mutex_lock(&console_mutex);

    if (console_initialized) {
        printf(ANSI_BLUE);
        printf("---------------------------------------------------------\n");
        printf("  DCH Server shutting down\n");
        printf("=========================================================\n");
        printf(ANSI_RESET);
        fflush(stdout);

        if (log_file) {
            fprintf(log_file, "=== DCH Server shutdown ===\n\n");
            fclose(log_file);
            log_file = NULL;
        }

        console_initialized = 0;
    }

    pthread_mutex_unlock(&console_mutex);
}

//--------------------------------------------------------
// RtPrintf - Drop-in replacement for RTX RtPrintf
//
// This is the main output function. All system messages
// flow through here, just like the RTX server window.
//--------------------------------------------------------
int RtPrintf(const char* fmt, ...)
{
    char msg[2048];
    char timestamp[32];
    va_list args;
    enum ConsoleSeverity severity;
    const char* color;

    // Format the message
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Detect severity from content
    severity = DetectSeverity(msg);
    color = GetColor(severity);

    // Get timestamp
    GetTimestamp(timestamp, sizeof(timestamp));

    // Use trylock to prevent deadlocks - if another thread holds the mutex
    // and is blocked (e.g., in fflush), we skip output rather than deadlock.
    // Use timed wait: try for up to 50ms before giving up.
    struct timespec abs_timeout;
    clock_gettime(CLOCK_REALTIME, &abs_timeout);
    abs_timeout.tv_nsec += 50000000; // 50ms
    if (abs_timeout.tv_nsec >= 1000000000) {
        abs_timeout.tv_sec++;
        abs_timeout.tv_nsec -= 1000000000;
    }

    if (pthread_mutex_timedlock(&console_mutex, &abs_timeout) != 0) {
        // Could not acquire lock within 50ms - write directly to avoid deadlock
        size_t len = strlen(msg);
        write(STDOUT_FILENO, msg, len);
        return (int)len;
    }

    // Print to terminal with timestamp and color
    printf("%s[%s]%s %s%s%s",
           ANSI_DIM, timestamp, ANSI_RESET,
           color, msg, ANSI_RESET);

    size_t len = strlen(msg);

    fflush(stdout);

    // Also write to log file
    if (log_file) {
        fprintf(log_file, "[%s] %s", timestamp, msg);
        fflush(log_file);
    }

    pthread_mutex_unlock(&console_mutex);

    return (int)len;
}

//--------------------------------------------------------
// ConsolePrint - Print with explicit severity
//--------------------------------------------------------
void ConsolePrint(enum ConsoleSeverity severity, const char* fmt, ...)
{
    char msg[2048];
    char timestamp[32];
    va_list args;
    const char* color;

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    color = GetColor(severity);
    GetTimestamp(timestamp, sizeof(timestamp));

    pthread_mutex_lock(&console_mutex);

    printf("%s[%s]%s %s%s%s",
           ANSI_DIM, timestamp, ANSI_RESET,
           color, msg, ANSI_RESET);
    fflush(stdout);

    if (log_file) {
        fprintf(log_file, "[%s] %s", timestamp, msg);
        fflush(log_file);
    }

    pthread_mutex_unlock(&console_mutex);
}

//--------------------------------------------------------
// ConsoleUpdateStatus - Update the status line
//--------------------------------------------------------
void ConsoleUpdateStatus(const char* status)
{
    pthread_mutex_lock(&console_mutex);

    // Save cursor, move to status line, print, restore cursor
    printf("\033[s");        // Save cursor
    printf("\033[1;1H");    // Move to top-left
    printf(ANSI_BLUE "%-57s" ANSI_RESET, status);
    printf("\033[u");        // Restore cursor
    fflush(stdout);

    pthread_mutex_unlock(&console_mutex);
}
