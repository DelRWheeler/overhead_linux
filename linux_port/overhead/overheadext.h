#pragma once

#include "types.h"

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
