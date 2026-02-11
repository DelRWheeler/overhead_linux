/*
 * interface.h - Linux port of Interface declarations
 *
 * Same as the original but with Linux-compatible types.
 */

#pragma once

// Forward declare types.h content is already included
// This file uses types from platform.h

// Change both of these
#define     VER1                "DCH Server version 2.0.0 started."
#define     VER2                "DCH Server V2.0.0"

#define     RUN_APP             1
#define     CONNECTING_STATE    0
#define     READING_STATE       1
#define     WRITING_STATE       2
#define     INSTANCES           6
#define     MAX_PIPE_MSG_SIZE   MAX_APP_MSG_SIZE
#define     PIPE_ERR_RESTART_NT 1000

// Shared memory for diagnostic use
extern  LPVOID      lpMapAddress;

// Message id for watchdog
#define     IF_SERVICE_CONTROL_ALIVE     200

extern app_type::SHARE_MEMORY *pShm;
extern app_type::TRACE_MEMORY *pTraceMemory;
extern HANDLE  hLog;
extern char*   pEvtStr;

// declarations for tx mailbox
extern HANDLE      hTxShm,  hTxMutex, hTxSemPost, hTxSemAck;
extern PAPPIPCMSG  pTxMsg;

// declarations for host tx mailbox
extern HANDLE      hHstTxShm,  hHstTxMutex, hHstTxSemPost, hHstTxSemAck;
extern PAPPIPCMSG  pHstTxMsg;

// declarations for rx mailbox
extern HANDLE      hRxShm, hRxMutex, hRxSemPost, hRxSemAck;
extern PAPPIPCMSG  pRxMsg;

extern HANDLE   hPipe[INSTANCES];
extern HANDLE   hInterfaceThreadsOk;
extern char     pipe_str[INSTANCES][MAXIISYSNAME];
extern char     evt_str[];

// Pipe compatibility structure (not used with TCP but kept for shared headers)
typedef struct
{
   OVERLAPPED   oOverlap;
   HANDLE       hPipeInst;
   CHAR         chBuf[MAX_PIPE_MSG_SIZE];
   DWORD        cbToWrite;
   DWORD        dwState;
   BOOL         fPendingIO;
} PIPEINST, *LPPIPEINST;

// Function prototypes
void AppStartTcp();
BOOL ConsoleHandler(DWORD CEvent);
void Heartbeats();
void ErrorRoutine(BYTE *sbuf);
void DumpData(BYTE *pbuf, int len);
int  RTFCNDCL TcpConnectionManager();
int  RTFCNDCL TcpServer(PVOID unused);
int  RTFCNDCL SendTcpMbxServer(PVOID unused);
int  RTFCNDCL SendTcpHostMbxServer(PVOID unused);
int  RTFCNDCL UdpTraceThread(PVOID unused);
int  CreateTxMailbox();
int  CreateHstTxMailbox();
int  CreateRxMailbox();
void SetTime(SYSTEMTIME *dt);
void Shutdown(int action);
