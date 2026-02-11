/*
 * console.h - DCH Server Console Window
 *
 * Provides a terminal-based console display that mimics the RTX server
 * window, showing all system messages with timestamps and color coding.
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stdio.h>
#include <stdarg.h>

// Console message severity levels
enum ConsoleSeverity {
    CON_INFO = 0,
    CON_WARNING,
    CON_ERROR,
    CON_DEBUG,
    CON_STARTUP
};

// Initialize the DCH Server console
void ConsoleInit(void);

// Shutdown the console
void ConsoleShutdown(void);

// Print a message to the console (replaces RtPrintf)
int RtPrintf(const char* fmt, ...);

// Print with explicit severity
void ConsolePrint(enum ConsoleSeverity severity, const char* fmt, ...);

// Update the status bar
void ConsoleUpdateStatus(const char* status);

#endif // _CONSOLE_H_
