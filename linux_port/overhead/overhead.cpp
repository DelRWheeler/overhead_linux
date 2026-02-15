//////////////////////////////////////////////////////////////////////
// overhead.cpp: implementation of the overhead class.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "telnetsrv.h"
#ifdef _PCM3724_SIM_
#include "pcm3724_sim.h"
#endif

#undef  _FILE_
#define _FILE_      "overhead.cpp"

//////////////////////////////////////////////////////////////////////
// Global object pointers/handles
//////////////////////////////////////////////////////////////////////

overhead*       app       = NULL;
io*             iop       = NULL;
io*             iop_2     = NULL;
io*				iop_3	  = NULL;
io*				iop_4	  = NULL;  // TAG 12-15-24
io*             ios       = NULL;

HBMLoadCell::HBMinit*	pHBMLoadCellInit1 = NULL;
HBMLoadCell::HBMinit*	pHBMLoadCellInit2 = NULL;
HBMLoadCell::HBMinit*	pHBMLoadCellInit3 = NULL;
HBMLoadCell::HBMinit*	pHBMLoadCellInit4 = NULL;
HANDLE                  hLoadCellInitialized[MAXLOADCELLS];

InterSystems*   Isys      = NULL;
DropManager*    drpman    = NULL;
Simulator*      sim       = NULL;

HANDLE          hShmem    = NULL;
HANDLE          hTraceMem = NULL;
HANDLE			hTelnetTermSMem = NULL;
HANDLE			hTerminalThread = NULL;
HANDLE          hDebug    = NULL;
HANDLE          hGpSend   = NULL;
HANDLE          hAppTimer = NULL;
HANDLE          hGpTimer  = NULL;
HANDLE          hDrpMgr   = NULL;
HANDLE          hIsys     = NULL;
HANDLE          hIsysMbx  = NULL;
HANDLE          hIsysRx   = NULL;
HANDLE          hAppMbx   = NULL;
HANDLE          hAppRx    = NULL;
HANDLE          hAppRxEv  = NULL;
HANDLE          hEvtHndlr = NULL;
HANDLE          hSimTimer = NULL;
HANDLE          hFstTimer = NULL;
HANDLE          hShutdown = NULL;
HANDLE          hInterfaceThreadsOk = NULL;

ser_typ::UCB   ucbList[COM_MAX_PORTS] = {

#ifdef _SIMULATION_MODE_
	{
	COM1,                      //COM port
	(PUCHAR)COM1_BADDR,		   //base address
	Serial::COM1Isr,           // isr handle
	0,                         //interrupt vector address
	COM1_IRQ,                  //IRQ
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	0						   //RS232
	},
	{
	COM2,                      //COM port
	(PUCHAR)COM2_BADDR,		   //base address
	Serial::COM2Isr,           // isr handle
	0,                         //interrupt vector address
	COM2_IRQ,                  //IRQ
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	0						   //RS232
	},
	{                          //COM3 UCB RS485
	COM3,                      //COM port
	(PUCHAR)COM3_BADDR,		   //base address
	Serial::COM3Isr,           // isr handle
	0,						   //interrupt vector address
	COM3_IRQ,                  //IRQ level
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	1						   //1 - RS422 option
	},
	{
	COM4,                      //COM port
	(PUCHAR)COM4_BADDR,        //base address
	Serial::COM4Isr,           // isr handle
	0,                         //interrupt vector address
	COM4_IRQ,                  //IRQ
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	1						   //RS422
	}
#else
	{                          //COM3 UCB RS485
	COM3,                      //COM port
	(PUCHAR)COM3_BADDR,		   //base address
	Serial::COM3Isr,           // isr handle
	0,						   //interrupt vector address
	COM3_IRQ,                  //IRQ level
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	1						   //1 - RS422 option
	},
	{
	COM4,                      //COM port
	(PUCHAR)COM4_BADDR,        //base address
	Serial::COM4Isr,           // isr handle
	0,                         //interrupt vector address
	COM4_IRQ,                  //IRQ
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	1						   //RS422
	},
	{
	COM1,                      //COM port
	(PUCHAR)COM1_BADDR,		   //base address
	Serial::COM1Isr,           // isr handle
	0,                         //interrupt vector address
	COM1_IRQ,                  //IRQ
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	0						   //RS232
	},
	{
	COM2,                      //COM port
	(PUCHAR)COM2_BADDR,		   //base address
	Serial::COM2Isr,           // isr handle
	0,                         //interrupt vector address
	COM2_IRQ,                  //IRQ
	NULL,                      //pointer to input ring buff
	NULL,                      //pointer to output ring buff
	DEFAULT_FIFO_MASK,         //default FIFO mask
	DEFAULT_FIFO_SIZE,         //default FIFO size in bytes
	DEFAULT_PARITY,            //default - no parity
	DEFAULT_STOP_BITS,         //default stop bits
	0,                         //receiver ISR inactive
	0,                         //transmitter ISR inactive
	0,                         //time-delimited receive
	0,                         //stats flag
	0,                         //spare bits
	COM_STATE_IDLE,            //UART is not yet initialized
	DEFAULT_WORDSIZE,          //default xmit wordsize in bits
	DEFAULT_BAUD_RATE,         //Default baud rate
	COM_FLOW_CONTROL_NONE,
	0,                         //receiver timer
	0,                         //last error
	0,                         //error count
	0						   //RS232
	}
#endif
};

Serial*  pser1 = NULL;//;
Serial*  pser2 = NULL;//;
Serial*  pser3 = NULL;//;
Serial*  pser4 = NULL;//;

TELNETD atdTelnetd[MAXTELNETD];
extern int __stdcall TerminalThread(PVOID unused);

//////////////////////////////////////////////////////////////////////
// Local object handles/misc.
//////////////////////////////////////////////////////////////////////

// space for file read/write operations
char*           file_buf;
_LARGE_INTEGER  fbuf_addr;

// Useful mask array for turning bit numbers into a mask
const BYTE      Mask[8] = {0x01,0x2,0x4,0x8,0x10,0x20,0x40,0x80};

// Handles and variables for mailboxes
HANDLE          hAppShm,       hAppMutex,      hAppSemPost,      hAppSemAck;
HANDLE          hDrpMgrShm,    hDrpMgrMutex,   hDrpMgrSemPost,   hDrpMgrSemAck;
HANDLE          hIsysShm,      hIsysMutex,     hIsysSemPost,     hIsysSemAck;

PIPCMSG         pDrpMgrIpcMsg, pIsysIpcMsg;
PAPPIPCMSG      pAppIpcMsg;                          // extra large buffer for app messages

const LONG      lAppInitialCount = 0;
const LONG      lAppMaximumCount = 1;
const LONG      lAppReleaseCount = 1;

char            app_dt_str      [MAXDATETIMESTR];    // place to build a date/time string
UINT            TraceMask;
trcbuf          trc_buf         [MAXTRCBUFFERS];     // each thread must have a trace buffer
tmptrcbuf       tmp_trc_buf     [MAXTRCBUFFERS];     // for parts to be catinated
char            app_err_buf     [MAXERRMBUFSIZE];
trace_ctrl      trc             [MAXTRCBUFFERS];
HANDLE          process_event   [NUM_EVT];           // handles for events to process
HANDLE          lbl_mutex;                           // mutex for writing to batch_label table
HANDLE          load_cell_mutex [MAXLCMUTEX];        // mutex for writing to load cell card
HANDLE          load_cell_mutex2 [MAXLCMUTEX];                 // mutex for writing to load cell card

// for processing drop records
app_drp_rec     drp_rec_send_buf[MAXDROPRECS];       // small buffer for sending drop records
app_drp_rec     sav_rec_buf     [MAXSAVERECS+MAXDROPRECS]; // buffer for saving/reading drop records from file
void*           bbram_addr;     // battery backed ram address

// Response timing check blocking time on messages
#ifdef _HOST_TIMING_TEST_
    static LARGE_INTEGER msg_StartTime, msg_EndTime;
    UINT   msg_time;
#endif

// Response timing check for drop records
#ifdef _DREC_TIMING_TEST_
    static LARGE_INTEGER drec_StartTime, drec_EndTime;
    static UINT drec_seq_done = 0;
#endif

// Response timing check for print label, drop records that contain last in batch
#ifdef _LREC_TIMING_TEST_
    static LARGE_INTEGER ldrec_StartTime, ldrec_EndTime;
    static UINT ldrec_seq_done = 0;
#endif

#ifdef _SHMW_TIMING_TEST_
    #define SHMW_TMO    3
    static LARGE_INTEGER shmw_StartTime, shmw_EndTime;
    static UINT shmw_seq_done = 0;
    static int  shmw_seq_tmo  = SHMW_TMO;
#endif

// !!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!!!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!

// These descriptions need to change if any commands are added.

// For debug use with the network messages,
// translates the command to description
char isys_msg_desc[10][MAX_DBG_DESC] =
{   "Bad0",
    "Ack",
    "Com Chk",
    "Maint Cmd",
    "Drop Msg",
    "Bad1",
    "Bad2",
    "Bad3",
    "Bad4",
    "Bad5"
};

// For debug use with the drop manager messages,
// translates the command to description
char dmgr_msg[10][MAX_DBG_DESC] =
{   "Bad0",
    "Drp Maint",
    "Maint Resp",
    "Maint Fail",
    "ChkDrpLoc",
    "ChkDrpRem",
    "Bad1",
    "Bad2",
    "Bad3",
    "Bad4"
};

// For debug use with the drop manager messages,
// translates the command to description
char app_msg[26][MAX_DBG_DESC] =
{   "Bad0",
    "shm read",
    "shm write",
    "drp resp",
    "blk req",
    "blk resp",
    "err msg",
    "dbg msg",
    "drp msg",
    "ack msg",
    "lbl req",
    "lbl resp",
    "set dt",
    "rst app",
    "shtdn nt",
    "resrt nt",
    "rst bch#s",
    "indv limit",
    "pre req",
    "pre rsp",
    "clr tot",
    "clr bch",
    "heartbeat",
    "clr bch recs",
    "slave pre",
    "bad1"
};

// translates the distribution modes to a description
char dist_mode_desc[10][MAX_DBG_DESC] =
{   "Bad0",
    "m1/nogrd",
    "m2",
    "m3/Bch",
    "m4/rl",
    "m5/Bch/rl",
    "m6/Bch/alt",
    "m7/Bch/alt/rl",
    "Bad1",
    "Bad2"
};

// For debug use with checkrange
// translates the bpm modes to a description
char bpm_mode_desc[4][MAX_DBG_DESC] =
{   "reg",
    "trickle",
    "Bad0",
    "Bad1"
};

// For debug use with checkrange
// translates the bpm modes to a description
char chk_rng_desc[10][MAX_DBG_DESC] =
{   "drop it",
    "weight",
    "grade",
    "trickle limit",
    "hole_in logic",
    "Bad1",
    "Bad2",
    "Bad3",
    "Bad4",
    "Bad5"
};

// For debug use with the maintence commands,
// translates the action to description
char drp_state_desc[10][MAX_DBG_DESC] =
{   "set flgs",
    "rst bpm",
    "rstbchloop",
    "bchnumreq",
    "Bad2",
    "Bad3",
    "Bad4",
    "Bad5",
    "Bad6",
    "Bad7"
};

// For debug use with the maintence commands,
// translates the action to description
char trc_buf_desc[MAXTRCBUFFERS][MAX_DBG_DESC] =
{   "Main",
    "GpTm",
    "Evth",
    "Dbg ",
    "DmMbx ",
    "IsMain",
    "IsMbx "
};

// For sync error messages to host,
char sync_desc[MAXSYNCS][MAX_DBG_DESC] =
{   "Scale 1",
    "Scale 2",
    "Drop Sync 1",
    "Drop Sync 2",
    "Drop Sync 3",
    "Drop Sync 4",
    "Drop Sync 5",
    "Drop Sync 6",
    "Drop Sync 7",
    "Drop Sync 8",
    "Drop Sync 9",
    "Drop Sync 10",
    "Drop Sync 11",
    "Drop Sync 12",
    "Drop Sync 13",
    "Drop Sync 14"
};


// !!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!!!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!

// For switching capture buffers when full
const int  nxt_cap_buf[2] = {1,0};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

overhead::overhead()
{
	syncOffset = 0;// offset from the scale sync to the scale.
}

overhead::~overhead()
{

}

LONG __stdcall overhead::appMbxUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** App Mbx Exception!\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

    return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall overhead::appUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** Main Timer Exception!\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

    return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall overhead::appGpUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** GpTimer Exception!\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

    return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall overhead::appDebugUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** Debug Exception!\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

    return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall overhead::appMbxEvUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** Event Handler Exception!\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

    return EXCEPTION_CONTINUE_SEARCH;
}

// Function to read an 8-bit value from an I/O port
unsigned char overhead::ReadFPGARegister(unsigned short base_addr, unsigned char offset) {
    return RtReadPortUchar((PUCHAR)(base_addr + offset));
}

void overhead::CheckIfEPM19() {
    unsigned char product_code = (unsigned char)(ReadFPGARegister(FPGA_BASE_ADDR, FPGA_PRODUCT_CODE_OFFSET) & 0x7F); // Mask 7 bits
    BoardIsEPM19 = (product_code == VL_EPM19_PRODUCT_CODE);
}


//--------------------------------------------------------
//  initialize: starts everything
//--------------------------------------------------------

void overhead::initialize()
{

	//	Determine which board the system is running on
	BoardIsEPM15 = ((unsigned char)(RtReadPortUchar((PUCHAR)0x1d1)) != 0xff);
	 
	// TG - If Board is not EPM15, then check for EPM19
	if (!BoardIsEPM15) {
		CheckIfEPM19();
	}

	//------------- Initialize Memory

    // Open shared memory
    if ((hShmem = InitMem()) == NULL)
    {
        RtPrintf("Error InitMem file %s, line %d \n",_FILE_, __LINE__);
        Sleep (1000);
        RtExitProcess(1);
    }

    // If shared memory was just created (no Interface running), set RUN_APP
    if (!(pShm->AppFlags & RUN_APP))
        pShm->AppFlags |= RUN_APP;

#ifdef	_TELNET_UDP_ENABLED_
    if ((hTraceMem = InitTraceMem()) == NULL)
    {
        RtPrintf("Error InitTraceMem file %s, line %d \n",_FILE_, __LINE__);
        Sleep (1000);
        RtExitProcess(1);
    }
#endif


	ld_cel[LOADCELL_1]    = NULL; //RCB
	ld_cel[LOADCELL_2]    = NULL; //RCB
	ld_cel[LOADCELL_3]    = NULL; //RCB
	ld_cel[LOADCELL_4]    = NULL; //RCB


    // for sending messages to host

   InitTrace();
    RtPrintf("InitTrace complete, calling InitLocals...\n");

//------------- Initialize data

    // This needs to be before IO card
    // init because it sets outputs to 0
    InitLocals();
    RtPrintf("InitLocals complete, calling SetDefaults...\n");

//----- Load hard coded defaults

    SetDefaults();
    RtPrintf("SetDefaults complete\n");

//----- This table is a master lookup table for info on shared memory.
//      It is used for the debug task and for the interface

    InitShmTbl();

//----- Read config files

    ReadConfiguration();

//----- Set initial InterSystem fastest indices for each drop

	SetIsysFastestIndices(-1, true);

//----- Setup 2nd 3724 card requirements from Configuration

	AnalyzeConfig(true);

//----- Any overrides for debug here

#ifdef _DEBUGING_

    InitDebug();

    // Do this only if shared memory changed, there is no host and debugging
    // has to be done. This only needs to be done once to create new files with
    // the new structure(s).
    //for (int i = 1; i < MAX_GROUPS; i++)
    //{
    //    (bool) fsave_grp_tbl[i].changed = true;
    //    (bool) fsave_grp_tbl[i].loaded  = true;
    //}

#endif

    pShm->OpMode          = ModeStart;
    shm_updates[OPMODE-1] = true;

//------------- Initialize the IO cards

    InitIO();

//------------- Create events/threads to handle communications

    InitCommunications();

	//	Check to see what board we are running on
	if (BoardIsEPM15)
	{
		//	We're running on MANX under XP, so change COM2, COM3 & COM4 IRQs
		#ifdef _SIMULATION_MODE_
			ucbList[1].irq = 7;		//	COM2
			ucbList[2].irq = 3;		//	COM3
			ucbList[3].irq = 5;		//	COM4
		#else
			ucbList[3].irq = 7;		//	COM2
			ucbList[0].irq = 3;		//	COM3
			ucbList[1].irq = 5;		//	COM4
		#endif

		//	Initialize COM3 & COM4 ports to RS-422 by writing to SPECIAL_CONTROL_REGISTER
		RtWritePortUchar((PUCHAR)0x1da, 0x33);

	}
	// TG - Check if we are running EPM19
	else if (BoardIsEPM19) {
		#ifdef _SIMULATION_MODE_
			ucbList[1].irq = 7;		//	COM2
			ucbList[2].irq = 3;		//	COM3
			ucbList[3].irq = 11;	//	COM4
		#else
			ucbList[3].irq = 7;		//	COM2
			ucbList[0].irq = 3;		//	COM3
			ucbList[1].irq = 11;	//	COM4
		#endif

		//	Initialize COM3 & COM4 ports to RS-422 by writing to SPECIAL_CONTROL_REGISTER
		// Set GPIO-P2[7:0] to output mode
		RtWritePortUchar((PUCHAR)GPIO_P2_DIRECTION_REGISTER, 0xFF);
		RtPrintf("EPM19 - Set GPIO-P2 to output mode (0xFF written to 0x9A).\n");

		// Enable RS-485 mode on COM3 and COM4, and enable transceivers
		RtWritePortUchar((PUCHAR)GPIO_P2_DATA_REGISTER, 0x0E);
		RtPrintf("EPM19-Enabled RS-485 mode and transceivers (0x0E written to 0x7A).\n");
	}
	else // Assume Were Running VL-EPM-19 (FOX)
	{
		#ifdef _SIMULATION_MODE_
			ucbList[1].irq = 7;		//	COM2
			ucbList[2].irq = 3;		//	COM3
			ucbList[3].irq = 5;		//	COM4
		#else
			ucbList[3].irq = 7;		//	COM2
			ucbList[0].irq = 3;		//	COM3
			ucbList[1].irq = 11;	//	COM4
		#endif
	}
#ifndef _WEIGHT_SIMULATION_MODE_ //GLC added 1/24/05

//------------- Set up load cell

    // Create load cell initialized event GLC

    char      load_cell_init_event[255];
    for (int i=0;i<MAXSCALES;i++)
    {
        sprintf(load_cell_init_event, "Load Cell %d Initialized", i+1);
        hLoadCellInitialized[i] = RtCreateEvent(NULL, true, 0, load_cell_init_event);
        if(hLoadCellInitialized[i] == NULL)
        {
            sprintf(load_cell_init_event, "Failure to create Load Cell %d Initialized event!\n");
            app->GenError(critical, load_cell_init_event);
        }
    }

#ifndef _WEIGHT_SIMULATION_MODE_       //GLC
		InitLoadCell();
#endif
	//Sleep(5000);

#endif

    //------------- Other threads and timers

    InitMiscThreads();

}

//--------------------------------------------------------
//  InitMem: Creates shared memory
//--------------------------------------------------------

HANDLE overhead::InitMem()
{

#ifdef  _PC104_BBSRAM_

    LARGE_INTEGER base;

//----- Open the battery backed ram on pc104

    if(!RtEnablePortIo(MPCRTLREG, MPCRTLBYTES))
    {
        sprintf(app_err_buf,"Can't enable vsbc6 io %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    RtWritePortUchar(MPCRTLREG, BBSRAMENABLE);

    base.QuadPart = BBSRAMBASEADDR;
    bbram_addr    = RtMapMemory(base ,(ULONG) BBSRAMSIZE, 0);

    if (bbram_addr == NULL)
    {
        sprintf(app_err_buf,"Can't enable vsbc6 bbsram %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

#endif

//----- Open shared memory

    hShmem = RtOpenSharedMemory(SHM_MAP_WRITE, FALSE, "SharedMemory", (VOID **) &pShm);

    return (hShmem);
}

//--------------------------------------------------------
//  InitMem: Creates shared memory
//--------------------------------------------------------

HANDLE overhead::InitTraceMem()
{


//----- Open shared memory
	pTraceMemory = NULL;

    hTraceMem = RtOpenSharedMemory(SHM_MAP_WRITE, FALSE, "TraceMemory", (VOID **) &pTraceMemory);

    return (hTraceMem);
}


HANDLE overhead::InitDebugShell()
{
    DWORD            threadId;

	hTelnetTermSMem = RtOpenSharedMemory(SHM_MAP_WRITE, FALSE,
				"TelnetTermSMem", (VOID **) &atdTelnetd[0].ssmMShare.pSharedMemory);
	if(hTelnetTermSMem == NULL)
	{
		RtPrintf("InitDebugShell OpenSharedMemory failed\n");
	}

	hTerminalThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)TerminalThread,0,CREATE_SUSPENDED,(LPDWORD) &threadId);
	if(hTerminalThread == NULL)
    {
        RtPrintf("InitDebugShell Create thread failed\n");
        //ErrorRoutine((BYTE*) "Error: CreateThread hTerminalThread");
    }

    if(!RtSetThreadPriority( hTerminalThread, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        //ErrorRoutine((BYTE*) "Error: CreateThread SetPriority hTerminalThread");
    }
    ResumeThread( hTerminalThread);

	return (hTerminalThread);
}



//--------------------------------------------------------
//  InitInitTrace
//--------------------------------------------------------

int overhead::InitTrace()
{
    int i;

//----- Create/Open Mutex for trace buffer

    // Create 1st
    RtPrintf("InitTrace: Creating main trace mutex...\n");
    trc[MAINBUFID].mutex = RtCreateMutex( NULL, FALSE, "app_main_trace");
    RtPrintf("InitTrace: main trace mutex = %p\n", trc[MAINBUFID].mutex);

    if ( trc[MAINBUFID].mutex == NULL )
    {
        sprintf(app_err_buf,"Can't create mutex %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    // Open rest
    for ( i = 1; i < MAXTRCBUFFERS; i++)
    {
        trc[i].mutex = RtOpenMutex( SYNCHRONIZE, FALSE, "app_main_trace");
        RtPrintf("InitTrace: trc[%d].mutex = %p\n", i, trc[i].mutex);

        if ( trc[i].mutex == NULL )
        {
            sprintf(app_err_buf,"Can't open mutex %s line %d\n", _FILE_, __LINE__);
            GenError(critical, app_err_buf);
        }
    }

    // For label info table
    RtPrintf("InitTrace: Creating label_tbl mutex...\n");
    lbl_mutex = RtCreateMutex( NULL, FALSE, "label_tbl");
    RtPrintf("InitTrace: lbl_mutex = %p\n", lbl_mutex);

    if ( lbl_mutex == NULL )
    {
        sprintf(app_err_buf,"Can't open mutex %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }
    return 0;
}

//--------------------------------------------------------
//  InitMiscThreads
//--------------------------------------------------------

void overhead::InitMiscThreads()
{

//------------- Create/Start Debug Thread
    if (!CreateDebugThread())
    {
        sprintf(app_err_buf,"Can't create debug thread %s line %d\n", _FILE_, __LINE__);
        GenError(warning, app_err_buf);
    }

//------------- Create timer(s)

    if ((hGpTimer = Create_Gp_Timer()) == NULL)
    {
        sprintf(app_err_buf,"Can't create Gp timer %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((hFstTimer = Create_Fast_Update()) == NULL)
    {
        sprintf(app_err_buf,"Can't create Gp timer %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((hAppTimer = Create_App_Timer()) == NULL)
    {
        RtCancelTimer(hGpTimer, NULL);
        sprintf(app_err_buf,"Can't create App timer %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

//------------- Gp send thread

    if (!CreateGpSendThread())
    {
        sprintf(app_err_buf,"Can't create gp send thread %s line %d\n", _FILE_, __LINE__);
        GenError(warning, app_err_buf);
    }

//------------- Create Simulator instance

    if (simulation_mode)
    {
        if ((sim = new Simulator) == NULL)
        {
            sprintf(app_err_buf,"Can't create Sim Obj %s line %d\n", _FILE_, __LINE__);
            GenError(warning, app_err_buf);
        }
    }
}

//--------------------------------------------------------
//  InitIO
//--------------------------------------------------------

void overhead::InitIO()
{

//------------- Initialize the LoadCell card
/*
#ifdef _1510_LOAD_CELL_

    if (!(ld_cel = new LoadCell_1510))
    {
        sprintf(app_err_buf,"Can't create loadcell obj %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

//----- Create/Open Mutex for load cell

    load_cell_mutex[MAINBUFID] = RtCreateMutex( NULL, FALSE, "loadCell");

    if ( load_cell_mutex[MAINBUFID] == NULL )
    {
        sprintf(app_err_buf,"Can't create load cell mutex %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    load_cell_mutex[GPBUFID] = RtOpenMutex( NULL, FALSE, "loadCell");

    if ( load_cell_mutex[GPBUFID] == NULL )
    {
        sprintf(app_err_buf,"Can't open load cell mutex %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    // Request Mutex for reading/writing
    if(RtWaitForSingleObject(load_cell_mutex[MAINBUFID], WAIT200MS) != WAIT_OBJECT_0)
        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    else
    {
        ld_cel->init_adc(AVG, 10);

        if(!RtReleaseMutex(load_cell_mutex[MAINBUFID]))
            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    }

#endif
*/
//------------- Initialize the PCM-3724 I/O simulator (if enabled)
//              Must be initialized BEFORE the I/O boards so virtual
//              registers are ready for initialize() write operations.

#ifdef _PCM3724_SIM_
    PCM3724Sim_Init();
    PCM3724Sim_Start();
#endif

//------------- Initialize the digital card(s)

#ifdef _3724_IO_

    if((iop = new io_3724) == NULL) // primary i/o
    {
        sprintf(app_err_buf,"Can't create 3724 io obj %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    iop->initialize();

#endif

#ifdef _VSBC6_IO_

	if (BoardIsEPM15)
	{
		if((iop_3 = new io_3724_3) == NULL) // secondary i/o
		{
			sprintf(app_err_buf,"Can't create 3724_3 io obj %s line %d\n", _FILE_, __LINE__);
			GenError(critical, app_err_buf);
		}

		iop_3->initialize();
	}
	// TG - Adding EPM19 Support
	else if (BoardIsEPM19)
	{
		// TG - TODO (Use EPM15 for now)
		if((iop_4 = new io_3724_4) == NULL) // secondary i/o
		{
			sprintf(app_err_buf,"Can't create 3724_4 io obj %s line %d\n", _FILE_, __LINE__);
			GenError(critical, app_err_buf);
		}

		iop_4->initialize();
	}
	else
	{

		if ((ios = new io_vsbc6) == NULL) // secondary i/o
		{
			sprintf(app_err_buf,"Can't create vsbc6 io obj %s line %d\n", _FILE_, __LINE__);
			GenError(critical, app_err_buf);
		}

		ios->initialize();
	}

#endif

#ifdef _3724_IO_

	//	Only initilalize 2nd 3724 card if we have need for it
	if (Second_3724_Required)
	{
		if((iop_2 = new io_3724_2) == NULL) // primary i/o
		{
			sprintf(app_err_buf,"Can't create 3724_2 io obj %s line %d\n", _FILE_, __LINE__);
			GenError(critical, app_err_buf);
		}

		iop_2->initialize();
	}
#endif

}


//--------------------------------------------------------
//  ReadConfiguration
//--------------------------------------------------------

void overhead::ReadConfiguration()
{
#ifndef  _PC104_BBSRAM_
    UINT len;
#endif

//----- Allocate memory for file read/writes

    // allocate about 50K memory for file read/write operations
    fbuf_addr.QuadPart = 0xFFFFFF;
    file_buf = (char*) RtAllocateContiguousMemory(MAXFILEBUFFER, fbuf_addr);

    if (!file_buf)
    {
        sprintf(app_err_buf,"Can't allocate file buffer %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

#ifdef  _PC104_BBSRAM_
    {
        bb_sram* bb = (bb_sram*) bbram_addr;
        ULONG    cs;
        UINT     len;

        //memset(bb, 0, sizeof(bb_sram));
        //bb->sset_cs    = 1;
        //bb->scl_set_cs = 1;
        //bb->tare_cs    = 1;
        //bb->sch_cs     = 1;

//----- Drop record info

        len = sizeof(fdrec_info);
        cs  = CheckSum((BYTE*) &bb->drec, len);
        if (cs == bb->drec_cs)
        {
            //RtPrintf("Checksum good drecinfo data.\n");
            memcpy(&sav_drp_rec_file_info, &bb->drec, len);
            //RtPrintf(" curfile %d nxt2snd %d curcnt %d nxtseq %u nxtlbl %u\n",
            //            sav_drp_rec_file_info.cur_file,
            //            sav_drp_rec_file_info.nxt2_send,
            //            sav_drp_rec_file_info.cur_cnt,
            //            sav_drp_rec_file_info.nxt_seqnum,
            //            sav_drp_rec_file_info.nxt_lbl_seqnum);
        }
        else
        {
            RtPrintf("No bbsram drop record info.\n");
            sav_drp_rec_file_info.cur_file       = 0;
            sav_drp_rec_file_info.nxt2_send      = 0;
            sav_drp_rec_file_info.cur_cnt        = 0;
            sav_drp_rec_file_info.nxt_seqnum     = 1;
            sav_drp_rec_file_info.nxt_lbl_seqnum = 1;
        }

//----- Totals

        len = sizeof(ftot_info);
        cs  = CheckSum((BYTE*) &bb->tots, len);
        if (cs == bb->tots_cs)
        {
            //RtPrintf("Checksum good totals info.\n");
            // Put saved totals into the right place.
            memcpy(&pShm->sys_stat.TotalCount,      &bb->tots.TCnt,     sizeof(totals_info.TCnt));
            memcpy(&pShm->sys_stat.TotalWeight,     &bb->tots.TWt,      sizeof(totals_info.TWt));
            memcpy(&pShm->sys_stat.MissedDropInfo,  &bb->tots.MdrpInfo, sizeof(totals_info.MdrpInfo));
            memcpy(&pShm->sys_stat.DropInfo,        &bb->tots.DrpInfo,  sizeof(totals_info.DrpInfo));
        }
        else
        {
            RtPrintf("No bbsram totals.\n");
        }

//----- System settings

        len = sizeof(sys_in);
        cs  = CheckSum((BYTE*) &bb->sset, len);
        if ( (cs == bb->sset_cs)          &&
             (pShm->sys_set.Shackles > 0) &&  // sanity check, cs could pass with all 0's.
             (pShm->sys_set.Shackles <= MAXPENDANT) )
        {
            //RtPrintf("Checksum good system settings.\n");
            //RtPrintf("sset %u size %u cs %u\n", (UINT) &bb->sset, len, cs);
            // Put saved totals into the right place.
            memcpy(&pShm->sys_set, &bb->sset, len);
            (bool) fsave_grp_tbl[SYS_IN_GROUP].loaded = true;
            grade_zeroed[0] = ( pShm->sys_set.Grading ? false : true );
			grade_zeroed[1] = ( pShm->sys_set.Grading ? false : true );
        }
        else RtPrintf("No bbsram system settings.\n");

//----- Scale settings

        len = sizeof(scl_in);
        cs  = CheckSum((BYTE*) &bb->scl_set, len);
        if ( (cs == bb->scl_set_cs) &&
             (pShm->scl_set.NumScales > 0) &&  // sanity check, cs could pass with all 0's.
             (pShm->scl_set.NumScales <= MAXSCALES) )
        {
            //RtPrintf("Checksum good scale settings.\n");
            //RtPrintf("scl_set %u size %u cs %u\n", (UINT) &bb->scl_set, len, cs);
            // Put saved totals into the right place.
            memcpy(&pShm->scl_set, &bb->scl_set, len);
            (bool) fsave_grp_tbl[SCL_IN_GROUP].loaded = true;
        }
        else RtPrintf("No bbsram scale settings.\n");

//----- Tares

        len = sizeof(pShm->ShackleTares);
        cs  = CheckSum((BYTE*) &bb->tares, len);
        if ( (cs == bb->tare_cs) && (cs != 0) ) // sanity check, cs could pass with all 0's.
        {
            //RtPrintf("Checksum good tares.\n");
            //RtPrintf("tares %u size %u cs %u\n", (UINT) &bb->tares, len, cs);
            // Put saved totals into the right place.
            memcpy(&pShm->ShackleTares, &bb->tares, len);
            (bool) fsave_grp_tbl[SHKL_TARES].loaded = true;
        }
        else RtPrintf("No bbsram tares.\n");

//----- Schedules

        len = sizeof(pShm->Schedule);
        cs  = CheckSum((BYTE*) &bb->sched, len);
        if ( (cs == bb->sch_cs) && (cs != 0) ) // sanity check, cs could pass with all 0's.
        {
            //RtPrintf("Checksum good schedule.\n");
            //RtPrintf("sched %u size %u cs %u\n", (UINT) &bb->sched, len, cs);
            // Put saved totals into the right place.
            memcpy(&pShm->Schedule, &bb->sched, len);
            (bool) fsave_grp_tbl[SCHEDULE].loaded = true;
        }
        else RtPrintf("No bbsram schedule.\n");
    }
#endif

#ifndef  _PC104_BBSRAM_

//----- Read shared memory settings from group files, skip NO_GROUP.

    for ( int i = 1; i < MAX_GROUPS; i++)
    {
        len = fsave_grp_tbl[i].struct_size;
        if (Read_File(OPEN_EXISTING, (char*) fsave_grp_tbl[i].fname,
                     (BYTE*) fsave_grp_tbl[i].struct_ptr,
                      &len, NULL) == ERROR_OCCURED)
            RtPrintf("Error reading file %s\n", (char*) fsave_grp_tbl[i].desc);
        else
            fsave_grp_tbl[i].loaded = true;
    }

//----- Read totals.

    len = sizeof(totals_info);
    if (FileExists(TOTALS_PATH))
    {
        if (Read_File(OPEN_EXISTING, TOTALS_PATH,
                     (BYTE*) &totals_info,
                      &len, NULL) == NO_ERRORS)
        {
            // Put saved totals into the right place.
            memcpy(&pShm->sys_stat.TotalCount,      &totals_info.TCnt,     sizeof(totals_info.TCnt));
            memcpy(&pShm->sys_stat.TotalWeight,     &totals_info.TWt,      sizeof(totals_info.TWt));
            memcpy(&pShm->sys_stat.MissedDropInfo,  &totals_info.MdrpInfo, sizeof(totals_info.MdrpInfo));
            memcpy(&pShm->sys_stat.DropInfo,        &totals_info.DrpInfo,  sizeof(totals_info.DrpInfo));
            RtPrintf("Totals restored from file.\n");
        }
        else
            RtPrintf("Error reading totals file - starting with zero totals.\n");
    }
    else
    {
        RtPrintf("No saved totals file - starting with zero totals.\n");
        memset(&totals_info, 0, sizeof(totals_info));
        if (Write_File(CREATE_ALWAYS, TOTALS_PATH,
                 (BYTE*) &totals_info,
                 sizeof(totals_info)) == ERROR_OCCURED)
            RtPrintf("Error creating totals file.\n");
    }

//----- Read the drop record recovery info.

    len = sizeof(sav_drp_rec_file_info);
    if (FileExists(DREC_INFO_PATH))
    {
        if (Read_File(OPEN_EXISTING, DREC_INFO_PATH,
                     (BYTE*) &sav_drp_rec_file_info,
                      &len, NULL) != NO_ERRORS)
        {
            RtPrintf("Error reading drecinfo - reinitializing.\n");
            goto init_drecinfo;
        }
        else
            RtPrintf("Drop record info restored (seq %d).\n", sav_drp_rec_file_info.nxt_seqnum);
    }
    else
    {
init_drecinfo:
        RtPrintf("Initializing new drop record tracking file.\n");
        sav_drp_rec_file_info.cur_file       = 0;
        sav_drp_rec_file_info.nxt2_send      = 0;
        sav_drp_rec_file_info.cur_cnt        = 0;
        sav_drp_rec_file_info.nxt_seqnum     = 1;
        sav_drp_rec_file_info.nxt_lbl_seqnum = 1;

        if (Write_File(CREATE_ALWAYS, DREC_INFO_PATH,
                 (BYTE*) &sav_drp_rec_file_info,
                 sizeof(sav_drp_rec_file_info)) == ERROR_OCCURED)
            RtPrintf("Error writing drecinfo file.\n");
    }

#endif

}

//--------------------------------------------------------
//  InitCommunications
//--------------------------------------------------------

void overhead::InitCommunications()
{

//---- Event to synchronize all mailbox clients/servers

    hInterfaceThreadsOk = RtOpenEvent(NULL,NULL,"IPC Threads");

    if(hInterfaceThreadsOk == NULL)
    {
        sprintf(app_err_buf,"Can't open threads ok event %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

//---- Events/Handler which do work for the app mailbox server

    process_event   [EVT_SHM_READ] = RtCreateEvent(NULL, false, 0,"Read Shared Memory");
    if(process_event[EVT_SHM_READ] == NULL)
    {
        sprintf(app_err_buf,"Create ReadShm event file %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    process_event   [EVT_DREC_RESP] = RtCreateEvent(NULL, false, 0,"Drop Record Response");
    if(process_event[EVT_DREC_RESP] == NULL)
    {
        sprintf(app_err_buf,"Create DrpRecResp event file %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    ///////// for debugging Rx's /////////
    process_event   [EVT_SND_DREC_RESP] = RtCreateEvent(NULL, false, 0,"Drop Record Send");
    if(process_event[EVT_SND_DREC_RESP] == NULL)
    {
        sprintf(app_err_buf,"Create DrpRecResp event file %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    /////////  end for debugging Rx's/////////

//---- Event for mailbox server to wait if need to use the to_be_processed buffer and
//     wait on event handler.

    hEvtHndlr = RtCreateEvent(NULL, true, 0,"Event Handler Ready");
    if(hEvtHndlr == NULL)
    {
        sprintf(app_err_buf,"Create event handler event file %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((CreateMbxEventHandler() != NO_ERRORS))
    {
        sprintf(app_err_buf,"Can't create app mailbox event handler %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

//---- Create mailboxes\threads

    if ((CreateAppMailbox() != NO_ERRORS))
    {
        sprintf(app_err_buf,"Can't create app mailbox %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((CreateAppMbxThread() != NO_ERRORS))
    {
        sprintf(app_err_buf,"Can't create app mailbox thread %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

//----- If intersystems enabled, create drop manager and intersystems


    if ((CreateDMgrMailbox() != NO_ERRORS))
    {
        sprintf(app_err_buf,"Can't create DMgr mailbox %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((CreateIsysMailbox() != NO_ERRORS))
    {
        sprintf(app_err_buf,"Can't create Isys mailbox %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((drpman = new DropManager) == NULL)
    {
        sprintf(app_err_buf,"Can't create DMgr Obj %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

    if ((Isys = new InterSystems) == NULL)
    {
        sprintf(app_err_buf,"Can't create Isys Obj %s line %d\n", _FILE_, __LINE__);
        GenError(critical, app_err_buf);
    }

//----- if intersystems is not enabled, fake the bits for the threads
//      which will not be started.

    else
        pShm->IsysLineStatus.app_threads |= (RTSS_DM_MBX_SVR_RDY |
                                             RTSS_IS_MBX_SVR_RDY |
                                             RTSS_IS_RX_SVR_RDY);
}

//--------------------------------------------------------
//   CreateAppMailbox
//--------------------------------------------------------

int  overhead::CreateAppMailbox()
{
    //
    // Create required RTX IPC objects -- mutex is last so that clients
    // can not start sending messages before the server is initialized.
    //

    hAppShm = RtCreateSharedMemory( PAGE_READWRITE, 0, MAX_APP_MSG_SIZE, APP_SHM, (LPVOID*) &pAppIpcMsg);

    if(GetLastError()==ERROR_ALREADY_EXISTS)
        RtPrintf("Warning!\nThe shared memory does already exist.\nThe MsgServer may already be running.\n");

    if (hAppShm==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    pAppIpcMsg->Ack = 1;        // Initilize shared memory data
    hAppSemPost  = RtCreateSemaphore( NULL, lAppInitialCount, lAppMaximumCount, APP_SEM_POST);

    if (hAppSemPost==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hAppSemAck = RtCreateSemaphore( NULL, lAppInitialCount, lAppMaximumCount, APP_SEM_ACK);

    if (hAppSemAck==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hAppMutex = RtCreateMutex( NULL, FALSE, APP_MUTEX);

    if (hAppMutex==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//   CreateDMgrMailbox
//--------------------------------------------------------

int  overhead::CreateDMgrMailbox()
{

    //
    // Create required RTX IPC objects -- mutex is last so that clients
    // can not start sending messages before the server is initialized.
    //

    hDrpMgrShm = RtCreateSharedMemory( PAGE_READWRITE, 0, sizeof(pDrpMgrIpcMsg), DRPMGR_SHM, (LPVOID*) &pDrpMgrIpcMsg);

    if(GetLastError()==ERROR_ALREADY_EXISTS)
        RtPrintf("Warning!\nThe shared memory does already exist.\nThe MsgServer may already be running.\n");

    if (hDrpMgrShm==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    pDrpMgrIpcMsg->Ack = 1;        // Initilize shared memory data
    hDrpMgrSemPost  = RtCreateSemaphore( NULL, lAppInitialCount, lAppMaximumCount, DRPMGR_SEM_POST);

    if (hDrpMgrSemPost==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hDrpMgrSemAck = RtCreateSemaphore( NULL, lAppInitialCount, lAppMaximumCount, DRPMGR_SEM_ACK);

    if (hDrpMgrSemAck==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hDrpMgrMutex = RtCreateMutex( NULL, FALSE, DRPMGR_MUTEX);

    if (hDrpMgrMutex==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//   CreateIsysMailbox
//--------------------------------------------------------

int  overhead::CreateIsysMailbox()
{

    //
    // Create required RTX IPC objects -- mutex is last so that clients
    // can not start sending messages before the server is initialized.
    //

    hIsysShm = RtCreateSharedMemory( PAGE_READWRITE, 0, sizeof(pIsysIpcMsg), ISYS_SHM, (LPVOID*) &pIsysIpcMsg);

    if(GetLastError()==ERROR_ALREADY_EXISTS)
        RtPrintf("Warning!\nThe shared memory does already exist.\nThe MsgServer may already be running.\n");

    if (hIsysShm==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    pIsysIpcMsg->Ack = 1;        // Initilize shared memory data
    hIsysSemPost  = RtCreateSemaphore( NULL, lAppInitialCount, lAppMaximumCount, ISYS_SEM_POST);

    if (hIsysSemPost==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hIsysSemAck = RtCreateSemaphore( NULL, lAppInitialCount, lAppMaximumCount, ISYS_SEM_ACK);

    if (hIsysSemAck==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hIsysMutex = RtCreateMutex( NULL, FALSE, ISYS_MUTEX);

    if (hIsysMutex==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  CreateAppMbxThread
//--------------------------------------------------------

int overhead::CreateAppMbxThread()
{
    DWORD            threadId;

//----- Debugger

    hAppRx = CreateThread(
                            NULL,
                            0,               //default
                            (LPTHREAD_START_ROUTINE) Mbx_Server, //function
                            NULL,            //parameters
                            0,
                            (LPDWORD) &threadId
                            );

    if(hAppRx == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hAppRx, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hAppRx, 1);
        return ERROR_OCCURED;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("hAppRx\tthread\t%x \n",threadId);
#endif

    return NO_ERRORS;
}

//--------------------------------------------------------
//  CreateMbxEventHandler
//--------------------------------------------------------

int overhead::CreateMbxEventHandler()
{
    DWORD            threadId;

//----- Debugger

    hAppRxEv = CreateThread(
                            NULL,
                            0,               //default
                            (LPTHREAD_START_ROUTINE) MbxEventHandler, //function
                            NULL,            //parameters
                            0,
                            (LPDWORD) &threadId
                            );

    if(hAppRxEv == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hAppRxEv, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hAppRxEv, 1);
        return ERROR_OCCURED;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("hAppRxEv\tthread\t%x \n",threadId);
#endif

    return NO_ERRORS;
}

//--------------------------------------------------------
//  InitLocals: Initializes global variables
//--------------------------------------------------------

void overhead::InitLocals()
{
    int i,j;

//----- Miscellaneous app flags/variables

    app->this_lineid             = 1;
    app->isys_reset_bpm          = false;
    app->bpm_qrtr                = 0;
    app->trickle_active          = false;
    app->simulation_mode         = false;
    app->shm_valid               = false;
    app->config_Ok               = false;
    app->someone_is_connected    = false;
    app->susp_drop_rec_send      = false;
    app->last_drop_rec_susp      = false;
    app->sendDrpRecs             = false;
    app->sendGpDebug             = false;
    app->sendBpmReset            = false;
    app->masterCheck             = false;
    app->sendBlockRequested      = false;
    app->sendBlockGranted        = false;
    app->configGroupCheck        = false;
    app->send_error.send         = false;
    app->saveTotals              = false;
    app->saveDrpRecs             = false;
    app->drp_rec_count           = 0;
    app->updateDrecInfo          = false;
    app->zero_bias_mode          = 2;

//----- Clear inputs and outputs

    for ( i = 0; i < MAXINPUTBYTS; i++ )
    {
        sync_in[i]    = 0;
        sync_zero[i]  = 0;
        switch_in[i]  = 0;
    }

    for ( i = 0; i < MAXOUTPUTBYTS; i++ )
        output_byte[i] = 0;

    shackle_count      = 0;

//----- Sync stuff

    for (i = 0; i< MAXSYNCS; i++)
    {
        sync_armed[i]    = true;
        sync_debounce[i] = pShm->sys_set.SyncOn;
    }

    for (i = 0; i < MAXGRADESYNCS; i++)
    {
        grade_armed[i]  = false;
        grade_zeroed[i] = false;
    }

//----- Weight stuff

    for (i = 0; i < MAXSCALES; i++)
    {
        weigh_state[i]                      = WeighIdle;
        bpm_act_count[i]                    = 0;
        LastNumAdcReads[i]                  = 99; // something other than real values
        LastOffsetAdc[i]                    = 99; // ""
        LastAdcMode[i]                      = 99; // ""
        get_weight_init[i]                  = false;
    }

    individual_lim                          = 10;

//----- The rest

    isys_shksec_sec_cnt                      = 0;
    isys_shksec_max_cnt                      = 0;

    for ( i = 0; i < MAXDROPS; i++ )
    {
        // More miscellaneous things/drop
        output_timer[i]                     = 0;
        target_trickle_count[i]             = 0;
        trickle_short_cnt[i]                = 0;
		trickle_shorts_added[i]				= 0;
        isys_cnt_slwdn[i]                   = 0;
        isys_wt_slwdn[i]                    = 0;
        isys_fastest_idx[i]                 = 0;
    }

    for (j = 0; j <= MAXIISYSLINES; j++ )
        isys_comm_fail [j]              = true;

    memset(&drp_rec_send_buf,  0,  sizeof(drp_rec_send_buf));  // small buffer for sending drop records
    memset(&sav_rec_buf,       0,  sizeof(sav_rec_buf));       // buffer for saving/reading drop records from file
    memset(&drp_rec_resp_q,    0,  sizeof(drp_rec_resp_q));    // buffer for verifying drop record response

    for ( i = 0; i < MAXTRCBUFFERS; i++ )
    {
        // The trace messages sent to the host should be kept to a minimum. All the debug
        // info is appended to the trace buf in the various functions. Nothing will
        // be sent until gp timer sets the Ok flag.
        sprintf( (char*) &trc_buf[i],"L?:%6.6s\n", trc_buf_desc[i]);
        trc[i].send_OK     = false;
        trc[i].buffer_full = false;
        trc[i].len         = 0;
    }

//----- Batch label stuff

    sendBatchLabel      = false;
    memset(&batch_label, 0, sizeof(batch_label));

    for (i = 0; i < MAXBCHLABELS; i++)
        slave_batch[i]   = 0;

#ifdef _SIMULATION_MODE_

    simulation_mode = true;

#endif

#ifdef _WEIGHT_SIMULATION_MODE_       //GLC

    weight_simulation_mode = true;   //GLC

#endif                               //GLC

    memset(&capt_wt,     0, sizeof(capt_wt));
    memset(&shm_updates, 0, sizeof(shm_updates));
    memset(&w_avg,       0, sizeof(w_avg));        // Zero weight averaging buffers (prevents garbage weights)
}

//--------------------------------------------------------
//  SetDefaults: Initializes data
//--------------------------------------------------------

void overhead::SetDefaults()
{

   int i,j;

   RtPrintf("Setting Defaults.\n");

// There is no need to set anything in shared memory (pShm->) to
// zero or false, it is cleared after allocated.

//----- Grading stuff. See "The Rest" for schedule grades.

    pShm->sys_set.Grading                   = true;
    pShm->sys_set.ResetGrading.ResetGrade                = true;

	for ( i = 0; i < MAXGRADESYNCS; i++ )
	{
		pShm->grade_shackle[i]              = 2;
	}



    for ( i = 0; i < MAXGRADES; i++ )
    {
        pShm->sys_set.GradeArea[i].grade = (char) (pShm->sys_set.Grading ? ('A' + i) : 'N');
        pShm->sys_set.GradeArea[i].offset   = -(i * 4);
		pShm->sys_set.GradeArea[i].GradeSyncUsed = 0;
    }

//----- Miscellaneous

    pShm->sys_set.Shackles                  = 87;
    pShm->sys_set.NumDrops                  = 32;
    pShm->sys_set.LastDrop                  = 12;
    pShm->sys_set.GibDrop                   = 20;
    pShm->sys_set.SkipTrollies              = 0;

//----- Sync stuff

    pShm->sys_set.MBSync[0]                 = 3;
    pShm->sys_set.MBOffset[0]               = 5;
    pShm->sys_set.MBSync[1]                 = 4;
    pShm->sys_set.MBOffset[1]               = 6;
    pShm->sys_set.SyncOn                    = 5;

    for (i = 0; i< MAXSYNCS; i++)
    {
        pShm->SyncStatus[i].shackleno       = 0; //87 - (i*5); //GLC REMOVED 2/15/05//
		true_shackle_count[i] = (__int64) pShm->SyncStatus[i].shackleno; //GLC added 2/14/05

        // What drops are controlled by which sync. The output for the drop will
        // not fire without these.
        if (i == 2)
        {
            pShm->sys_set.SyncSettings[i].first = 1;
            pShm->sys_set.SyncSettings[i].last  = MAXDROPS;
        }
    }

//----- Weight stuff

    //pShm->scl_set.LoadCellType              = LOADCELL_TYPE_HBM;
    pShm->scl_set.NumScales                 = 1;
    pShm->scl_set.AdcMode[0]                = 1;
    pShm->scl_set.AdcMode[1]                = 1;
    pShm->scl_set.NumAdcReads[0]            = ADCREADSDFLT;
    pShm->scl_set.NumAdcReads[1]            = ADCREADSDFLT;
    pShm->scl_set.OffsetAdc[0]              = ADCOFFSET;
    pShm->scl_set.OffsetAdc[1]              = ADCOFFSET;
    pShm->scl_set.ZeroNumber                = 4;
    pShm->scl_set.AutoBiasLimit             = 18;
    pShm->scl_set.LC_Sample_Adj[0].start    = 33;
    pShm->scl_set.LC_Sample_Adj[0].end      = 66;
    pShm->scl_set.LC_Sample_Adj[1].start    = 33;
    pShm->scl_set.LC_Sample_Adj[1].end      = 66;

    pShm->sys_set.MinBird                   = (int) CNV_WT(1);
    pShm->sys_set.GibMin                    = (int) CNV_WT(.5);
    pShm->sys_set.TareTimes                 = 1;

    for (i = 0; i < MAXSCALES; i++)
    {
        pShm->WeighShackle[i]               = 2;
        pShm->WeighZero[i]                  = false;
    }

	// Set HBM load cell defaults. Will be over written by settings f
	for (i = 0; i < MAXSCALES; i++)
	{
		/*
		pShm->scl_set.DigLCSet[i].measure_mode	= 0;
		pShm->scl_set.DigLCSet[i].ASF	= 5;
		pShm->scl_set.DigLCSet[i].FMD	= 1;
		pShm->scl_set.DigLCSet[i].ICR	= 0;
		pShm->scl_set.DigLCSet[i].CWT	= 1000000;
		pShm->scl_set.DigLCSet[i].LDW	= 0;
		pShm->scl_set.DigLCSet[i].LWT	= 1000000;
		pShm->scl_set.DigLCSet[i].NOV	= 0;
		pShm->scl_set.DigLCSet[i].RSN	= 1;
		pShm->scl_set.DigLCSet[i].MTD	= 0;
		pShm->scl_set.DigLCSet[i].LIC0	= 0;
		pShm->scl_set.DigLCSet[i].LIC1	= 1000000;
		pShm->scl_set.DigLCSet[i].LIC2	= 0;
		pShm->scl_set.DigLCSet[i].LIC3	= 0;
		pShm->scl_set.DigLCSet[i].ZTR	= 0;
		pShm->scl_set.DigLCSet[i].ZSE	= 0;
		pShm->scl_set.DigLCSet[i].TRC1	= 0;
		pShm->scl_set.DigLCSet[i].TRC2	= 0;
		pShm->scl_set.DigLCSet[i].TRC3	= 0;
		pShm->scl_set.DigLCSet[i].TRC4	= 0;
		pShm->scl_set.DigLCSet[i].TRC5	= 0;
		*/

		DefaultDigLCSet[i].measure_mode	= 0;
		DefaultDigLCSet[i].ASF			= 5;
		DefaultDigLCSet[i].FMD			= 1;
		DefaultDigLCSet[i].ICR			= 4;
		DefaultDigLCSet[i].CWT			= 500000;
		DefaultDigLCSet[i].LDW			= 0;
		DefaultDigLCSet[i].LWT			= 500000;
		DefaultDigLCSet[i].NOV			= 0;
		DefaultDigLCSet[i].RSN			= 1;
		DefaultDigLCSet[i].MTD			= 0;
		DefaultDigLCSet[i].LIC0			= 0;
		DefaultDigLCSet[i].LIC1			= 500000;
		DefaultDigLCSet[i].LIC2			= 0;
		DefaultDigLCSet[i].LIC3			= 0;
		DefaultDigLCSet[i].ZTR			= 0;
		DefaultDigLCSet[i].ZSE			= 0;
		DefaultDigLCSet[i].TRC1			= 0;
		DefaultDigLCSet[i].TRC2			= 0;
		DefaultDigLCSet[i].TRC3			= 0;
		DefaultDigLCSet[i].TRC4			= 0;
		DefaultDigLCSet[i].TRC5			= 0;

		LastDigLCSet[i].measure_mode	= -1;
		LastDigLCSet[i].ASF				= -1;
		LastDigLCSet[i].FMD				= -1;
		LastDigLCSet[i].ICR				= -1;
		LastDigLCSet[i].CWT				= -1;
		LastDigLCSet[i].LDW				= -1;
		LastDigLCSet[i].LWT				= -1;
		LastDigLCSet[i].NOV				= -1;
		LastDigLCSet[i].RSN				= -1;
		LastDigLCSet[i].MTD				= -1;
		LastDigLCSet[i].LIC0			= -1;
		LastDigLCSet[i].LIC1			= -1;
		LastDigLCSet[i].LIC2			= -1;
		LastDigLCSet[i].LIC3			= -1;
		LastDigLCSet[i].ZTR				= -1;
		LastDigLCSet[i].ZSE				= -1;
		LastDigLCSet[i].TRC1			= -1;
		LastDigLCSet[i].TRC2			= -1;
		LastDigLCSet[i].TRC3			= -1;
		LastDigLCSet[i].TRC4			= -1;
		LastDigLCSet[i].TRC5			= -1;

		pShm->scl_set.DigLCSet[i] = DefaultDigLCSet[i];

	}

//----- Timed operations

    // Main Loop, 100ns * 50000 = 5ms intervals
    // 50 = .25 seconds
    pShm->sys_set.DropOn                    = 50;
    pShm->sys_set.SyncOn                    = 1;
	pShm->sys_set.ReassignTimerMax			= 38;


//----- The rest

    for ( i = 0; i < MAXDROPS; i++ )
    {
        // More miscellaneous things/drop
        pShm->sys_set.Order[i].Unload       = i + 1;

        for (j = 0; j < MAXIISYSLINES; j++ )
        {
            pShm->sys_set.IsysDropSettings[i].active[j]  = false;
            pShm->sys_set.IsysDropSettings[i].drop[j]    = i+j+2;
            pShm->IsysDropStatus[i].PPMCount[j]    = 0;
            pShm->IsysDropStatus[i].PPMPrevious[j] = 0;
            pShm->IsysDropStatus[i].BWeight[j]     = 0;
            pShm->IsysDropStatus[i].BCount[j]      = 0;
        }

        pShm->Schedule[i].NextEntry         = i + 2; //;
        pShm->Schedule[i].DistMode          = mode_4_rate;
        pShm->Schedule[i].BpmMode           = bpm_normal;
        pShm->Schedule[i].BchCntSp          = 20;
        pShm->Schedule[i].BchWtSp           = pShm->Schedule[i].BchCntSp * (pShm->sys_set.MinBird * 3);
        pShm->Schedule[i].BPMSetpoint       = 20;
        pShm->Schedule[i].MinWeight         = (int) CNV_WT(1);
        pShm->Schedule[i].MaxWeight         = (int) CNV_WT(5);
        pShm->Schedule[i].ExtMinWeight      = (int) CNV_WT(1.75);
        pShm->Schedule[i].ExtMaxWeight      = (int) CNV_WT(3.5);
        pShm->Schedule[i].Ext2MinWeight     = (int) CNV_WT(1.5);
        pShm->Schedule[i].Ext2MaxWeight     = (int) CNV_WT(3.75);
        pShm->Schedule[i].Shrinkage         = (int) CNV_WT(.5);

        pShm->sys_set.DropSettings[i].Active           = false;
        pShm->sys_set.DropSettings[i].Extgrade         = true;
        pShm->sys_stat.DropStatus[i].Batched           = 0;
        pShm->sys_stat.DropStatus[i].Suspended         = false;
        pShm->sys_set.DropSettings[i].Sync             = 2;
        pShm->sys_set.DropSettings[i].Offset           = 4 + (i * 2);
		pShm->sys_set.DropSettings[i].DropMode         = DM_NORMAL;		// Normal
		pShm->sys_set.DropSettings[i].SwitchLEDIndex   = 0;				// Since not used for Normal mode, just set it to zero

        for ( j = 0; j <= MAXGRADES - 1; j++ )
        {
            //if (j == 0)
            {
                pShm->Schedule[i].Grade[j]      = char('A'+ j);
                pShm->Schedule[i].Extgrade[j]   = 'B';
            }
            //else
            //{
            //    pShm->Schedule[i].Grade[j]      = 'N';
            //    pShm->Schedule[i].Extgrade[j]   = 'N';
            //}
        }

        pShm->Schedule[i].Grade   [MAXGRADES - 1] = '\0';
        pShm->Schedule[i].Extgrade[MAXGRADES - 1] = '\0';
		last_in_batch_dropped[i] = false;
    }

    // Ensure this_line_id is set so MasterCtrl() doesn't error on this_lineid==0
    // (Settings files may not exist yet, and this field isn't set elsewhere in defaults)
    if (pShm->sys_set.IsysLineSettings.this_line_id == 0)
        pShm->sys_set.IsysLineSettings.this_line_id = 1;

}

//--------------------------------------------------------
//  InitDebug: Any junk used for debug put here
//--------------------------------------------------------
#define LOC_ID  1

void overhead::InitDebug()
{
    //int i;

//----- Info for DebugThread task

    pShm->dbg_set.debug_id.id_count     = 0;

    // Just enter id and index for a specific array element
    pShm->dbg_set.debug_id.ids[0].id    = 21;
    pShm->dbg_set.debug_id.ids[0].index = 0;
    pShm->dbg_set.debug_id.ids[1].id    = 67;
    pShm->dbg_set.debug_id.ids[1].index = 0;
    pShm->dbg_set.debug_id.ids[2].id    = 68;
    pShm->dbg_set.debug_id.ids[2].index = 0;
    pShm->dbg_set.debug_id.ids[3].id    = 69;
    pShm->dbg_set.debug_id.ids[3].index = 0;
    pShm->dbg_set.debug_id.ids[4].id    = 70;
    pShm->dbg_set.debug_id.ids[4].index = 0;
    pShm->dbg_set.dbg_extra             = 0;

    pShm->Schedule [0].DistMode                         = mode_4_rate;
    pShm->Schedule [0].MinWeight                        = (int) CNV_WT(1);
    pShm->Schedule [0].MaxWeight                        = (int) CNV_WT(3);
    pShm->Schedule [0].BpmMode                          = bpm_normal;
    pShm->sys_set.DropSettings[0].Active                = true;
/*
    pShm->sys_set.IsysLineSettings.isys_enable          = 1;
    pShm->sys_set.IsysLineSettings.isys_master          = 0;
    pShm->scl_set.NumScales                             = 1;
    pShm->sys_set.MissedBirdEnable[0]                   = 1;


    pShm->sys_set.IsysDropSettings[0].active[0]         = true;
    pShm->sys_set.IsysDropSettings[0].drop  [0]         = 5;
    pShm->sys_set.IsysDropSettings[0].drop  [LOC_ID-1]  = 1;
    pShm->sys_set.IsysDropSettings[0].active[LOC_ID-1]  = true;

    pShm->Schedule [4].DistMode                         = mode_4_rate;
    pShm->Schedule [4].MinWeight                        = (int) CNV_WT(1);
    pShm->Schedule [4].MaxWeight                        = (int) CNV_WT(5);
    pShm->Schedule [4].BpmMode                          = bpm_normal;
    pShm->sys_set.DropSettings[4].Active                = true;

    pShm->sys_set.IsysDropSettings[4].active[0]         = true;
    pShm->sys_set.IsysDropSettings[4].drop  [0]         = 1;
    pShm->sys_set.IsysDropSettings[4].drop  [LOC_ID-1]  = 5;
    pShm->sys_set.IsysDropSettings[4].active[LOC_ID-1]  = true;

    pShm->sys_set.IsysLineSettings.active  [0]          = true;
    pShm->sys_set.IsysLineSettings.active  [LOC_ID-1]   = true;
    pShm->sys_set.IsysLineSettings.line_ids[0]          = 1;
    pShm->sys_set.IsysLineSettings.this_line_id         = LOC_ID;

    memcpy(pShm->sys_set.IsysLineSettings.pipe_path[0],
           "\\\\ntembed\\pipe\\remote",27);
    memcpy(pShm->sys_set.IsysLineSettings.pipe_path[LOC_ID-1],
           "\\\\ntembed\\pipe\\remote",26);
    memcpy(pShm->sys_set.IsysLineSettings.pipe_path[HOST_INDEX],
           "\\\\ntembed\\pipe\\remote",26);
    //memcpy(pShm->sys_set.IsysLineSettings.pipe_path[HOST_INDEX],
    //       "\\\\elvis\\pipe\\host",22);
*/
}

//--------------------------------------------------------
//  CreateDebugThread
//--------------------------------------------------------

HANDLE overhead::CreateDebugThread()
{
    DWORD           threadId;

//----- Debugger

    hDebug = CreateThread(
        NULL,
        0,               //default
        (LPTHREAD_START_ROUTINE) DebugThread, //function
        NULL,           //parameters
        0,
        (LPDWORD) &threadId
        );

    if(hDebug == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return hDebug;
    }

    if(!RtSetThreadPriority( hDebug, RT_PRIORITY_MIN))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hDebug, 1);
        hDebug = NULL;
        return hDebug;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("DbgTsk\tthread\t%x \n",threadId);
#endif

    return hDebug;
}

//--------------------------------------------------------
//  CreateGpSendThread
//--------------------------------------------------------

HANDLE overhead::CreateGpSendThread()
{
    DWORD threadId;

//----- Debugger

    hGpSend = CreateThread(
        NULL,
        0,               //default
        (LPTHREAD_START_ROUTINE) GpSendThread, //function
        NULL,           //parameters
        0,
        (LPDWORD) &threadId
        );

    if(hGpSend == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return hGpSend;
    }

    if(!RtSetThreadPriority( hGpSend, RT_PRIORITY_MIN+51))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hGpSend, 1);
        hGpSend = NULL;
        return hGpSend;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("GpSend\tthread\t%x \n",threadId);
#endif

    return hGpSend;
}

//--------------------------------------------------------
//  BatchResetStation
//
// Warning: this routine uses i/o points which conflict
// with normal inputs/outputs. The input/outputs start at
// 13 (12 if starting at 0).
//--------------------------------------------------------

void overhead::BatchResetStation()
{
    int i;
    static int delay = 2;
    BYTE pushbuttons;

    pushbuttons = (unsigned char) ~RtReadPortUchar(PCM_3724_1_PORT_B0);

    if (--delay <= 0)
    {
        delay = 2;

        for (i = 0; i < 4; i++)
        {
            if ( BITSET(pushbuttons, i + 4 ) )
            {
                    pShm->sys_stat.ScheduleStatus[i].BchActCnt   = 0;
                    pShm->sys_stat.ScheduleStatus[i].BchActWt    = 0;
                    pShm->sys_stat.DropStatus[i].batch_number    = 0;
                    pShm->sys_stat.DropStatus[i].Batched         = 0;
                    pShm->sys_stat.ScheduleStatus[i].PPMCount    = 0;
                    pShm->sys_stat.ScheduleStatus[i].PPMPrevious = 0;
                    pShm->sys_stat.DropStatus[i].Suspended       = 0;
            }

            if (pShm->sys_stat.DropStatus[i].Batched)
            {
                SetOutput(i + 13, true);
                output_timer[i + 12] = 0xFFFF;
            }
            else
            {
                SetOutput(i + 13, false);
                output_timer[i + 12] = 0;
            }
        }
    }
}


//--------------------------------------------------------
//  SaveTotals
//--------------------------------------------------------

void overhead::SaveTotals()
{
    static __int64 lastTotalCnt = 0;
    static UINT    lastSeqNum   = 0;

#ifdef  _PC104_BBSRAM_
    bb_sram* bb = (bb_sram*) bbram_addr;
    ULONG    cs;
    UINT     len;
#endif

    // Totals have changed, save them.
    if (lastTotalCnt != pShm->sys_stat.TotalCount)
    {
        lastTotalCnt = pShm->sys_stat.TotalCount;
        memcpy(&totals_info.TCnt,     &pShm->sys_stat.TotalCount,     sizeof(totals_info.TCnt));
        memcpy(&totals_info.TWt,      &pShm->sys_stat.TotalWeight,    sizeof(totals_info.TWt));
        memcpy(&totals_info.MdrpInfo, &pShm->sys_stat.MissedDropInfo, sizeof(totals_info.MdrpInfo));
        memcpy(&totals_info.DrpInfo,  &pShm->sys_stat.DropInfo,       sizeof(totals_info.DrpInfo));

#ifdef  _PC104_BBSRAM_

        len = sizeof(ftot_info);
        memcpy(&bb->tots, &totals_info, len);
        cs  = CheckSum((BYTE*) &bb->tots, len);
        bb->tots_cs = cs;

#endif

#ifndef  _PC104_BBSRAM_

        if (Write_File( CREATE_ALWAYS, TOTALS_PATH,
                        (BYTE*) &totals_info,
                        sizeof(totals_info)) == ERROR_OCCURED)
            RtPrintf("Error writing totals.\n");
#endif

    }


//----- Update the drop record recovery info if needed. During good communications,
//      the current sequence number is not saved, this is a good place to update it.

    if (lastSeqNum != sav_drp_rec_file_info.nxt_seqnum)
    {
        lastSeqNum = sav_drp_rec_file_info.nxt_seqnum;

#ifdef  _PC104_BBSRAM_

        len = sizeof(fdrec_info);
        memcpy(&bb->drec, &sav_drp_rec_file_info, len);
        cs  = CheckSum((BYTE*) &bb->drec, len);
        bb->drec_cs = cs;

#endif

#ifndef  _PC104_BBSRAM_

        if (Write_File( CREATE_ALWAYS, DREC_INFO_PATH,
                        (BYTE*) &sav_drp_rec_file_info,
                         sizeof(sav_drp_rec_file_info)) == ERROR_OCCURED)
            RtPrintf("Error writing drec info.\n");
#endif
    }


}

//--------------------------------------------------------
//  CheckConfig
//--------------------------------------------------------

void overhead::CheckConfig()
{
    static bool start_msg       = false;
    static bool ferr_sent       = false;
    int         i, files_loaded = 0;

    // write group files, skip NO_GROUP.
    for ( i = 1; i < MAX_GROUPS; i++)
    {
        // see if anything in shared memory has changed
        if (fsave_grp_tbl[i].changed)
        {

#ifdef  _PC104_BBSRAM_

            {
                bb_sram* bb = (bb_sram*) bbram_addr;
                ULONG    cs;
                UINT     len;

                switch(i)
                {
                    case SYS_IN_GROUP:
                        len = sizeof(sys_in);
                        memcpy(&bb->sset, &pShm->sys_set, len);
                        cs  = CheckSum((BYTE*) &bb->sset, len);
                        bb->sset_cs = cs;
                        //RtPrintf("sset %u size %u cs %u\n", (UINT) &bb->sset, len, cs);
                        break;

                    case SCL_IN_GROUP:
                        len = sizeof(scl_in);
                        memcpy(&bb->scl_set, &pShm->scl_set, len);
                        cs  = CheckSum((BYTE*) &bb->scl_set, len);
                        bb->scl_set_cs = cs;
                        //RtPrintf("scl_set %u size %u cs %u\n", (UINT) &bb->scl_set, len, cs);
                        break;

                    case SHKL_TARES:
                        len = sizeof(pShm->ShackleTares);
                        memcpy(&bb->tares, &pShm->ShackleTares, len);
                        cs  = CheckSum((BYTE*) &bb->tares, len);
                        bb->tare_cs = cs;
                        //RtPrintf("tares %u size %u cs %u\n", (UINT) &bb->tares, len, cs);
                        break;

                    case SCHEDULE:
                        len = sizeof(pShm->Schedule);
                        memcpy(&bb->sched, &pShm->Schedule, len);
                        cs  = CheckSum((BYTE*) &bb->sched, len);
                        bb->sch_cs = cs;
                        //RtPrintf("sched %u size %u cs %u\n", (UINT) &bb->sched, len, cs);
                        break;

                     default:
                        break;
                }
            }

            (bool) fsave_grp_tbl[i].changed = false;

#endif

#ifndef  _PC104_BBSRAM_

            if (Write_File( CREATE_ALWAYS, (char*) fsave_grp_tbl[i].fname,
                           (BYTE*) fsave_grp_tbl[i].struct_ptr,
                           fsave_grp_tbl[i].struct_size) == ERROR_OCCURED)
            {
                if (!ferr_sent)
                {
                    sprintf(app_err_buf,"Error writing file %s\n", fsave_grp_tbl[i].desc);
                    GenError(warning, app_err_buf);
                    ferr_sent = true;
                }
            }
            else
                fsave_grp_tbl[i].changed = false;
#endif
        }

        if(fsave_grp_tbl[i].loaded)
            files_loaded++;
    }

//----- The configuration may not have been loaded at startup, but when
//      the missing settings come in, flag the config good.

    if (files_loaded == MAX_GROUPS - 1)
    {
        config_Ok = true;
        ferr_sent = false;

        // Send a message to the host stating we started with no errors
        if (!start_msg)
        {
            sprintf(app_err_buf,"System started, configuration OK.\n");
            GenError(informational, app_err_buf);
            start_msg = true;
        }
    }
    else
    {
        config_Ok = false;

        // On Linux (TCP), skip the named pipe path validation.
        // On Windows the path started with \\ (0x5c), but TCP doesn't use pipe paths.
        if (1)
        {
            // send message to host requesting an update. skip NO_GROUP.
            for ( i = 1; i < MAX_GROUPS; i++)
            {
                if (fsave_grp_tbl[i].loaded == false)
                {
                    // Request Mutex for send
                    if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    else
                    {
                        RtPrintf("Requesting group %d\n",i);

                        if HOST_OK
                            SendHostMsg( SHM_READ, fsave_grp_tbl[i].id, (BYTE*) NULL, sizeof(int) );

                        if(!RtReleaseMutex(trc[GPBUFID].mutex))
                            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    }
                }
            }
        }
        else RtPrintf("No Path, can't request configuration.\n");
    }
}

//--------------------------------------------------------
//  SendSlavePreLabel:
//--------------------------------------------------------

void overhead::SendSlavePreLabel()
{
    int line;

    // Request Mutex for send
    if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        return;
    }
    else
    {
        for (int pb = 0; pb < MAXBCHLABELS; pb++)
        {
            if (slave_batch[pb] != 0)
            {
                line = slave_batch[pb] >> LINE_SHIFT;

                if ( (line > 0) && (line < MAXIISYSLINES) )
                {
                    if( (!trc[GPBUFID].buffer_full) && (TraceMask & _LABELS_) )
                    {
                        sprintf((char*) &tmp_trc_buf[GPBUFID],"Send\tplabel\tline\t%d\tbch#\t%d\t(slave)\n",
                                line, slave_batch[pb] & BATCH_MASK);
                        strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
                    }
                    SendLineMsg( SLAVE_PRE_LABEL, line , NULL, (BYTE*) &slave_batch[pb], sizeof(int));
                }
                else
                    RtPrintf("Error SndSPre line %d file %s, line %d \n",line, _FILE_, __LINE__);

                memset(&slave_batch[pb], 0, sizeof(int));

            }
        }

        if(!RtReleaseMutex(trc[GPBUFID].mutex))
            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    }
}

//--------------------------------------------------------
//  SendPreLabel:
//--------------------------------------------------------

void overhead::SendPreLabel()
{
    for (int idx = 0; idx < MAXBCHLABELS; idx++)
    {
        if ( (batch_label.info[idx].seq_num        != 0) &&
             (batch_label.info[idx].pre_label_step == 1) )
        {
            if( (!trc[GPBUFID].buffer_full) && (TraceMask & _LABELS_) )
            {
                sprintf((char*) &tmp_trc_buf[GPBUFID],"Send\tplabel\tbch#\t%d\tindx\t%d\n",
                        batch_label.info[idx].seq_num & BATCH_MASK, idx);

                strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
            }

            // Request Mutex for send
            if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS ) != WAIT_OBJECT_0)
            {
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                return;
            }
            else
            {
                SendHostMsg( PRN_BCH_PRE_REQ, 0, (BYTE*) &batch_label.info[idx].seq_num, sizeof(int));

                if(!RtReleaseMutex(trc[GPBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                return;
            }
        }
    }
}

//--------------------------------------------------------
//  SendLabelInfo:
//--------------------------------------------------------

void overhead::SendLabelInfo()
{
    for (int idx = 0; idx < MAXBCHLABELS; idx++)
    {
        if ( (batch_label.info[idx].seq_num    != 0) &&
             (batch_label.info[idx].label_step == 0) )
        {
            if( (!trc[GPBUFID].buffer_full) && (TraceMask & _LABELS_) )
            {
                sprintf((char*) &tmp_trc_buf[GPBUFID],"Send\tlabel\tbch#\t%d\tindx\t%d\n",
                          batch_label.info[idx].seq_num & BATCH_MASK, idx);
                strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
            }

            // Request Mutex for send
            if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
            {
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                return;
            }
            else
            {

                SendHostMsg( PRN_BCH_REQ, 0, (BYTE*) &batch_label.info[idx].seq_num, sizeof(print_info) - (sizeof(int)*2));

                if(!RtReleaseMutex(trc[GPBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                return;
            }
        }
    }
}

//--------------------------------------------------------
//  UpdateHost: Send id update to host
//--------------------------------------------------------

void overhead::UpdateHost()
{
    for (int i = 0; i < ALL_SHM_IDS; i++)
    {
        if ( (shm_updates[i]) && (i != DISP_DATA-1) ) //display data done in fast update
        {
            if HOST_OK
            {
                // Request Mutex for send
                if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
                    RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                else
                {
                    shm_updates[i] = false;

                    SendHostMsg( SHM_WRITE, i + 1,
                                 (BYTE*) shm_tbl[i].data,
                                 shm_tbl[i].struct_len);

                    if(!RtReleaseMutex(trc[GPBUFID].mutex))
                        RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                }
            }
        }
    }
}

//--------------------------------------------------------
//  UpdateCapture: Send load cell capture info to host
//--------------------------------------------------------

void overhead::UpdateCapture(int buf)
{
    // Request Mutex for send
    if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT100MS) != WAIT_OBJECT_0)
        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    else
    {
        if HOST_OK
            SendHostMsg( LC_CAPTURE_INFO, NULL,
                         (BYTE*) &capt_wt.capture_weights[buf][0],
                          sizeof(__int64) * capt_wt.send_length[buf]);

        if(!RtReleaseMutex(trc[GPBUFID].mutex))
            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    }
}

//--------------------------------------------------------
//  GpDebug: Anything to help debug.
//--------------------------------------------------------

void overhead::GpDebug()
{
    static int last_trc = 0;
    int i;

    for ( i = 0; i < MAXTRCBUFFERS; i++)
    {
        trc[last_trc].len = strlen((char*) &trc_buf[last_trc]);
        //RtPrintf("trace %d len %d\n",last_trc, trc[last_trc].len);

        // stop at high water mark to protect from overruns
        if (trc[last_trc].len > TRCMBUFFULL)
            trc[last_trc].buffer_full = true;
        else
            trc[last_trc].buffer_full = false;

        // if there is something to send, send it.
        if (trc[last_trc].len > MAXTRCIDHDRSIZE)
        {
            trc[last_trc].send_OK = true;
            //RtPrintf("Send trace %s, len %d\n",&trc_buf_desc[last_trc],trc[last_trc].len);
            if (++last_trc >= MAXTRCBUFFERS) last_trc = 0;
            break;
        }
        else
        {
            if (++last_trc >= MAXTRCBUFFERS) last_trc = 0;
        }
    }

//----- Send the trace for this thread if OK.

    if (last_trc == GPBUFID)
    {
        if ( trc[GPBUFID].send_OK )
        {
            // Request Mutex for send
            //if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
            //    RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            //else
            //{

// Send debug to self
#ifdef _REDIRECT_DEBUG_
            // Request Mutex for send
            if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {

                SendLineMsg( DEBUG_MSG, this_lineid, NULL,
                                  (BYTE*) &trc_buf[GPBUFID],
                                  trc[GPBUFID].len);
                if(!RtReleaseMutex(trc[GPBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }

#endif

#ifdef _LOCAL_DEBUG_

				if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
				{
#ifdef	_TELNET_UDP_ENABLED_
					UdpTraceNBuf((char*) &trc_buf[GPBUFID], trc[GPBUFID].len);
#else
					PrintNBuf((char*) &trc_buf[GPBUFID], trc[GPBUFID].len);
#endif
				}
				else
				{
					PrintNBuf((char*) &trc_buf[GPBUFID], trc[GPBUFID].len);
				}

#endif

#ifdef	_HOST_DEBUG_
            // Request Mutex for send
            if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {

				if HOST_OK
				{
					SendHostMsg( DEBUG_MSG, NULL,
                                  (BYTE*) &trc_buf[GPBUFID],
                                  trc[GPBUFID].len);
				}
                if(!RtReleaseMutex(trc[GPBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }
#endif

            // clear old message.
            memset(&trc_buf[GPBUFID], 0, sizeof(trcbuf));
            sprintf( (char*) &trc_buf[GPBUFID],"L%d:%s\n", this_lineid, &trc_buf_desc[GPBUFID]);
            trc[GPBUFID].send_OK = false;
        }
    }

    // timeout for ack when testing shared memory writes to host
#ifdef _SHMW_TIMING_TEST_
    if (shmw_seq_tmo > 0)
        shmw_seq_tmo--;
    else
    {
        if (shmw_seq_done != 0)
        {
            RtGetClockTime(CLOCK_FASTEST,&shmw_EndTime);
		    RtPrintf("SHMW\ttmo\ttime\t%u ms\n",
                     (UINT) ((shmw_EndTime.QuadPart - shmw_StartTime.QuadPart)/10000));
            shmw_seq_done = 0;
        }
    }
#endif
}

//--------------------------------------------------------
//  MasterCtrl
//--------------------------------------------------------

int overhead::MasterCtrl()
{
    int i,          this_line_minus1;
    static DBOOL    master_state = 0;
    static int      comm_state   = 0;
    static int      delay        = 0;
    bool            lower_id     = false;
    bool            mbx_hung     = false;

    // needs valid configuration
    if (this_lineid == 0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    this_line_minus1 = this_lineid - 1;

    // probably a fair assumption
    if ( pShm->IsysLineStatus.connected[this_line_minus1] )
        isys_comm_fail[this_line_minus1] = false;

    // a commfail may not be caught by intersystems on a sudden disconnect
    for (i = 0; i < MAXIISYSLINES; i++)
    {
        if ( !pShm->IsysLineStatus.connected[i])
		{
            isys_comm_fail[i] = true;
		}
    }

    switch(comm_state)
    {
        // Determine the state of communications. Send a port check command to freshen
        // the commfail status.
        case 0:
            for (i = 0; i < MAXIISYSLINES; i++)
            {
                if ( pShm->IsysLineStatus.connected[i]          &&
                     pShm->sys_set.IsysLineSettings.active  [i] &&
                     pShm->sys_set.IsysLineSettings.line_ids[i] &&
                     (i != this_line_minus1) )
                {
                    SendIsysMsg(ISYS_PORTCHK,
                                pShm->sys_set.IsysLineSettings.line_ids[i],
                                NULL, NULL);
                    delay = 7; // allow time for responses
                }
            }
            comm_state = 1;
            break;

        // Wait delay
        case 1:
            if (delay > 0) delay--;
            // Go to next step
            if (!delay) comm_state = 2;
            break;

        // Check for anybody, including host
        case 2:
            someone_is_connected = true;
/*
            someone_is_connected = false;

            // check remote comm status
            for (i = 0; i < MAXIISYSLINES; i++)
            {
                if ( (!isys_comm_fail[i]) &&
                     (i != this_line_minus1) )
                     someone_is_connected = true;
            }

            // only have connected status for host
            if (pShm->IsysLineStatus.connected[MAXIISYSLINES])
                someone_is_connected = true;

            //RtPrintf("someone_is_connected? %d hcfs %d rcfs %d %d %d %d\n",
            //            someone_is_connected,
            //            pShm->IsysLineStatus.connected[MAXIISYSLINES],
            //            isys_comm_fail[0],
            //            isys_comm_fail[1],
            //            isys_comm_fail[2],
            //            isys_comm_fail[3]);
*/
            comm_state = 3;
            break;

        // If we are line 1 or all preceding lines are not
        // communicating, make master.
        case 3:
            if (someone_is_connected)
            {
                if (this_line_minus1 == 0)
                    pShm->sys_set.IsysLineSettings.isys_master = 1;
                else
                {
                    for (i = 0; i < this_line_minus1; i++)
                    {
                        // There is a working line with a lower id, let it be the master
                        if ( (!isys_comm_fail[i])                       &&
                             pShm->IsysLineStatus.connected[i]          &&
                             pShm->sys_set.IsysLineSettings.active  [i] &&
                             pShm->sys_set.IsysLineSettings.line_ids[i] )
                            lower_id = true;
                    }

                    // Even though it is not declaired hung, it is leaning that direction
                    for (int hng = 0; hng < MBX_STATUSES; hng++)
                    {
                        if (pShm->mbx_status[i].hung_cnt > 0)
                            mbx_hung = true;
                    }

                }

                // don't do anything yet
                if (mbx_hung)
                {
                    if ( pShm->OpMode != ModeStart )
                    {
                        pShm->OpMode = ModeStart;
                        shm_updates[OPMODE-1] = true;
                    }
                    pShm->sys_set.IsysLineSettings.isys_master = 0;
                }
                else
                    pShm->sys_set.IsysLineSettings.isys_master = (unsigned char) (lower_id ? 0 : 1);

//----- Should be OK to run

                //RtPrintf("mode1 %d config %d\n",pShm->OpMode,config_Ok);
                if ( (pShm->OpMode == ModeStart) && config_Ok && (!mbx_hung))
                {
                    pShm->OpMode          = ModeRun;
                    shm_updates[OPMODE-1] = true;
                }
            }

//----- Can't talk to anyone, stop dropping.

            else
            {
                if (pShm->OpMode == ModeRun)
                {
                    pShm->OpMode          = ModeStart;
                    shm_updates[OPMODE-1] = true;
                }
                pShm->sys_set.IsysLineSettings.isys_master = 0;
            }

//----- Master state change, update host.

            if (master_state != pShm->sys_set.IsysLineSettings.isys_master)
            {
                RtPrintf("Master change from %d to %d\n",
                          master_state, pShm->sys_set.IsysLineSettings.isys_master);

                if (shm_tbl[LSID-1].group != NO_GROUP)
                {
                    fsave_grp_tbl[shm_tbl[LSID-1].group].changed = true;
                    fsave_grp_tbl[shm_tbl[LSID-1].group].loaded  = true;
                }

                //RtPrintf("mode %d config %d\n",pShm->OpMode,config_Ok);
                shm_updates[LSID-1] = true;
            }

			master_state = pShm->sys_set.IsysLineSettings.isys_master;

            // start over
            comm_state = 0;
            break;

        default:
            // start over
            comm_state = 0;
            break;
    }
    return NO_ERRORS;
}

//--------------------------------------------------------
//  Create_Gp_Timer
//--------------------------------------------------------

HANDLE overhead::Create_Gp_Timer()
{
    LARGE_INTEGER  pExp, pInt;

    hGpTimer = RtCreateTimer(
                         NULL,              // Security - NULL is none
                         0,                 // Stack size - 0 is use default
                         Gp_Timer_Main,     // Timer handler
                         (PVOID) 0,         // NULL context (argument to handler)
                         RT_PRIORITY_MIN+50,// Priority
                         CLOCK_FASTEST);    // RTX HAL Timer

    //------------- Start Timer
    // 100ns * 5,000,000 = .5 sec intervals
    pExp.QuadPart = 5000000;
    pInt.QuadPart = 5000000;

     if (!RtSetTimerRelative( hGpTimer, &pExp, &pInt))
     {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCancelTimer(hGpTimer, NULL);
        hGpTimer = NULL;
     }
    else
    {
        RtPrintf("GP Timer created OK (0.5s interval), handle=%p\n", hGpTimer);
#ifdef _SHOW_HANDLES_
        RtPrintf("Hnd %.2X Gp Timer\n", hGpTimer);
#endif

    }
    return (hGpTimer);
}

//--------------------------------------------------------
//   Gp_Timer_Main: Called by RtTimer, this is a 1/2 second
//   timer where all the slow stuff happens
//--------------------------------------------------------

void __stdcall overhead::Gp_Timer_Main(PVOID addr)
{
	addr;	//	RED - To eliminate unreferenced formal parameter warning
    static bool gp_init             = false;
    static bool time_printed        = false;
    static int  tick                = 0;
    static int  IfThreadsNotOkCount = 0;
    char        am_pm[]             = "AM";
    bool        sec1_OK             = 0;  // for delaying stuff
    bool        sec2_OK             = 0;
    bool        sec3_OK             = 0;
    bool        sec4_OK             = 0;
    bool        sec5_OK             = 0;
    bool        sec10_OK            = 0;
    bool        sec15_OK            = 0;
    bool        sec30_OK            = 0;
    time_t      long_time;
    struct tm   *ptime;

    if (!gp_init)
    {
        SetUnhandledExceptionFilter(appGpUnhandledExceptionFilter);
        gp_init = true;
    }

//----- Every hour, print the time for a time reference in the rtx log

    time( &long_time );
    ptime = localtime( &long_time );

    if ((ptime->tm_min == 0) &&
        (ptime->tm_sec <= 1) &&
        (!time_printed) )
    {
        if( ptime->tm_hour > 12 ) strcpy( am_pm, "PM" );
        sprintf(app_dt_str,"%.19s %s ", asctime( ptime ), am_pm );
        RtPrintf("%s\n",app_dt_str);
        time_printed = true;
		if( app->bVersionMismatch == true )
		{
			sprintf(app_err_buf,
				"Mismatched software versions for interface.exe and overhead.rtss\n",
					app->pShm->app_ver, app->pShm->comm_ver);
			app->GenError(warning, app_err_buf);
			app->bVersionMismatch = true;
		}
    }
    else {if (ptime->tm_sec > 1) time_printed = false;}

// Timer is running at .5 seconds right now
// Using various tick values to slow things down

//----- Shortcuts

    if (tick & 1)
    {
        sec1_OK       = true;

        if (app->pShm->sys_stat.PPMTimer != 0)
        {
            sec2_OK   = (((app->pShm->sys_stat.PPMTimer % 2  ) == 0 ) ? true  : false);
            sec3_OK   = (((app->pShm->sys_stat.PPMTimer % 3  ) == 0 ) ? true  : false);
            sec4_OK   = (((app->pShm->sys_stat.PPMTimer % 4  ) == 0 ) ? true  : false);
            sec5_OK   = (((app->pShm->sys_stat.PPMTimer % 5  ) == 0 ) ? true  : false);
            sec10_OK  = (((app->pShm->sys_stat.PPMTimer % 10 ) == 0 ) ? true  : false);
            sec15_OK  = (((app->pShm->sys_stat.PPMTimer % 15 ) == 0 ) ? true  : false);
            sec30_OK  = (((app->pShm->sys_stat.PPMTimer % 30 ) == 0 ) ? true  : false);
        }

        app->this_lineid  =    app->pShm->sys_set.IsysLineSettings.this_line_id;
        app->dual_scale   = (((app->pShm->scl_set.NumScales )      == 2 ) ? true  : false);

//----- Show any change in host comm status
        // fail
        if ( (app->pShm->IsysLineStatus.connected[MAXIISYSLINES] == 0) &&
            (!(app->isys_comm_fail[MAXIISYSLINES])) )
        {
            app->isys_comm_fail[MAXIISYSLINES] = true;
            GetDateStr(app_dt_str);
            RtPrintf("%s host comm fail\n", app_dt_str);
        }
        // ok
        if ( app->pShm->IsysLineStatus.connected[MAXIISYSLINES] &&
             app->isys_comm_fail[MAXIISYSLINES] )
        {
            app->isys_comm_fail[MAXIISYSLINES] = false;
            GetDateStr(app_dt_str);
            RtPrintf("%s host comm normal\n", app_dt_str);
        }
    }

    tick = ~tick;

//----- If debugging, uncomment the desired trace

#ifdef _DEBUGING_

    //  For testing without the interface
    app->pShm->dbg_set.dbg_trace  =
                    //_CHKRNG_   |
                    //_FDROPS_   |
                    //_COMBASIC_ |
                    //_COMQUEUE_ |
                    //_ISCOMS_   |
                    //_DRPMGRQ_  |
                    //_DROPS_    |
                    //_WEIGHT_   |
                    //_SYNCS_    |
                    //_ZEROS_    |
                    //_GRADE_    |
                    //_TRICKLE_  |
                    //_IPC_      |
					//_HBMLDCELL_ |
                    _ALWAYS_ON_ ;
#endif

//----- Shorter name.

    TraceMask = app->pShm->dbg_set.dbg_trace;

//----- Show the interface the backend is alive

    if (app->pShm->AppHeartbeat++ >= 32000)
        app->pShm->AppHeartbeat = 0;

//----- Set/Clear ipc thread event if a problem occurs.

    if(app->pShm->IsysLineStatus.app_threads == ALL_SYSTEMS_READY)
    {
        IfThreadsNotOkCount = 0;

        if(!RtSetEvent(hInterfaceThreadsOk))
            RtPrintf("Error %d file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
    }
    else
    {
        RtResetEvent(hInterfaceThreadsOk);

        if (sec3_OK)
        {
            RtPrintf("Interface threads %X\n", app->pShm->IsysLineStatus.app_threads);

            // If threads do not start, show error slightly before the interface restarts NT.
            // The interface will log the condition in the event log.
            if (++IfThreadsNotOkCount == FATAL_ERR_CNT - 3)
            {
                sprintf(app_err_buf,"Interface threads = %X\n", app->pShm->IsysLineStatus.app_threads);
                app->GenError(warning, app_err_buf);
            }
        }
    }

//----- Update communication statistics.

    if (sec1_OK)
    {

        //RtPrintf("shk2shk_ticks %d\n",app->shk2shk_ticks); //GLC TEMP

        // Update network stats
        app->pShm->IsysLineStatus.pps_sent   = app->pShm->IsysLineStatus.pkts_sent;
        app->pShm->IsysLineStatus.pkts_sent  = 0;
        app->pShm->IsysLineStatus.pps_recvd  = app->pShm->IsysLineStatus.pkts_recvd;
        app->pShm->IsysLineStatus.pkts_recvd = 0;

        // Show messages sent/received
        //RtPrintf("Pckt/sec sent %d recv'd %d\n",
        //          app->pShm->IsysLineStatus.pps_sent,
        //          app->pShm->IsysLineStatus.pps_recvd);
    }

//----- Save any totals which changed

    if (sec5_OK && app->shm_valid ) app->configGroupCheck = true;

//----- Save any settings/status which changed

    if ( sec1_OK && (app->pShm->OpMode == ModeRun) )
    {
        app->shm_updates[STATID-1] = true;
        app->saveTotals       = true;
    }


//----- Send any batch label info.
    if (sec1_OK && HOST_OK)
        app->sendBatchLabel = true;

//----- Send any drop records. If the host is not alive, save

    if ( sec3_OK ) app->sendDrpRecs = true;

//----- Check and send any drop records

    if (sec4_OK)
    {
        // if we are connected to the host
        if ( (HOST_OK && (app->susp_drop_rec_send == 0)) &&
            ((app->sav_drp_rec_file_info.cur_cnt > 0)                    ||
             (app->sav_drp_rec_file_info.nxt2_send != app->sav_drp_rec_file_info.cur_file)) )
             app->sendSavedDrpRecs = true;
    }

//----- Run Mode

    // Update pieces per minute
    if (sec1_OK) app->TimePPM();

    // This is miscellaneous stuff to check in case the interface
    // sends down something invalid (mainly RtPrintf's from failing)
    // or to set something if there is a change, as in the load cell
    // settings.

    if (sec2_OK)
    {
        app->Validate();

#ifndef _WEIGHT_SIMULATION_MODE_ //GLC added 1/24/05
        // These should change only in raw mode, but it will
        // take care of a change after initialization
        app->Check_Adc_Settings(0);
        if (app->dual_scale) app->Check_Adc_Settings(1);
#endif //GLC added 1/24/05
    }

	if (sec5_OK)
	{
#ifndef _WEIGHT_SIMULATION_MODE_

		// Digital Load Cell Specific Checks
		if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_HBM)
		{
			// If debug shell Readloadcell command completes, write file
			if ( app->WriteLCReadsToFile == 0 )
			{
				app->WriteLCReadsToFile = app->pShm->scl_set.NumScales;
				app->HBMLc->WriteLCReadsQ();

				if(app->pShm->scl_set.NumScales == 2)
				{
					app->HBMLc2->WriteLCReadsQ();
				}
			}

		}

#endif
	}

#ifndef _WEIGHT_SIMULATION_MODE_
	// Display the stuck load cell message if required GLC added 12/10/04
	for (int scaleidx = 0; scaleidx < MAXSCALES; scaleidx++)
	{
		if ( (sec30_OK) && (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_HBM) && (app->StuckLoadCellWarning[scaleidx]) )
		{
			app->StuckLoadCellWarning[scaleidx] = false;
			sprintf(app_err_buf, "Load cell readings on scale %d are not changing!\n Check load cell power and data connections, then restart the controller application.\n", scaleidx + 1); //GLC added 12/10/04
			app->GenError(warning, app_err_buf);
		}
	}
#endif

//----- Monitor communications and change to master if needed.

    if (sec1_OK) app->masterCheck = true;

//----- Trigger trace messages in this/other threads.

    if( sec1_OK ) app->sendGpDebug = true;

//----- Process remote batch resets

	if (app->pShm->sys_set.MiscFeatures.EnableBoazMode)
	{
		app->BatchResetStation();
	}

}

//--------------------------------------------------------
//  GpSendThread: This routine was created to send all the
//                messages to prevent delays in main threads.
//                File writes are also done here to prevent
//                multiple threads from trying to call
//                WriteFile at the same time.
//--------------------------------------------------------

#define DREC_BLK_WAIT 5

void __stdcall overhead::GpSendThread(PVOID unused)
{
	unused;	//	RED - To eliminate unreferenced formal parameter warning
    int i;
    static int drecs_block_wait = DREC_BLK_WAIT;

    for (;;)
    {

//----- Check/Send updates to host for all shared memory ids.
        app->UpdateHost();

//----- Diagnostic data from the load cell

        for (i = 0; i < MAXCAPTUREBUFFS; i++)
        {
            if (app->capt_wt.send_capture[i])
            {
                app->UpdateCapture(i);
                memset(&app->capt_wt.capture_weights[i][0], 0, sizeof(__int64) * SAMPLE_WEIGHTS);
                app->capt_wt.send_capture[i] = false;
            }
        }

//----- Miscellaneous file saves. All file writes should be done here to prevent
//      multiple calls to Write_File.

        if (app->saveTotals)
        {
            app->SaveTotals();
            app->saveTotals = false;
        }

        if (app->updateDrecInfo)
        {
            if (Write_File( CREATE_ALWAYS, DREC_INFO_PATH, (BYTE*)
                            &app->sav_drp_rec_file_info,
                            sizeof(app->sav_drp_rec_file_info)) == ERROR_OCCURED)
                RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            app->updateDrecInfo = false;
        }

        // This flag is mostly for unexpected shutdowns and is going to call
        // SaveDropRecords and pass a true for save everything.

        if (app->saveDrpRecs)
        {
            app->SaveDropRecords(true);
            app->saveDrpRecs = false;
		}

//----- Send BPM reset to slaves

        if (app->sendBpmReset)
        {
			//RCB - Put some debug here for multiple lines
            app->SendDrpManager( DROP_MAINT, NULL, NULL, isys_drop_bpm_reset, isys_send_all, NULL );
            app->sendBpmReset = false;
        }

//----- Check comms and determine master status

        if (app->masterCheck)
        {
			app->MasterCtrl();
            app->masterCheck = false;
        }

//----- Check to see if there are any error messages to send

        if (HOST_OK && app->send_error.send)
        {
            app->SendError();
            app->send_error.send = false;
        }

//----- Check to see if there are any needed settings

        if (app->configGroupCheck)
        {
            app->CheckConfig();
            app->configGroupCheck = false;
        }

//----- Send info for batch labels

        if (app->sendBatchLabel)
        {
            app->SendLabelInfo();
            app->SendPreLabel();
            app->SendSlavePreLabel();
            app->sendBatchLabel = false;
        }

//----- Send any drop records. If the host is not alive, save

        // Get a count. the host may be connected, but not taking any records.
        // compare the total to a high water mark and save to a file if exceeded.
        app->drp_rec_count = 0;

        for (int i = 0; i < MAXDROPRECS; i++)
        {
            if (app->pShm->HostDropRec[i].flags == PCRECUSED)
                app->drp_rec_count++;
        }

        // If time to send
        if ( app->sendDrpRecs )
        {
            if (app->pShm->OpMode == ModeRun)
            {
                if ( HOST_OK                       &&
                     (app->susp_drop_rec_send == 0) &&
                     (app->drp_rec_count <= DROPRECHISP) )
                    app->SendDropRecords();
                else
                {
                    RtPrintf("SaveDrpRecs: HOST_OK=%d susp=%d cnt=%d hisp=%d\n",
                             HOST_OK ? 1 : 0, app->susp_drop_rec_send & 1,
                             app->drp_rec_count, DROPRECHISP);
                    app->SaveDropRecords(false);
                }

                app->sendDrpRecs = false;
            }
        }

//----- Send a block send of saved drop records

        if ( app->sendSavedDrpRecs )
        {
            app->sendSavedDrpRecs = false;

            // ask host if it's ok to send
            if ( !app->sendBlockRequested )
            {
                app->RequestToSendDropRecordBlock();
                app->sendBlockRequested = true;
            }

            // send granted by host
            if (app->sendBlockGranted)
            {
                app->SendSavedDropRecords();
                app->sendBlockRequested = false;
                app->sendBlockGranted   = false;
                drecs_block_wait        = DREC_BLK_WAIT;
            }

            // ask host again if no response received
            if ( app->sendBlockRequested && (!app->sendBlockGranted) )
            {
                if (--drecs_block_wait <= 0)
                {
                     app->sendBlockRequested = false;
                     app->sendBlockGranted   = false;
                     drecs_block_wait        = DREC_BLK_WAIT;
                }
            }
        }

//----- Send any debug info

        if ( app->sendGpDebug )
        {
            app->GpDebug();
            app->sendGpDebug = false;
        }

        Sleep(5);
    }
}
//--------------------------------------------------------
//  Create_Fast_Update
//--------------------------------------------------------

HANDLE overhead::Create_Fast_Update()
{
    LARGE_INTEGER  pExp, pInt;

    hFstTimer = RtCreateTimer(
                         NULL,              // Security - NULL is none
                         0,                 // Stack size - 0 is use default
                         Fast_Update,       // Timer handler
                         (PVOID) 0,         // NULL context (argument to handler)
                         RT_PRIORITY_MIN+52,// Priority
                         CLOCK_FASTEST);    // RTX HAL Timer

    //------------- Start Timer
    // 100ns * 2,000,000 = .2 sec intervals
    pExp.QuadPart = 2000000;
    pInt.QuadPart = 2000000;

     if (!RtSetTimerRelative( hFstTimer, &pExp, &pInt))
     {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCancelTimer(hFstTimer, NULL);
        hGpTimer = NULL;
     }
    else
    {

#ifdef _SHOW_HANDLES_
        RtPrintf("Hnd %.2X Fst Timer\n", hFstTimer);
#endif

    }
    return (hGpTimer);
}

//--------------------------------------------------------
//   Fast_Update - For only the most time critical sends
//--------------------------------------------------------

void __stdcall overhead::Fast_Update (PVOID addr)
{
	addr;	//	RED - To eliminate unreferenced formal parameter warning
	TDisplayShackle* pDisplayData;

    // display data
    if (app->shm_updates[DISP_DATA-1])
    {
        app->shm_updates[DISP_DATA-1] = false;

        // Request Mutex for send
        if(RtWaitForSingleObject(trc[FASTBUFID].mutex, WAIT200MS) != WAIT_OBJECT_0)
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        else
        {
            if HOST_OK
			{
                app->SendHostMsg( SHM_WRITE, DISP_DATA,
                             (BYTE*) app->shm_tbl[DISP_DATA-1].data,
                             app->shm_tbl[DISP_DATA-1].struct_len);

				pDisplayData = (TDisplayShackle*)app->shm_tbl[DISP_DATA-1].data;
				//DebugTrace(_DEBUG_,
				//	"UpdateHost:DisplayData:\tGradeIndex\t%d\tshackle1\t%d\tdrop1\t%d"
				//	"\tshackle2\t%d\tdrop2\t%d\n",
				//	pDisplayData->GradeIndex[0],
				//	pDisplayData->GradeIndex[1],
				//	pDisplayData->shackle[0],
				//	pDisplayData->drop[0],
				//	pDisplayData->shackle[1],
				//	pDisplayData->drop[1]);
			}


            if(!RtReleaseMutex(trc[FASTBUFID].mutex))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }
    }
}

//--------------------------------------------------------
//  Create_App_Timer
//--------------------------------------------------------

HANDLE overhead::Create_App_Timer()
{
    LARGE_INTEGER  pExp, pInt;

    hAppTimer = RtCreateTimer(
                     NULL,              // Security - NULL is none
                     0,                 // Stack size - 0 is use default
                     App_Timer_Main,     // Timer handler
                     (PVOID) 0,         // NULL context (argument to handler)
                     RT_PRIORITY_MIN+50,// Priority
                     CLOCK_FASTEST);    // RTX HAL Timer

    //------------- Start Timer
    // 100ns * 50000 = 5ms intervals
    pExp.QuadPart = 50000;
    pInt.QuadPart = 50000;

    if (!RtSetTimerRelative( hAppTimer, &pExp, &pInt))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCancelTimer(hAppTimer, NULL);
        hAppTimer = NULL;
    }
    else
    {

#ifdef _SHOW_HANDLES_
        RtPrintf("Hnd %.2X Overhead Timer\n", hAppTimer);
#endif

    }

    return (hAppTimer);
}

//--------------------------------------------------------
//   App_Timer_Main: Called by RtTimer, this is where all
//                  the main events happen
//--------------------------------------------------------

void __stdcall overhead::App_Timer_Main(PVOID addr)
{
	addr;	//	RED - To eliminate unreferenced formal parameter warning
    static bool app_init = false;

	int         i;
	//static int	ToggleScale = 1;

    if (!app_init)
    {
        SetUnhandledExceptionFilter(appUnhandledExceptionFilter);
        app_init = true;
    }

    // Figure out how many ticks between shackles 1 & 2. It is used
    // to stop the weigh average routine in time to calculate average
    // weight before the next shackle.
    if ( app->calc_shk2shk_ticks )
        app->cur_shk2shk_ticks++;

    // Read inputs
    if (!app->simulation_mode)
    {
        iop->readInputs();  // three primary input ports

        #ifdef _VSBC6_IO_
           #ifdef VSBC6_LOWIN_HIOUT
				
				if (BoardIsEPM15)
				{
					iop_3->readInputs();
				}
				// TG - Adding EPM19 Support
				else if (BoardIsEPM19)
				{
					// TG - TODO Copying EPM15 for now
					iop_4->readInputs();
				}
				else
				{
	                ios->readInputs(); // one secondary input port
				}
           #endif
        #endif

		if (app->Second_3724_Required)
		{
			iop_2->readInputs();	//	read input registers from second 3724 card

			//	Process Remote Batch Resets
		
			//	For all the inputs
			for (i = 0; i < app->Second_3724_No_of_Inputs; i++)
			{
				int DropMode = app->pShm->sys_set.DropSettings[app->Second_3724_Drops[i] - 1].DropMode;
				//	If input is turned on
				if (app->Second_3724_Input[i])
				{
					//	If input is for a Remote Batch Reset Drop
					if (DropMode == DM_REMOTERESET)
					{
						//	Only do reset if drop is currently batched and last bird has arrived
						if ((app->pShm->sys_stat.DropStatus[app->Second_3724_Drops[i] - 1].Batched == 1) &&
							(app->last_in_batch_dropped[app->Second_3724_Drops[i] - 1]))
						{
							//	Reset the drop from Batched Mode
							CLEAR_DROP_BATCH(app->Second_3724_Drops[i] - 1, app->pShm)
							CLEAR_ISYS_DROP_BATCH(app->Second_3724_Drops[i] - 1, app->pShm);

							//	If this is InterSystem drop, notify other lines this has happened
							if (ISYS_ENABLE && ISYS_DROP(app->Second_3724_Drops[i]-1, app->pShm))
							{
								for (int j = 0; j < MAXIISYSLINES; j++)
								{
									if (((j+1) != app->this_lineid) &&
										(app->pShm->sys_set.IsysDropSettings[app->Second_3724_Drops[i] - 1].active[j]))
									{
										int line = j+1;
										int drop = app->pShm->sys_set.IsysDropSettings[app->Second_3724_Drops[i] - 1].drop[j];
										app->SendLineMsg(REMOTE_BATCH_RESET, line , NULL, (BYTE*) &drop, sizeof(drop));
									}
								}
							}
						}
					}
					if ((DropMode == DM_REASSIGN) && (app->Second_3724_DropDisabled[i] == true) && (app->Second_3724_DropCounter[i]++ > 0))
					{
						app->Second_3724_DropDisabled[i] = false;
					}
				}
				else
				{
					if (DropMode == DM_REASSIGN)
					{
						if (app->Second_3724_DropDisabled[i] == true)
						{
							if (app->Second_3724_DropCounter[i]++ > app->pShm->sys_set.ReassignTimerMax)
							{
								// Remove bird from books
								app->SubtractBird(app->pShm->ShackleStatus[app->Second_3724_Shackle[i]].GradeIndex[app->Second_3724_Scale[i]],
											 app->Second_3724_Drops[i],
											 app->pShm->ShackleStatus[app->Second_3724_Shackle[i]].weight[app->Second_3724_Scale[i]]);

								// Remove bird from BPM count for scale
								app->bpm_act_count[app->Second_3724_Scale[i]]--;

								// Unassign shackle from drop
								app->pShm->ShackleStatus[app->Second_3724_Shackle[i]].drop[app->Second_3724_Scale[i]] = 0;

								// Call FindDrop with current drop as base to get bird reassigned further down the line
								app->FindDrops(app->Second_3724_Scale[i]+1, app->Second_3724_Drops[i], app->Second_3724_Shackle[i]);

								//	Re-enable drop
								app->Second_3724_DropDisabled[i] = false;
							}
						}
					}
				}
			}
		}
    }
	
    // Decrement output counters and turn off output
    app->DecCntrlCtrs();

    //grade sync and count error handling
    if (app->pShm->sys_set.Grading) app->GradeSyncs();

    //  Main sync processing routine
    //  Keep ProcessSyncs before switch statement
    app->ProcessSyncs();

    switch(app->pShm->OpMode)
    {
        case ModeStart:
            break;

        case ModeRun:
        case ModeATare1:
        case ModeATare2:
        case ModeATare3:
        case ModeASpan1:
        case ModeASpan2:
            app->ProcessWeight();
            break;

        case ModeRaw:
            app->RawMode();
            // Set these to false. ProcessWeight normally sets it to idle.
            // Left over active will cause invalid read.
            app->weigh_state[0] = WeighIdle;
            app->weigh_state[1] = WeighIdle;
            break;

        default:
            RtPrintf("Error OpMode %d file %s, line %d \n",
                      app->pShm->OpMode, _FILE_, __LINE__);
            break;
    }

    // ok to send oh main timer trace
    if ( trc[MAINBUFID].send_OK )
    {
        // Request Mutex for send
        if(RtWaitForSingleObject(trc[MAINBUFID].mutex, WAIT100MS) != WAIT_OBJECT_0)
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        else
        {

// Send debug to self
#ifdef _REDIRECT_DEBUG_
            app->SendLineMsg( DEBUG_MSG, app->this_lineid, NULL,
                              (BYTE*) &trc_buf[MAINBUFID],
                              trc[MAINBUFID].len);
#endif

#ifdef _LOCAL_DEBUG_
			if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
			{
#ifdef	_TELNET_UDP_ENABLED_
				UdpTraceNBuf((char*) &trc_buf[MAINBUFID], trc[MAINBUFID].len);
#else
				PrintNBuf((char*) &trc_buf[MAINBUFID], trc[MAINBUFID].len);
#endif
			}
			else
			{
				PrintNBuf((char*) &trc_buf[MAINBUFID], trc[MAINBUFID].len);
			}
#endif

#ifdef	_HOST_DEBUG_
            if HOST_OK
			{
                app->SendHostMsg( DEBUG_MSG, NULL,
                                  (BYTE*) &trc_buf[MAINBUFID],
                                  trc[MAINBUFID].len);
			}
#endif

            if(!RtReleaseMutex(trc[MAINBUFID].mutex))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }

        // clear old message.
        memset(&trc_buf[MAINBUFID], 0, sizeof(trcbuf));
        sprintf( (char*) &trc_buf[MAINBUFID],"L%d:%s\n",
                 app->this_lineid,
                 &trc_buf_desc[MAINBUFID]);

        trc[MAINBUFID].send_OK = false;
    }

//----- Weight capture/average stuff

    if ( app->capture_period > 0)
        app->capture_period--;

	if (!app->weight_simulation_mode)
		app->CaptureLcData();

	for (i = 0; i < MAXSCALES; i++)
    {
        // If decrement wt avg trigger counters
        if (( app->w_avg[i].avg_trigger_cntr > 0 ) && (app->pShm->OpMode != ModeRaw))
        {

			app->w_avg[i].avg_trigger_cntr--;

            if (!app->weight_simulation_mode)
			{
				app->AverageWeight(i);
			}
            else
            {
                WEIGH_SHACKLE(i,app->pShm).weight[i] = sim->SimWeight();
                app->w_avg[i].valid_wt      = true;
                app->weigh_state[i]         = WeighDone;
            }
       }
    }
	//	Write outputs for Remote Batch Reset drops after all processing is done
	if (app->Second_3724_No_of_Outputs > 0)
	{
		bool btemp;
		for (int i = 0; i < app->Second_3724_No_of_Outputs; i++)
		{
			btemp = ((app->pShm->sys_stat.DropStatus[app->Second_3724_Drops[i] - 1].Batched != 0) &&
					 (app->last_in_batch_dropped[app->Second_3724_Drops[i] - 1]))? true : false;
			if (btemp != app->Second_3724_Output[i])
			{
				app->Second_3724_WriteOutput = true;
				app->Second_3724_Output[i] = btemp;
			}
		}

		iop_2->writeOutputs();
	}
}

//--------------------------------------------------------
//  AverageWeight
//--------------------------------------------------------

void overhead::AverageWeight(int scale)
{
    int     i;
    int     tossed  = 0;
    int     cnt     = 0;
    int     tmp_avg = 0;
    int     sample_start = 0;
    int     sample_end = 0;
    __int64 wt = 0;		// RED - Initialized to eliminate initialization warning
	static __int64	PrevRawWeight[MAXSCALES] = {0, 0};
	static	int		LCStuckCounter[MAXSCALES] = {0, 0};


//----- The object of all this is to mark the highest points at leading and falling
//      edges of product. These marks are used to calculate an average weight.

    switch(w_avg[scale].step)
    {


//----- Read weights based on counter window

        case read_window:
            //RtPrintf("Ticks %d ShmStart %d ShmEnd %d\n", shk2shk_ticks, pShm->scl_set.LC_Sample_Adj[scale].start, pShm->scl_set.LC_Sample_Adj[scale].end);
            sample_start = (shk2shk_ticks - (int)((shk2shk_ticks * app->pShm->scl_set.LC_Sample_Adj[scale].start) / 100)); //GLC
            sample_end = (sample_start - (int)((sample_start * app->pShm->scl_set.LC_Sample_Adj[scale].end) / 100)); //GLC
            //RtPrintf("AvgWt\tStep\t%d\tStartIdx\t%d\tEndIdx\t%d\n",w_avg[scale].step, sample_start, sample_end);
            //AVG_START shk2shk_ticks - (shk2shk_ticks/3)
            //AVG_END     (AVG_START)/2
            if ( (w_avg[scale].avg_trigger_cntr < sample_start) &&
                 (w_avg[scale].avg_trigger_cntr >= sample_end) )
            {
               if ( w_avg[scale].curr_index < AVG_WEIGHTS )
               {
                    // Request Mutex for reading/writing
                    if(RtWaitForSingleObject(load_cell_mutex[MAINBUFID], WAIT50MS) != WAIT_OBJECT_0)
                        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    else
                    {
						if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
						{
	                        wt = app->ld_cel[0]->read_load_cell(scale);
						}
						else
						{

							wt = app->ld_cel[scale]->read_load_cell(0);

							// GLC 12/13/2004
							// Check to see if digital load cell readings have stalled
							// Need to warn user to check loadcell power and reset backend

							if (PrevRawWeight[scale] == wt)
							{
								LCStuckCounter[scale]++;
								if (LCStuckCounter[scale] > MAXLCSTUCKCOUNTRUN)
								{
									StuckLoadCellWarning[scale] = true;
									LCStuckCounter[scale] = 0;
								}
							}
							else
							{
								StuckLoadCellWarning[scale] = false;
								LCStuckCounter[scale] = 0;
							}

							PrevRawWeight[scale] = wt;

						}

                        INDIV_READS(scale, FUNC_AVG, wt)

                        if(!RtReleaseMutex(load_cell_mutex[MAINBUFID]))
                            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    }

                    wt = w_avg[scale].weights[w_avg[scale].curr_index];
                    w_avg[scale].curr_index++;

                    // Update capture info
                    if ( (capt_wt.mode  == prod_capt) && (capt_wt.scale == scale + 1) )
                    {
                        w_avg[scale].capt_hold = true;
                        CAPTURE_WT = wt;
                        capt_wt.curr_index++;
                    }

                    // Just started, apply first mark
                    if ( w_avg[scale].mrk1_idx == 0 )
                    {
                        w_avg[scale].mrk1_idx = w_avg[scale].curr_index;
                        w_avg[scale].cap1_idx = capt_wt.curr_index - READ_INDEXES;
                        w_avg[scale].wt1      = wt;

                        //apply the first vertical line in the waveform
						if ( (capt_wt.mode  == prod_capt) &&
                             (capt_wt.scale == scale + 1) )
                            capt_wt.capture_weights[capt_wt.curr_buf][0] = capt_wt.curr_index - READ_INDEXES;

                        if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WTAVG_ ) )
                        {
                            sprintf((char*) &tmp_trc_buf[MAINBUFID],"AvgWt\t%d\tPrdCtr\t%d\tmrk1\t%d\twt\t%d\tcap1\t%d\n",
                                            w_avg[scale].step,     w_avg[scale].avg_trigger_cntr,
                                            w_avg[scale].mrk1_idx, w_avg[scale].wt1,
                                            capt_wt.curr_index);
                            strcat( (char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
                        }
                    }
               }
            }

            // Done
            if ( w_avg[scale].avg_trigger_cntr <= sample_end )
            {
                w_avg[scale].mrk2_idx         = w_avg[scale].curr_index - 1;
                w_avg[scale].cap2_idx         = capt_wt.curr_index - READ_INDEXES;
                w_avg[scale].wt2              = wt;

                // Update capture info
				if ( (capt_wt.mode  == prod_capt) && (capt_wt.scale == scale + 1) )
                {
                    // apply the second vertical line in the waveform
                    capt_wt.capture_weights[capt_wt.curr_buf][1] = capt_wt.curr_index - READ_INDEXES;
                    w_avg[scale].capt_hold = false;
                }

                if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WTAVG_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[MAINBUFID],"AvgWt\t%d\tPrdCtr\t%d\tmrk2\t%d\twt\t%d\tcap2\t%d\n",
                                    w_avg[scale].step,     w_avg[scale].avg_trigger_cntr,
                                    w_avg[scale].mrk2_idx, w_avg[scale].wt2,
                                    capt_wt.curr_index);
                    strcat( (char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
                }

                w_avg[scale].avg_trigger_cntr = 1;
                w_avg[scale].step             = compute_average;
            }

            break;


//----- Get an average weight from max1 to max2 index.

        case compute_average:

            w_avg[scale].avg = 0;
            tmp_avg          = 0;
            cnt              = 0;

            // Add all weights for the average
            for( i = w_avg[scale].mrk1_idx; i <= w_avg[scale].mrk2_idx; i++)
            {
                w_avg[scale].avg += w_avg[scale].weights[i];
                cnt++;
            }

            // Calculate average weight
            if (cnt > 0)
			{
				tmp_avg = (int) w_avg[scale].avg / cnt;
			}

            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WTAVG_ ) )
            {
                sprintf((char*) &tmp_trc_buf[MAINBUFID],"AvgWt\t%d\tavg\t%d\tcnt\t%d\n",
                                w_avg[scale].step, tmp_avg, cnt);
                strcat( (char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
            }

            w_avg[scale].step = reaverage_using_limits;

            // Do the rest of the steps without leaving.
            //break;

//----- Re-average, using the previous temporary average and limits to
//      throw away some which appear abnormal.

        case reaverage_using_limits:

            if (tmp_avg > 0)
            {
                w_avg[scale].avg = 0;
                cnt              = 0;
                tossed           = 0;

                for( i = w_avg[scale].mrk1_idx; i <= w_avg[scale].mrk2_idx; i++)
                {
                    if ( (w_avg[scale].weights[i] >= (tmp_avg - (individual_lim))) /*&&
                         (w_avg[scale].weights[i] <= (tmp_avg + individual_lim)) */)
                    {
                        w_avg[scale].avg += w_avg[scale].weights[i];
                        cnt++;
                    }
                    else tossed++;
                }
            }

            // Calculate a new average without bogus readings
            if (cnt > 0)
			{
                w_avg[scale].weight = (__int64) (w_avg[scale].avg / cnt);
			}
            else
            {
                DebugTrace(_WTAVG_, "AvgWt\t%d\tDidn't work\n",w_avg[scale].step);
                w_avg[scale].weight = 0;
            }

            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WTAVG_ ) )
            {
                sprintf((char*) &tmp_trc_buf[MAINBUFID],"AvgWt\t%d\tavg\t%d\tcnt\t%d\ttossed\t%d\n",
                                w_avg[scale].step, w_avg[scale].weight, cnt, tossed);
                strcat( (char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
            }

            WEIGH_SHACKLE(scale, pShm).weight[scale] = w_avg[scale].weight;
            w_avg[scale].valid_wt              = true;
            w_avg[scale].step                  = wt_average_finish;
            weigh_state[scale]                 = WeighDone;

            // Do the rest of the steps without leaving.
            //break;


//----- Wait here. When product counter = -1, average data is cleared.

        case wt_average_finish:
            break;

        default:
            RtPrintf("AvgWt bad case file %s, line %d \n", _FILE_, __LINE__);
            memset(&w_avg, 0, sizeof(w_avg));
            break;

    }
}

//--------------------------------------------------------
//  CaptureLcData
//--------------------------------------------------------

void overhead::CaptureLcData()
{
    static int cap_slowdown = CAPTURE_SPEED;

//----- Reset variables if mode changed to no capture.

    if (capt_wt.mode == no_capt)
    {
        if (capt_wt.curr_index != 0) memset(&capt_wt, 0, sizeof(t_capture_info));

        return;
    }

//----- Reading weights, slow down because the load cell card can't convert as
//      fast as the app timer.

    if (--cap_slowdown > 0) return;
    else  cap_slowdown = CAPTURE_SPEED;


    switch (capt_wt.mode)
    {
        case prod_capt:

            // Must be unitialized if curr idx < READ_INDEXES.
            if (capt_wt.curr_index < READ_INDEXES)
                RESET_PRODUCT_CAPTURE

            // Need a little more time
            //if ( ( w_avg[capt_wt.scale].step > 0) &&
            //     (!w_avg[capt_wt.scale].valid_wt) &&
            //     ( capture_period == 0) )
            //    capture_period++;

            if ( (capt_wt.curr_index     <  SAMPLE_WEIGHTS) &&
                 (capture_period         >  0)              &&
                 (w_avg[capt_wt.scale - 1].capt_hold == false) )
                LC_CAPTURE(capt_wt.scale - 1)

            if ( (capture_period == 0) || (capt_wt.curr_index >= (SAMPLE_WEIGHTS - 1)) )
            {
                // send buffer
                LC_SEND(capt_wt.curr_index)
                RESET_PRODUCT_CAPTURE
            }

            break;

        case cont_capt:
            if ( capt_wt.curr_index < SAMPLE_WEIGHTS )
                LC_CAPTURE(capt_wt.scale - 1)
            else
            {
                LC_SEND(capt_wt.curr_index)
                capt_wt.curr_index = 0;
            }

            break;

        default:
            RtPrintf("Error bad capture mode file %s, line %d \n",_FILE_, __LINE__);
            memset(&capt_wt, 0, sizeof(t_capture_info));
            break;
    }
}

//--------------------------------------------------------
//  ProcessMbxMsg
//
//  If a message requires a response, the data is copied
//  to temporary storage, an event is set and the event
//  handler sends the reply.
//
//--------------------------------------------------------

void overhead::ProcessMbxMsg()
{
    static TAppIpcMsg* MbMsg = (TAppIpcMsg*) &pAppIpcMsg->Buffer;
    int    i;

    // validate message
    if ( (MbMsg->gen.hdr.cmd <= APP_MSG_START) ||
         (MbMsg->gen.hdr.cmd >= LAST_APP_MSG) )
    {
        RtPrintf("App\tMbx\tbad cmd\t%d\n",
                  MbMsg->gen.hdr.cmd);
        return;
    }

    //RtPrintf("App\tMbx\t[%s]\n", app_msg[MbMsg->gen.hdr.cmd - APP_MSG_START]);

    switch(MbMsg->gen.hdr.cmd)
    {
        case SHM_READ:
            {
                int* pid = (int*) &to_be_processed;

                // check to see if ok to use the to_be_processed buffer
                if(RtWaitForSingleObject(hEvtHndlr,WAIT50MS)==WAIT_FAILED)
                {
                    RtPrintf("Error %d file %s, line %d \n",GetLastError(),_FILE_, __LINE__);
                    return;
                }
                else
                {
                    *pid = (int) MbMsg->rw_shm.shm_id;

                    // Never allow SHKSTAT to be read, it is too big for pipe
                    // buffer and there is no need to read it.
                    if ( (*pid >= 1) && (*pid <= ALL_SHM_IDS) && (*pid != SHKSTAT))
                    {
                        if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
                        {
                            sprintf((char*) &tmp_trc_buf[EVTBUFID],"SHM_READ\tid\t%d Ok\n",*pid);
                            strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
                        }

                        if(!RtSetEvent(process_event[EVT_SHM_READ]))
                            RtPrintf("Error %u file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
                    }
                    else
                        RtPrintf("Error id %d file %s, line %d \n",
                                  *pid, _FILE_, __LINE__);
                }
            }
            break;

        case SHM_WRITE:
            {
                int id  = MbMsg->rw_shm.shm_id;
                int len = MbMsg->rw_shm.hdr.len - sizeof(MbMsg->rw_shm.shm_id);

				sys_in* pMbMsgData = (sys_in *)&MbMsg->rw_shm.data;

                // When testing, the host path can be set to local path. Ignore
                // write if source id is not 5.
                if (MbMsg->rw_shm.hdr.srcLineId != 5)
                    return;

				//	if VALID_IDS
                if ( (((id <= ALL_SHM_IDS) && (id >= 1))           &&
                    (!((id >= FIRST_STAT)  && (id <= LAST_STAT) )) &&
                    (!((id >= ZERO_CTR)    && (id <= GRD_SHKL) ))) )
                {
                    id--;

                   if (len == shm_tbl[id].struct_len)
                    {
						// Check to make sure that the SYS_IN block is not all zeros (check that line id is nonzero)
						if ((id == (SYS_SET - 1)) && (pMbMsgData->IsysLineSettings.this_line_id == 0))
						{
							RtPrintf("ProcessMbxMsg SHM_WRITE this_line_id = 0 *write aborted*\n", pMbMsgData->IsysLineSettings.this_line_id);
							return;

						}

//                                    sys_in* ss = (sys_in*) &MbMsg->rw_shm.data;	RED - Removed because initialized but not referenced

                        if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
                        {
                            sprintf((char*) &tmp_trc_buf[EVTBUFID],"SHM_WRITE\tid\t%d Ok\n",id+1);
                            strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
                        }

                        // copy the data
                        memcpy(shm_tbl[id].data, &MbMsg->rw_shm.data, shm_tbl[id].struct_len);

                        // flag it changed so gp timer thread will save the data to a file
                        if (shm_tbl[id].group != NO_GROUP)
                        {
                            fsave_grp_tbl[shm_tbl[id].group].changed = true;
                            fsave_grp_tbl[shm_tbl[id].group].loaded  = true;
                        }

						switch (id+1)
						{
						case DROP_SET:
                           // If switched to no grading
							//	Re-analyze drops in sys_set to see if Reassignment or Remote Batch Reset
							//	parameters have changed enough to effect the initialization of the 
							//	second 3724 card and, if they have,  re-initialize the card
							
							{
								//	Re-analyze the drops and their requirements for the 2nd 3724 card
								AnalyzeConfig(false);
							}
							break;
						case SYS_SET:
                            if (!pShm->sys_set.Grading)
							{
								grade_zeroed[0] = true;

								// Hard coded for now. Should not need more than
								// 2 grade syncs
								grade_zeroed[1] = true;
							}

                            //  If the application was down and the totals were cleared on the host while
                            //  the line was down, this flag will be set. Clear local totals so the
                            //  batches and totals won't look bad on the host.

                            if ( pShm->sys_set.IsysLineSettings.reset_totals[app->this_lineid-1] )
                            {
                                RtPrintf("CLRTOTS (reset totals flag set)\n");
                                CLEAR_TOTALS
                            }
							break;
						default:
							break;
						}
                    }
                    else
                    {
                        RtPrintf("Bad length id %d file %s, line %d \n", id+1, _FILE_, __LINE__);

                        // Request Mutex for send
                        if(RtWaitForSingleObject(trc[EVTBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                        else
                        {
                            sprintf(app_err_buf,"Write Shm id %d bad length.\n",id+1);

                            if HOST_OK
                                SendHostMsg( ERROR_MSG, 2,
                                            (BYTE*) app_err_buf,
                                             strlen(app_err_buf));

                            if(!RtReleaseMutex(trc[EVTBUFID].mutex))
                                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                        }
                    }
                }
                else
                {
                    RtPrintf("SHM_WRITE Error !!! id %d %s, line %d \n", id, _FILE_, __LINE__);
                    DumpData("shm_id",(BYTE *)&MbMsg->rw_shm.shm_id, 4);
                }
            }
            break;

        case DREC_RESP:
            {
                int drsp_cnt;
                ev_app_drp_resp* p2bp;

                // the number of sequence numbers to process
                drsp_cnt  = MbMsg->drpResp.hdr.len / sizeof(int);

                if ( (drsp_cnt < 1) || (drsp_cnt > MAXDROPRECS) )
                {
                    RtPrintf("Error count %u file %s, line %d \n",
                             drsp_cnt, _FILE_, __LINE__);
                    return;
                }

                // build a simple format for event handler
                p2bp      = (ev_app_drp_resp*) &to_be_processed;

                // check to see if ok to use the to_be_processed buffer
                if(RtWaitForSingleObject(hEvtHndlr,WAIT50MS)==WAIT_FAILED)
                {
                    RtPrintf("Error %d file %s, line %d \n",GetLastError(),_FILE_, __LINE__);
                    return;
                }
                else
                {
                    p2bp->cnt = drsp_cnt;

                    //save the sequence numbers for reply message
                    for( i = 0; i < drsp_cnt; i++)
                        p2bp->seq_num[i] = MbMsg->drpResp.seq_num[i];

                    // set event for processing
                    if(!RtSetEvent(process_event[EVT_DREC_RESP]))
                    {
                        RtPrintf("Error %d file %s line %d\n", GetLastError(), _FILE_, __LINE__);
                        return;
                    }

                    // if a large chunk of drop records saved while the host
                    // was down is sent, the host may set a flag to pause
                    // drop records until it is ready for another chunk.
                    //	CHECK_PAUSE(__LINE__)
					{
						int* pflg = (int*) &MbMsg->rw_shm.hdr.flags;
						susp_drop_rec_send = (*pflg ? true : false);
						if (last_drop_rec_susp != susp_drop_rec_send)
						{
							RtPrintf("line %d pause changed from %d to %d\n",
								__LINE__, last_drop_rec_susp & 1, susp_drop_rec_send & 1);
							last_drop_rec_susp = susp_drop_rec_send;
						}
					}
                }
            }
            break;

        case ACK_MSG:

#ifdef _SHMW_TIMING_TEST_

            if (shmw_seq_done == MbMsg->rw_shm.hdr.seq_num)
            {
                RtGetClockTime(CLOCK_FASTEST,&shmw_EndTime);
                RtPrintf("SHMW\tresponse\ttime\t%u ms\n",
                         (UINT) ((shmw_EndTime.QuadPart - shmw_StartTime.QuadPart)/10000));
                shmw_seq_done = 0;
            }
#endif
            break;

        // This is a message to give ok to send a large block of drop records.
        // Nothing but a header is needed.
        case BLK_DREC_RESP:

            if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
            {
                sprintf((char*) &tmp_trc_buf[EVTBUFID],"Recv DREC Block Grant\n");
                strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
            }
            sendBlockGranted = true;

            break;

        // GUI received the new batch number
        case PRN_BCH_RESP:
            {
                UINT* pseq_num = (UINT*) &MbMsg->gen.data;

                for (int idx = 0; idx < MAXBCHLABELS; idx++)
                {
                    if (*pseq_num == batch_label.info[idx].seq_num)
                    {
                        batch_label.info[idx].label_step = 1;

                        if( (!trc[EVTBUFID].buffer_full) && (TraceMask & (_HOST_COMS_ | _LABELS_)) )
                        {
                            sprintf((char*) &tmp_trc_buf[EVTBUFID],"Mbx\tBchRsp\tbch#\t%d\tindx\t%d\n",
                                     batch_label.info[idx].seq_num & BATCH_MASK, idx);
                            strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
                        }

                        // clear the record, if both finished
                        CLEAR_BCH_INFO(idx)
                        break;
                    }
                }
            }
            break;

        // GUI received the new pre batch msg.
        case PRN_BCH_PRE_RSP:
            {
                UINT* pseq_num = (UINT*) &MbMsg->gen.data;

                for (int idx = 0; idx < MAXBCHLABELS; idx++)
                {
                    if (*pseq_num == batch_label.info[idx].seq_num)
                    {
                        batch_label.info[idx].pre_label_step = 2;

                        if( (!trc[EVTBUFID].buffer_full) && (TraceMask & (_HOST_COMS_ | _LABELS_)) )
                        {
                            sprintf((char*) &tmp_trc_buf[EVTBUFID],"Mbx\tPreRsp\tbch#\t%d\tindx\t%d\n",
                                     batch_label.info[idx].seq_num & BATCH_MASK, idx);
                            strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
                        }

                        // clear the record, if both finished
                        CLEAR_BCH_INFO(idx)
                        break;
                    }
                }
            }
            break;

        // slave detected last bird at intersystems drop, set pre label step.
        case SLAVE_PRE_LABEL:
            {
                UINT* pseq_num = (UINT*) &MbMsg->gen.data;

                if( (!trc[GPBUFID].buffer_full) && (TraceMask & (_HOST_COMS_ | _LABELS_)) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"SLAVE_PRE_LABEL %d\n", *pseq_num & BATCH_MASK);
                    strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
                }

                for (int idx = 0; idx < MAXBCHLABELS; idx++)
                {
                    if (*pseq_num == batch_label.info[idx].seq_num)
                    {
                        batch_label.info[idx].pre_label_step = 1;
                        break;
                    }
                }
            }
            break;

        // clear totals
        case CLEAR_TOTS:
            if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
            {
                sprintf((char*) &tmp_trc_buf[EVTBUFID],"CLEAR_TOTALS\n");
                strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
            }
            CLEAR_TOTALS
            GetDateStr(app_dt_str); //GLC added 2/15/05
            RtPrintf("Totals Cleared %s\n", app_dt_str); //GLC added 2/15/05
            break;

        // These are actually done in the interface.exe app. It is shown
        // here so it will be seen. The win32 library can't be used in
        // an rtss app.
        case SET_DATE_TIME:
        case RESTART_APP:
        case RESTART_NT:
        case SHUTDOWN_NT:
            break;

        // Reset batch numbers
        case RESET_BCH_NUMS:
            if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
            {
                sprintf((char*) &tmp_trc_buf[EVTBUFID],"RESET_BCH_NUMS\n");
                strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
            }
            sav_drp_rec_file_info.nxt_lbl_seqnum = 1;
            break;

        case IND_MODE_WT_LIM:
            {
                UINT* lim = (UINT*) &MbMsg->gen.data;
                RtPrintf("IND_MODE_WT_LIM %d\n",*lim);
                individual_lim = *lim;
            }
            break;

        case AUTO_BIAS_MODE:
            {
                UINT* md = (UINT*) &MbMsg->gen.data;
                RtPrintf("AUTO_BIAS_MODE %d\n",*md);
                zero_bias_mode = *md;
            }
            break;

        // clear manual batch
        case CLEAR_MAN_BATCH:
		case REMOTE_BATCH_RESET:
            {
                int* drp      = (int*) &MbMsg->gen.data;
                int  drp_sub1 = *drp - 1;

               if ( (*drp >= 1) && (*drp <= pShm->sys_set.NumDrops) )
                {
                    if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
                    {
                        sprintf((char*) &tmp_trc_buf[EVTBUFID],"CLEAR_MAN_BATCH drop %d\n", drp);
                        strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
                    }

                    CLEAR_DROP_BATCH(drp_sub1, pShm)
                    // clear intersystems data
                    CLEAR_ISYS_DROP_BATCH(drp_sub1, pShm)
                }
                else
                    RtPrintf("Bad drop %d file %s, line %d \n",
                              *drp, _FILE_, __LINE__);
            }
            break;
			
		case LAST_BIRD_CLEARED:
			{
				int* drp = (int*)&MbMsg->gen.data;
				int drp_sub1 = (*drp) - 1;
				last_in_batch_dropped[drp_sub1] = true;
			}
			break;

        case HOST_HEARTBEAT:

            // The host may decide to pause the backend's updates
            // while the host tries to retreive a large amount of
            // data from the database for a graph or report.
            // drop records until it is ready for another chunk.
            //	CHECK_HBEAT_PAUSE(__LINE__)
			{
				int* pflg = (int*) &MbMsg->gen.data;
				susp_drop_rec_send = (*pflg ? true : false);
				if (last_drop_rec_susp!= susp_drop_rec_send)
				{
					RtPrintf("line %d pause changed from %d to %d\n",
						__LINE__, last_drop_rec_susp & 1, susp_drop_rec_send & 1);
					last_drop_rec_susp = susp_drop_rec_send;
				}
			}

            if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
            {
                sprintf((char*) &tmp_trc_buf[EVTBUFID],"HOST_HEARTBEAT flags %x\n", MbMsg->gen.data);
                strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
            }

            break;

        case CLEAR_BATCH_RECS:

            if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
            {
                sprintf((char*) &tmp_trc_buf[EVTBUFID],"CLEAR_BATCH_RECS:\n");
                strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] );
            }

            sav_drp_rec_file_info.nxt_lbl_seqnum = 1;
            memset(&batch_label, 0, sizeof(batch_label));
            break;

        // turns capture off/on and specifies capture mode
        case SET_LC_CAPTURE:
            {
                int *pScl = (int*) &MbMsg->gen.data;
                int *pMd =  (int*) &MbMsg->gen.data[4];

                if ( ((*pScl >  0) && (*pScl < 3)) &&
                     ((*pMd  >= 0) && (*pMd  < 3)) )
                {
                    capt_wt.scale = *pScl;
                    capt_wt.mode  = *pMd;
                    RtPrintf("SET_LC_CAPTURE enable scale %d mode %d\n",*pScl, *pMd);
                }
                else
                {
                    RtPrintf("Bad Loadcell capture parm %s, line %d \n",_FILE_, __LINE__);
                    //DumpData("lc_cap",(BYTE *)&MbMsg->gen.data, 8);
                }


            }
            break;

//----- These cases below are for testing without a host. The messages can be redirected
//      to self and responses will be generated here.

        case ERROR_MSG:

            printf("ERROR_MSG, Severity %d msg = %s\n",
                    MbMsg->errMsg.severity,
                    &MbMsg->errMsg.text);
            break;

        case DEBUG_MSG:

            memset(&to_be_processed, 0, sizeof(to_be_processed));
            memcpy(&to_be_processed, &MbMsg->gen.data[0], MbMsg->gen.hdr.len);
            printf("DEBUG_MSG, %s", &to_be_processed);

            break;

        case DREC_MSG:
            {
                // This case will stuff a count then sequence #'s in to_be_processed & fire send event
                ev_app_drp_resp* p2bp;

                // build a simple format for event handler
                p2bp = (ev_app_drp_resp*) &to_be_processed;

                // check to see if ok to use the to_be_processed buffer
                if(RtWaitForSingleObject(hEvtHndlr,WAIT50MS)==WAIT_FAILED)
                {
                    RtPrintf("Error %d file %s, line %d \n",GetLastError(),_FILE_, __LINE__);
                    return;
                }
                else
                {
                    // save the 1st sequence number for recovery block
                    if (MbMsg->gen.hdr.flags & DRECRECOVERY)
                    {
                        p2bp->cnt        = 1;
                        p2bp->seq_num[0] = MbMsg->drpMsg.drec[0].seq_num;
                        //	DUMP_DRECS
//						for (int i = 0; i < (int) (MbMsg->gen.hdr.len/sizeof(app_drp_rec)); i++)
//						{
//						if (drp_rec_send_buf[i].seq_num != 0)
//							RtPrintf("DRecSndBuf\tseq\t%x\tshkl\t%d\n", 
//										  drp_rec_send_buf[i].seq_num,
//										  drp_rec_send_buf[i].d_rec.shackle);
//						}
                   }
                    // save all sequence numbers for reply message
                    else
                    {
                        // figure out how many records are in the message
                        p2bp->cnt = MbMsg->gen.hdr.len/sizeof(app_drp_rec);
                        for(int i = 0; i < p2bp->cnt; i++)
                            p2bp->seq_num[i] = MbMsg->drpMsg.drec[i].seq_num;
                    }

                    // set event for processing
                    if(!RtSetEvent(process_event[EVT_SND_DREC_RESP]))
                        RtPrintf("Error %u file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
                }
            }
            break;

        // Master's line has stopped. Next lower line-id should become master.
        case MASTER_STOPPED:
            {
 				int* pLineId      = (int*) &MbMsg->gen.data;

                if( (!trc[GPBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"MASTER_STOPPED %d\n", *pLineId);
                    strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
                }

				pShm->sys_set.IsysLineSettings.active  [*pLineId - 1] = 0;
             }
            break;

        // Master's line has stared again. Lower line-id should become master. Set the
		// the lineid back to active.
        case MASTER_STARTED:
            {
				int* pLineId      = (int*) &MbMsg->gen.data;


                if( (!trc[GPBUFID].buffer_full) && (TraceMask & _HOST_COMS_) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"MASTER_STARTED %d\n", *pLineId);
                    strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
                }

				pShm->sys_set.IsysLineSettings.active  [*pLineId - 1] = 1;
             }
            break;


        default:
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            break;
    }
}

//--------------------------------------------------------
//   Mbx_Server
//--------------------------------------------------------

int __stdcall overhead::Mbx_Server(PVOID unused)
{
unused;	//	RED - To eliminate unreferenced formal parameter warning
    // indicate this thread is ready
    app->pShm->IsysLineStatus.app_threads |= RTSS_APP_MBX_SVR_RDY;

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X App Mbx_Server\n", hAppRx);
#endif

    for (;;)
    {
       // check for everything ok
       if(RtWaitForSingleObject(hInterfaceThreadsOk,INFINITE)==WAIT_FAILED)
           RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
       else
       {
            if(RtWaitForSingleObject( hAppSemPost, INFINITE)==WAIT_FAILED)
                RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            else
            {

                app->ProcessMbxMsg();

                pAppIpcMsg->Ack = 1;

                if(!RtReleaseSemaphore( hAppSemAck, lAppReleaseCount, NULL))
                    RtPrintf("RtReleaseSemaphore failed.\n");
            }
       }
    }
}

//--------------------------------------------------------
//   MbxEventHandler
//--------------------------------------------------------

int __stdcall overhead::MbxEventHandler(PVOID unused)
{
	unused;	//	RED - To eliminate unreferenced formal parameter warning
    int event;

    SetUnhandledExceptionFilter(appMbxEvUnhandledExceptionFilter);

    for (;;)
    {

//----- Set event to let ProcessMbxMessage routine know it is ok
//      to use the to_be_processed buffer, if needed.

        if(!RtSetEvent(hEvtHndlr))
            RtPrintf("Error %d file %s, line %d \n",GetLastError(),_FILE_, __LINE__);

//----- Wait for the some event object to be signaled.

        event = RtWaitForMultipleObjects( NUM_EVT,       // number of event objects
                                          process_event, // array of event objects
                                          FALSE,         // does not wait for all
                                          WAIT50MS );   // waits 100ms
        if(!RtResetEvent(hEvtHndlr))
            RtPrintf("Error %d file %s, line %d \n",GetLastError(),_FILE_, __LINE__);

        // determine which event
        event -= WAIT_OBJECT_0;

        switch(event)
        {

            case EVT_SHM_READ:
                {
                    int* p2bp = (int*) &app->to_be_processed;

                    //RtPrintf("Event: SHM_READ id %d\n",*p2bp-1);
                    app->shm_updates[*p2bp-1] = true;
                }
                break;

            case EVT_DREC_RESP:
                {
                    ev_app_drp_resp* p2bp = (ev_app_drp_resp*) &app->to_be_processed;

                    //RtPrintf("Event: EVT_DREC_RESP\n");
                    // if the reply was for a saved drop record block
                    if ( (UINT) p2bp->seq_num[0] == app->sav_beg_seq_num)
                        app->RemoveSavedDropRecords();
                    else
                        app->RemoveDropRecords(p2bp->cnt, (UINT*) &p2bp->seq_num);

// Response timing check for drop records
#ifdef _DREC_TIMING_TEST_
                    for (int drec = 0; drec < p2bp->cnt; drec++)
                    {
                        if (drec_seq_done == p2bp->seq_num[drec])
                        {
                            RtGetClockTime(CLOCK_FASTEST,&drec_EndTime);
		                    RtPrintf("DREC\tresp\tcnt\t%d\ttime\t%u\tms\n",
                                    p2bp->cnt,
                                    (UINT) ((drec_EndTime.QuadPart - drec_StartTime.QuadPart)/10000));
                            drec_seq_done = 0;
                        }
                    }
#endif

// Response timing check for print label, drop records that contain last in batch
#ifdef _LREC_TIMING_TEST_
                    for (int ldrec = 0; ldrec < p2bp->cnt; ldrec++)
                    {
                        if (ldrec_seq_done == p2bp->seq_num[ldrec])
                        {
                            RtGetClockTime(CLOCK_FASTEST,&ldrec_EndTime);
		                    RtPrintf("LDREC\tresp\tcnt\t%d\ttime\t%u\tms\n",
                                    p2bp->cnt,
                                    (UINT) ((ldrec_EndTime.QuadPart - ldrec_StartTime.QuadPart)/10000));
                            ldrec_seq_done = 0;
                        }
                    }
#endif
                }
                break;

//----- This events below here should not normally occur, they are just for debugging.

            case EVT_SND_DREC_RESP:
                {
                    ev_app_drp_resp* p2bp = (ev_app_drp_resp*) &app->to_be_processed;

                    //RtPrintf("Event: EVT_SND_DREC_RESP\n");
                    // Request Mutex for send
                    if(RtWaitForSingleObject(trc[EVTBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    else
                    {
                        if HOST_OK
                            app->SendHostMsg( DREC_RESP, NULL, (BYTE*) &p2bp->seq_num, p2bp->cnt * sizeof(int));

                        if(!RtReleaseMutex(trc[EVTBUFID].mutex))
                            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    }
                }
                break;

            case WAIT_TIMEOUT:

                //RtPrintf("Event: WAIT_TIMEOUT\n");
                // ok to send event thread trace
                if ( trc[EVTBUFID].send_OK )
                {
                    // Request Mutex for send
                    if(RtWaitForSingleObject(trc[EVTBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    else
                    {

// Send debug to self
#ifdef _REDIRECT_DEBUG_
						app->SendLineMsg( DEBUG_MSG, app->this_lineid, NULL,
                              (BYTE*) &trc_buf[EVTBUFID],
                              trc[EVTBUFID].len);
#endif

#ifdef _LOCAL_DEBUG_
						if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
						{
#ifdef	_TELNET_UDP_ENABLED_
							UdpTraceNBuf((char*) &trc_buf[EVTBUFID], trc[EVTBUFID].len);
#else
							PrintNBuf((char*) &trc_buf[EVTBUFID], trc[EVTBUFID].len);
#endif
						}
						else
						{
							PrintNBuf((char*) &trc_buf[EVTBUFID], trc[EVTBUFID].len);
						}
#endif

#ifdef	_HOST_DEBUG_
						if HOST_OK
						{
							app->SendHostMsg( DEBUG_MSG, NULL,
                                  (BYTE*) &trc_buf[EVTBUFID],
                                  trc[EVTBUFID].len);
						}
#endif

                        if(!RtReleaseMutex(trc[EVTBUFID].mutex))
                            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                    }

                    // clear old message.
                    memset(&trc_buf[EVTBUFID], 0, sizeof(trcbuf));
                    sprintf( (char*) &trc_buf[EVTBUFID],"L%d:%s\n",
                             app->this_lineid,
                             &trc_buf_desc[EVTBUFID]);

                    trc[EVTBUFID].send_OK = false;
                }
                break;

            default:
                RtPrintf("Event %d file %s Error %d , line %d \n",
                          event, GetLastError(), _FILE_, __LINE__);
                break;
        }
    }
}

//--------------------------------------------------------
//  ClearOutputs
//--------------------------------------------------------

void overhead::ClearOutputs()
{
    const PUCHAR out_port[MAXOUTPUTBYTS-2] = {PCM_3724_1_PORT_A1,PCM_3724_1_PORT_B1,
                                              PCM_3724_1_PORT_C1};

//----- primary i/o

    for (int i = 0; i < MAXOUTPUTBYTS-2; i++)
    {
        output_byte[i] = 0;
        RtWritePortUchar(out_port[i], (unsigned char) ~0);
    }

//----- secondary i/o

// VSB6 On board outputs are swapped (B0 = B15)
#ifdef _VSBC6_IO_  // 2 secondary i/o ports

	if (BoardIsEPM15)
	{
		#ifdef VSBC6_LOWIN_HIOUT
            output_byte[OUTPUT_3] = 0;
			iop_3.writeOutput(OUTPUT_3);
		#endif
		// Both bytes are outputs
		#ifdef VSBC6_LOWOUT_HIOUT
            output_byte[OUTPUT_3] = 0;
            output_byte[OUTPUT_4] = 0;
			iop_3->writeOutput(OUTPUT_3);
			iop_3->writeOutput(OUTPUT_4);
		#endif
	}
	// TG - Adding EPM Support
	else if (BoardIsEPM19)
	{
		// TG - TODO (Copying EPM15 for now)
		#ifdef VSBC6_LOWIN_HIOUT
            output_byte[OUTPUT_3] = 0;
			iop_4.writeOutput(OUTPUT_3);
		#endif
		// Both bytes are outputs
		#ifdef VSBC6_LOWOUT_HIOUT
            output_byte[OUTPUT_3] = 0;
            output_byte[OUTPUT_4] = 0;
			iop_4->writeOutput(OUTPUT_3);
			iop_4->writeOutput(OUTPUT_4);
		#endif
	}
	else
	{
		#ifdef VSBC6_LOWIN_HIOUT
            output_byte[OUTPUT_3] = 0;
            RtWritePortUchar(VSBC6_OUTPUT_PORT_HI, ~0);
		#endif
		// Both bytes are outputs
		#ifdef VSBC6_LOWOUT_HIOUT
            output_byte[OUTPUT_3] = 0;
            output_byte[OUTPUT_4] = 0;
            RtWritePortUchar(VSBC6_OUTPUT_PORT_HI, UCHAR(~0));
            RtWritePortUchar(VSBC6_OUTPUT_PORT_LO, UCHAR(~0));
		#endif
	}

#endif //_VSBC6_IO_
}

//--------------------------------------------------------
//  RawMode
//--------------------------------------------------------

void overhead::RawMode()
{
    static int  sim_weight     = 2146;
    static int  adc_read_delay = 0;
	static __int64	PrevRawWeight[MAXSCALES] = {0, 0};
	static	int		LCStuckCounter[MAXSCALES] = {0, 0};

    adc_read_delay++;

    if (adc_read_delay < 200)
        return;

    if ( (app->pShm->raw_scale < 0) || (app->pShm->raw_scale > 1) )
    {
        RtPrintf("Error raw scale %d\n", pShm->raw_scale);
        return;
    }

    adc_read_delay = 0;

    if (!weight_simulation_mode) //GLC simulation_mode)
    {

        // Request Mutex for reading/writing
        if(RtWaitForSingleObject(load_cell_mutex[MAINBUFID], WAIT50MS) != WAIT_OBJECT_0)
        {
            sprintf(app_err_buf,"Can't open load cell mutex %s line %d\n", _FILE_, __LINE__);
            GenError(warning, app_err_buf);
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            return;
        }
        else
        {
			if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
			{
				app->pShm->sys_stat.raw_weight = app->ld_cel[0]->read_load_cell(app->pShm->raw_scale); //RCB 485???
			}
			else
			{
				app->pShm->sys_stat.raw_weight = app->ld_cel[app->pShm->raw_scale]->read_load_cell(0); //RCB 485???

				// GLC 12/10/2004
				// Check to see if digital load cell readings have stalled
				// Need to warn user to check loadcell power and reset backend

				if (PrevRawWeight[app->pShm->raw_scale] == app->pShm->sys_stat.raw_weight)
				{
					LCStuckCounter[app->pShm->raw_scale]++;
					if (LCStuckCounter[app->pShm->raw_scale] > MAXLCSTUCKCOUNTRAW)
					{
						StuckLoadCellWarning[app->pShm->raw_scale] = true;
						LCStuckCounter[app->pShm->raw_scale] = 0;
					}
				}
				else
				{
					StuckLoadCellWarning[app->pShm->raw_scale] = false;
					LCStuckCounter[app->pShm->raw_scale] = 0;
				}

				PrevRawWeight[app->pShm->raw_scale] = app->pShm->sys_stat.raw_weight;

			}

			INDIV_READS( app->pShm->raw_scale, FUNC_RAW, app->pShm->sys_stat.raw_weight)

            if(!RtReleaseMutex(load_cell_mutex[MAINBUFID]))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }
    }
    // Simulate raw mode
    else
    {
        if (app->pShm->scl_set.OffsetAdc[pShm->raw_scale] != LastOffsetAdc[pShm->raw_scale])
        {
            // multiplier is not important.
            sim_weight += pShm->scl_set.OffsetAdc[pShm->raw_scale] * 1000;
            LastOffsetAdc[pShm->raw_scale] = pShm->scl_set.OffsetAdc[pShm->raw_scale];

            // So GetWeight will start a new AutoBias average with this offset.
            get_weight_init[pShm->raw_scale] = false;

            // So no tares can be done until
            if (app->pShm->raw_scale == 0)
                app->pShm->SyncStatus[SCALE1SYNCBIT].zeroed = false;
            else
                app->pShm->SyncStatus[SCALE2SYNCBIT].zeroed = false;

            app->pShm->WeighZero[pShm->raw_scale] = false;
            weigh_state[app->pShm->raw_scale]     = WeighIdle;

        }

        app->pShm->sys_stat.raw_weight = sim_weight;
    }

    shm_updates[STATID-1] = true;
}

//--------------------------------------------------------
//  Check_Adc_Settings
//--------------------------------------------------------

void overhead::Check_Adc_Settings(int scale)
{
    // Need to update read the mode
    if (app->pShm->scl_set.AdcMode[scale] != app->LastAdcMode[scale])
    {
        if ((app->pShm->scl_set.AdcMode[scale] >= INDIVID) && (app->pShm->scl_set.AdcMode[scale] <= AVG) )
        {
            // Request Mutex for reading/writing
            if(RtWaitForSingleObject(load_cell_mutex[GPBUFID], WAIT50MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {
                app->LastAdcMode[scale] = app->pShm->scl_set.AdcMode[scale];
                RtPrintf("Writing Mode %d \n", app->pShm->scl_set.AdcMode[scale]);

				if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
				{
					app->ld_cel[0]->set_adc_mode(scale, app->pShm->scl_set.AdcMode[scale]);
				}
				else
				{
					app->ld_cel[scale]->set_adc_mode(scale, app->pShm->scl_set.AdcMode[0]);
				}

                if(!RtReleaseMutex(load_cell_mutex[GPBUFID]))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }
        }
        return;
    }

    // Need to update read the number of reads
    if (app->pShm->scl_set.NumAdcReads[scale] != app->LastNumAdcReads[scale])
    {
//////////
        // Request Mutex for reading/writing
        if(RtWaitForSingleObject(load_cell_mutex[GPBUFID], WAIT50MS) != WAIT_OBJECT_0)
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        else
        {
			if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
			{
				app->ld_cel[0]->set_adc_num_reads(scale, app->pShm->scl_set.NumAdcReads[scale]);
			}
			else
			{
				app->ld_cel[scale]->set_adc_num_reads(scale, app->pShm->scl_set.NumAdcReads[0]);
				//RtPrintf("set_adc_num_reads: scale %d reads %d \n",
				//	scale, pShm->scl_set.NumAdcReads[0]);
			}

			if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
			{
				if (app->pShm->scl_set.NumAdcReads[scale] != app->ld_cel[0]->read_adc_num_reads(scale))
				{
					app->pShm->scl_set.NumAdcReads[scale] = app->ld_cel[0]->read_adc_num_reads(scale);
				}
				else
				{
					app->LastNumAdcReads[scale] = app->ld_cel[0]->read_adc_num_reads(scale); //GLC
				}
			}
			else
			{
				if (app->pShm->scl_set.NumAdcReads[scale] != app->ld_cel[scale]->read_adc_num_reads(0))
				{

					//RtPrintf("Change NumAdcReads: scale %d scl_set.NumAdcReads %d ld_cel->read_adc_num_reads %d\n",
					//		scale, pShm->scl_set.NumAdcReads[scale], ld_cel[scale]->read_adc_num_reads(0) );
					app->pShm->scl_set.NumAdcReads[scale] = app->ld_cel[scale]->read_adc_num_reads(0);

				}
				else
				{
					app->LastNumAdcReads[scale] = app->ld_cel[scale]->read_adc_num_reads(0); //GLC
					//RtPrintf("LastNumAdcReads %d scale %d\n", LastNumAdcReads[scale], scale);
				}
			}

            if(!RtReleaseMutex(load_cell_mutex[GPBUFID]))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }
        return;

/* RED - Following code until next close comments was marked as unreachable by the compiler because of the return above
        /////////


//        if ((pShm->scl_set.NumAdcReads[scale] > 0) && (pShm->scl_set.NumAdcReads[scale] <= MAX_ADC_READS) )
//        {
        // Request Mutex for reading/writing
        if(RtWaitForSingleObject(load_cell_mutex[GPBUFID], WAIT50MS) != WAIT_OBJECT_0)
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        else
        {
//            LastNumAdcReads[scale] = pShm->scl_set.NumAdcReads[scale];
            //RtPrintf("Writing # reads %d \n", pShm->scl_set.NumAdcReads);

			if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
			{
				app->ld_cel[0]->set_adc_num_reads(scale, app->pShm->scl_set.NumAdcReads[scale]);

				if (app->pShm->scl_set.NumAdcReads[scale] != app->ld_cel[0]->read_adc_num_reads(scale))
				{
						app->pShm->scl_set.NumAdcReads[scale] = app->ld_cel[0]->read_adc_num_reads(scale);
				}
				else
				{
						app->LastNumAdcReads[scale] = app->ld_cel[0]->read_adc_num_reads(scale); //GLC
				}
			}
			else
			{
				app->ld_cel[scale]->set_adc_num_reads(scale, app->pShm->scl_set.NumAdcReads[0]);

				if (app->pShm->scl_set.NumAdcReads[scale] != app->ld_cel[scale]->read_adc_num_reads(0))
				{
						app->pShm->scl_set.NumAdcReads[scale] = app->ld_cel[scale]->read_adc_num_reads(0);
				}
				else
				{
						app->LastNumAdcReads[scale] = app->ld_cel[scale]->read_adc_num_reads(0); //GLC
				}

			}

            if(!RtReleaseMutex(load_cell_mutex[GPBUFID]))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }
        return;
*/	//	RED - end of commented out unreachable code
	}
    // Need to update the offset
    if (app->pShm->scl_set.OffsetAdc[scale] != app->LastOffsetAdc[scale])
    {
        if ((app->pShm->scl_set.OffsetAdc[scale]  >= -7) && (app->pShm->scl_set.OffsetAdc[scale] <= 7) )
        {
            // Request Mutex for reading/writing
            if(RtWaitForSingleObject(load_cell_mutex[GPBUFID], WAIT50MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {
                app->LastOffsetAdc[scale] = pShm->scl_set.OffsetAdc[scale];
                RtPrintf("Writing scale %d offset %d %X\n", scale, app->pShm->scl_set.OffsetAdc[scale], pShm->scl_set.OffsetAdc[scale]);

				if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
				{
					app->ld_cel[0]->set_adc_offset( scale, app->pShm->scl_set.OffsetAdc[scale]);
				}
				else
				{
					app->ld_cel[scale]->set_adc_offset( scale, app->pShm->scl_set.OffsetAdc[0]);
				}

                // So GetWeight will start a new AutoBias average with this offset.
                get_weight_init[scale] = false;

                // So no tares can be done until
                if (scale == 0)
                    app->pShm->SyncStatus[SCALE1SYNCBIT].zeroed = false;
                else
                    app->pShm->SyncStatus[SCALE2SYNCBIT].zeroed = false;


			    app->pShm->WeighZero[scale] = false;
                weigh_state[scale]     = WeighIdle;
                shm_updates[SCLWGHZ-1] = true;

                if(!RtReleaseMutex(load_cell_mutex[GPBUFID]))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }
            return;
        }
    }
}

//--------------------------------------------------------
//  Validate
//
//  Most anything can go here, this is mainly to be
//  sure debug string prints do not stray.
//--------------------------------------------------------

void overhead::Validate()
{
   int i,j,drop;
   bool found;

    for (i = 0; i < pShm->sys_set.NumDrops; i++)
    {
        // check grade and protect against unprintable chars and
        // lack of nulls which could cause fault in print string
        for (j = 0; j < MAXGRADES - 1; j++)
        {
            if (!VALID_GRADE(pShm->Schedule[i].Grade[j]))
                pShm->Schedule[i].Grade   [j] = 'N';

            if (!VALID_GRADE(pShm->Schedule[i].Extgrade[j]))
                pShm->Schedule[i].Extgrade[j] = 'N';
        }

        pShm->Schedule[i].Grade   [MAXGRADES - 1] = '\0';
        pShm->Schedule[i].Extgrade[MAXGRADES - 1] = '\0';

// This section was added because it was observed birds were being assigned
// but not dropped because the drop was not in the sync table first-last
// This was added to assign the sync in the sync table to the drop. If a
// sync is not found for the drop, the sync is set to bad and a message is
// sent to the interface.

       found = false;
       for (j = 0; j < MAXSYNCS; j++)
       {

            if ( ((i+1) >= pShm->sys_set.SyncSettings[j].first) &&
                 ((i+1) <= pShm->sys_set.SyncSettings[j].last) )
            {
                pShm->sys_set.DropSettings[i].Sync = j;
                found = true;
                break;
            }
        }

        if ( (!found) && (pShm->sys_set.DropSettings[i].Sync != bad_sync) )
        {
            pShm->sys_set.DropSettings[i].Sync = bad_sync;
            sprintf(app_err_buf,"Drop %d bad sync %s line %d\n", i+1, _FILE_, __LINE__);
            GenError(warning, app_err_buf);
        }
   }

//----- If drop is a slave drop and had no batch number at beginning of batch,
//      a response for a drop master batch request should arrive shortly afterward.
//      Shackle data is searched for temporary batch numbers and replaced
//      with the real ones.

    for (i = 0; i < pShm->sys_set.Shackles; i++)
    {
        for (j = 0; j < pShm->scl_set.NumScales; j++)
        {
            drop = pShm->ShackleStatus[i].drop[j] - 1;

            if ( (drop >= 0) && (pShm->ShackleStatus[i].batch_number[j] == TEMP_BATCH_NUM) )
            {
                pShm->ShackleStatus[i].batch_number[j] = pShm->sys_stat.DropStatus[drop].batch_number;

                if( (!trc[GPBUFID].buffer_full) && (TraceMask & _LABELS_) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"Replacing shkl TEMP_BATCH_NUM with %x drop %d\n",
                            pShm->sys_stat.DropStatus[drop].batch_number,drop+1);
                    strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID] );
                }
            }
        }
    }
}

//--------------------------------------------------------
//  RingSub
//--------------------------------------------------------

int overhead::RingSub( int shakle_no, int offset, int total_shakles)
{
   int drop_shackle;

   drop_shackle = shakle_no - offset;
   if (drop_shackle > 0)
   {
      if (drop_shackle > total_shakles)
         return (drop_shackle - total_shakles);
      else
         return (drop_shackle);
   }
   else
      return (drop_shackle + total_shakles);
}

//--------------------------------------------------------
//  DecCntrlCtrs
//
// Decrement control counters and turn off drops for each sync
// This block is very critical for disabling the drops after they
// have been fired later on in this ISR some 90ms earlier
// The output_timer[i] is set to 9 (default) and with each
// iteration of this ISR they are decremented and once they reach
// zero the drops are disabled
//--------------------------------------------------------

void overhead::DecCntrlCtrs()
{
    int i;

    for (i = 0; i < MAXOUTPUTBYTS * 8; i++)
    {
       if (output_timer[i] > 0)
       {
         output_timer[i]--;

         if (output_timer[i] == 0)
            SetOutput(i+1,false);
       }
    }
}

//--------------------------------------------------------
//  GradeSyncs
//
// The following block is needed for grade sync and count error
// handling, however in the new system we can I believe include it in
// the main counters loop.
//--------------------------------------------------------

void overhead::GradeSyncs()
{
	int GradeSyncIndex = 0;
	int i = 0;

    const int GradeSyncBit [MAXGRADESYNCS] = {GRADESYNCBIT, GRADESYNC2BIT};
	const int GradeZeroBit [MAXGRADESYNCS] = {GRADEZEROBIT, GRADEZERO2BIT};
	static bool	gradesync_zero_triggered[MAXGRADESYNCS]; //GLC Added for Carolina turkeys 3/9/05

	if (!(app->pShm->sys_set.MiscFeatures.EnableGradeSync2))
	{
		//GLC for Carolina Turkeys, check that the zero sensor is triggered before the sync
		if (app->pShm->sys_set.MiscFeatures.RequireZeroBeforeSync)
		{
			if (BITSET(switch_in[0], GradeZeroBit[GradeSyncIndex])) //GLC added 3/9/05
			{
				gradesync_zero_triggered[GradeSyncIndex] = true; //GLC added 3/9/05
			}
		}
		else
		{
			gradesync_zero_triggered[GradeSyncIndex] = true; //GLC added 3/9/05
		}

		if ( ( BITSET(switch_in[0], GradeSyncBit[GradeSyncIndex]) ) &&
			grade_armed[GradeSyncIndex] &&
			gradesync_zero_triggered[GradeSyncIndex] )
		{
			grade_armed[GradeSyncIndex] = false;
			gradesync_zero_triggered[GradeSyncIndex] = false; //GLC added 3/9/05
			//RtPrintf("Grade sync\n");

			if ( BITSET(switch_in[0], GradeZeroBit[GradeSyncIndex]) )
			{

				//RtPrintf("Grade zero\n");
				//GLC added 2/15/05
				if ((pShm->grade_shackle[GradeSyncIndex] < true_grade_shackle_count[GradeSyncIndex]) &&
					(true_grade_shackle_count[GradeSyncIndex] > pShm->sys_set.Shackles) &&
					grade_zeroed[GradeSyncIndex] )
                {
                    sprintf(app_err_buf,"Late Zero Flag Detected. Grade Sync %d late count %ld\n",
						GradeSyncIndex + 1, (long) (true_grade_shackle_count[GradeSyncIndex] - pShm->sys_set.Shackles));
                    GenError(warning, app_err_buf);

                }
                else if ((pShm->grade_shackle[GradeSyncIndex] < pShm->sys_set.Shackles) && grade_zeroed[GradeSyncIndex] )
                {
                    sprintf(app_err_buf,"Early Zero Flag Detected. Grade Sync %d shkl %d expected shkl %d\n",
						GradeSyncIndex + 1, pShm->grade_shackle[GradeSyncIndex], pShm->sys_set.Shackles);
                    GenError(warning, app_err_buf);
					true_grade_shackle_count[GradeSyncIndex] = 1;
                }

				//GLC 2/15/05 clear sync shackle counters to avoid late zero errors on syncs after
				// grade sync has zeroed the first time
				if (!grade_zeroed[GradeSyncIndex])
				{
                    sprintf(app_err_buf,"Initial Zero Flag Detected. Grade Sync %d\n",
						GradeSyncIndex + 1);
                    GenError(informational, app_err_buf);


					for (i = 0; i< MAXSYNCS; i++)
					{
						pShm->SyncStatus[i].shackleno = 0;
						true_shackle_count[i] = 0;
					}
				}

				trolly_counters[MAXSYNCS]					= 0;
				true_grade_shackle_count[GradeSyncIndex]	= 1;
				pShm->grade_shackle[GradeSyncIndex]         = 1;
				grade_zeroed[GradeSyncIndex]                = true;
				grade_zeroed[1]								= true;
				pShm->sys_stat.dbg_sync[ZERO_ON][MAXSYNCS - 1]	= 0;
				pShm->sys_stat.dbg_sync[SYNC_ON][MAXSYNCS - 1]++;

			}
			else
			{
				if (pShm->sys_set.SkipTrollies > 0)
				{
					trolly_counters[MAXSYNCS]++;

					if (trolly_counters[MAXSYNCS] > pShm->sys_set.SkipTrollies)
					{
 						pShm->grade_shackle[GradeSyncIndex]++;
						true_grade_shackle_count[GradeSyncIndex]++; //GLC added 2/14/05
						trolly_counters[MAXSYNCS] = 0;

						// special, the grade sync counter is tacked on to the end of regular syncs.
						pShm->sys_stat.dbg_sync[SYNC_ON][MAXSYNCS - 1]++;
						pShm->sys_stat.dbg_sync[ZERO_ON][MAXSYNCS - 1]++;
					}
				}
				else
				{
					pShm->grade_shackle[GradeSyncIndex]++;
					true_grade_shackle_count[GradeSyncIndex]++; //GLC added 2/14/05
					// special, the grade sync counter is tacked on to the end of regular syncs.
					pShm->sys_stat.dbg_sync[SYNC_ON][MAXSYNCS - 1]++;
					pShm->sys_stat.dbg_sync[ZERO_ON][MAXSYNCS - 1]++;
				}
				//RtPrintf("inc grade_shackle \n");
			}

			// Late zero flag
			if ( (pShm->grade_shackle[GradeSyncIndex] > pShm->sys_set.Shackles ) )
			{
                sprintf(app_err_buf,"Zero Flag NOT Detected. Grade Sync %d shackle %d expected %d\n", GradeSyncIndex + 1, pShm->grade_shackle[GradeSyncIndex], pShm->sys_set.Shackles); //GLC added 2/14/05

				trolly_counters[MAXSYNCS] = 0;
				pShm->grade_shackle[GradeSyncIndex] = 1;

				GenError(warning, app_err_buf);
			}

			if ( (pShm->OpMode == ModeRun) && (grade_zeroed[GradeSyncIndex]) )
			{
				GradeProcess(GradeSyncIndex);
			}
			 //else
			 //    RtPrintf("OpMode %d grade_zeroed %d\n",
			 //               pShm->OpMode, grade_zeroed);

		}
		else
		{
			if ( !(BITSET(switch_in[0], GradeSyncBit[GradeSyncIndex])) )
			{
				grade_armed[GradeSyncIndex] = true;
			}
		}
	}

	if(app->pShm->sys_set.MiscFeatures.EnableGradeSync2)
	{
		for (GradeSyncIndex = 0; GradeSyncIndex < MAXGRADESYNCS; GradeSyncIndex++)
		{
			//GLC for Carolina Turkeys, check that the zero sensor is triggered before the sync
			if (app->pShm->sys_set.MiscFeatures.RequireZeroBeforeSync)
			{
				if (BITSET(switch_in[0], GradeZeroBit[GradeSyncIndex])) //GLC added 3/9/05
				{
					gradesync_zero_triggered[GradeSyncIndex] = true; //GLC added 3/9/05
				}
			}
			else
			{
				gradesync_zero_triggered[GradeSyncIndex] = true; //GLC added 3/9/05
			}

			if ( ( BITSET(switch_in[0], GradeSyncBit[GradeSyncIndex]) ) &&
				grade_armed[GradeSyncIndex] &&
				gradesync_zero_triggered[GradeSyncIndex] ) //GLC added 3/9/05
			{
				grade_armed[GradeSyncIndex] = false;
				gradesync_zero_triggered[GradeSyncIndex] = false; //GLC added 3/9/05
				//RtPrintf("Grade sync\n");

				if ( BITSET(switch_in[0], GradeZeroBit[GradeSyncIndex]) )
				{

					//GLC added 2/15/05
					if ((pShm->grade_shackle[GradeSyncIndex] < true_grade_shackle_count[GradeSyncIndex]) &&
						(true_grade_shackle_count[GradeSyncIndex] > pShm->sys_set.Shackles) &&
						grade_zeroed[GradeSyncIndex] )
					{
						sprintf(app_err_buf,"Late Zero Flag Detected. Grade Sync %d late count %ld\n",
							GradeSyncIndex + 1, (long) (true_grade_shackle_count - pShm->sys_set.Shackles));
						GenError(warning, app_err_buf);

					}
					else if ((pShm->grade_shackle[GradeSyncIndex] < pShm->sys_set.Shackles) && grade_zeroed[GradeSyncIndex])
					{
						sprintf(app_err_buf,"Early Zero Flag Detected. Grade Sync %d shkl %d expected shkl %d\n",
							GradeSyncIndex + 1, pShm->grade_shackle[GradeSyncIndex], pShm->sys_set.Shackles);
						GenError(warning, app_err_buf);
					}

					//GLC 2/15/05 clear sync shackle counters to avoid late zero errors on syncs after
					// grade sync has zeroed the first time
					if (!grade_zeroed[GradeSyncIndex])
					{
						sprintf(app_err_buf,"Initial Zero Flag Detected. Grade Sync %d\n",
							GradeSyncIndex + 1);
						GenError(informational, app_err_buf);

						for (i = 0; i< MAXSYNCS; i++)
						{
							pShm->SyncStatus[i].shackleno = 0;
							true_shackle_count[i] = 0;
						}
					}

					trolly_counters[MAXSYNCS + GradeSyncIndex]  = 0;
					true_grade_shackle_count[GradeSyncIndex]	= 1;
					pShm->grade_shackle[GradeSyncIndex]         = 1;
					grade_zeroed[GradeSyncIndex]                = true;
					pShm->sys_stat.dbg_sync[ZERO_ON][MAXSYNCS + GradeSyncIndex - 1] = 0;
					pShm->sys_stat.dbg_sync[SYNC_ON][MAXSYNCS + GradeSyncIndex - 1]++;

				}
				else
				{
					if (pShm->sys_set.SkipTrollies > 0)
					{
						trolly_counters[MAXSYNCS + GradeSyncIndex]++;

						if (trolly_counters[MAXSYNCS + GradeSyncIndex] > pShm->sys_set.SkipTrollies)
						{
							true_grade_shackle_count[GradeSyncIndex]++;
 							pShm->grade_shackle[GradeSyncIndex]++;
							trolly_counters[MAXSYNCS + GradeSyncIndex] = 0;

							// special, the grade sync counter is tacked on to the end of regular syncs.
							pShm->sys_stat.dbg_sync[SYNC_ON][MAXSYNCS + GradeSyncIndex - 1]++;
							pShm->sys_stat.dbg_sync[ZERO_ON][MAXSYNCS + GradeSyncIndex - 1]++;
						}
					}
					else
					{
						pShm->grade_shackle[GradeSyncIndex]++;
						true_grade_shackle_count[GradeSyncIndex]++;
						// special, the grade sync counter is tacked on to the end of regular syncs.
						pShm->sys_stat.dbg_sync[SYNC_ON][MAXSYNCS + GradeSyncIndex - 1]++;
						pShm->sys_stat.dbg_sync[ZERO_ON][MAXSYNCS + GradeSyncIndex - 1]++;
					}
					//RtPrintf("inc grade_shackle \n");
				}

				// Late zero flag
				if ( (pShm->grade_shackle[GradeSyncIndex] > pShm->sys_set.Shackles ) )
				{
	                sprintf(app_err_buf,"Zero Flag NOT Detected. Grade Sync %d shackle %d expected %d\n",
						GradeSyncIndex + 1, pShm->grade_shackle[GradeSyncIndex], pShm->sys_set.Shackles); //GLC added 2/14/05

					trolly_counters[MAXSYNCS + GradeSyncIndex]   = 0;
					pShm->grade_shackle[GradeSyncIndex]         = 1;

					GenError(warning, app_err_buf);
				}

				// Hard coded for now. We should not need more than 2 grade syncs
				// But NEVER say NEVER
				if ( (pShm->OpMode == ModeRun) &&
					(grade_zeroed[0] && grade_zeroed[1]) )
				{
					GradeProcess(GradeSyncIndex);
				}
				//else
				//    RtPrintf("OpMode %d grade_zeroed %d\n",
				//               pShm->OpMode, grade_zeroed);

			}
			else
			{
				if ( !(BITSET(switch_in[0], GradeSyncBit[GradeSyncIndex])) )
				{
					grade_armed[GradeSyncIndex] = true;
				}
			}
		}
	}
}

//--------------------------------------------------------
//  ProcessWeight
//--------------------------------------------------------
void overhead::ProcessWeight()
{
    static bool cfg_err_sent = false;
    static int  last_shackle[] = {0, 0};      // used to trigger an update to host
    static int  WAct_shack, WDn_shack; // sanity check
    int i, loop_cnt;

    loop_cnt = (dual_scale ? 2 : 1);

    for (i = 0; i < loop_cnt; i++)
    {

//----- Process syncs routine has detected a new shackle

        if ( weigh_state[i] == WeighActive )
        {
            if (i == 0)
            {
                shackle_count++;
                WAct_shack = pShm->WeighShackle[0];
            }

            weigh_state[i] = WeighAverage;
        }

//----- Ok to use weight

        if ( weigh_state[i] == WeighDone )
        {

            weigh_state[i] = WeighIdle;

            // clear weight averaging struct.
            memset(&w_avg[i], 0, sizeof(t_wt_avg));

//----- There is no shackle here. Process auto zero.

            // There is no shackle here. Process auto zero.
            if (pShm->WeighShackle[i] == 1)
            {
                //if (simulation_mode)
                if (weight_simulation_mode)
                    WEIGH_SHACKLE(i, pShm).weight[i] = 0;

                AutoZero(i);
            }

//----- Subtract auto zero.

            WEIGH_SHACKLE(i, pShm).weight[i] -= (__int64) pShm->AutoBias[i];

            switch(pShm->OpMode)
            {
              case ModeATare1:

                  if (i == 0)
                  {
                      //if (simulation_mode)
                      if (weight_simulation_mode)
                          WEIGH_SHACKLE(i, pShm).weight[i] = 0;

                      AutoTare(i);
                      // Don't need weight anymore, set to 0 so display will not show
                      // strange weights.
                      WEIGH_SHACKLE(i, pShm).weight[i] = 0;
                  }
                  break;

              case ModeATare2:

                  if (i == 1)
                  {
//                      if (simulation_mode)
                      if (weight_simulation_mode)
                          WEIGH_SHACKLE(i, pShm).weight[i] = 0;

                      AutoTare(i);
                      WEIGH_SHACKLE(i, pShm).weight[i] = 0;
                  }
                  break;

              case ModeATare3:

//                  if (simulation_mode)
                  if (weight_simulation_mode)
                      WEIGH_SHACKLE(i, pShm).weight[i] = 0;

                  AutoTare(i);
                  WEIGH_SHACKLE(i, pShm).weight[i] = 0;
                  break;

              case ModeASpan1:
              case ModeASpan2:

                  // Subtract the tare
                  WEIGH_SHACKLE(i, pShm).weight[i] -= TARE_SHACKLE(i, pShm);
                  break;

              case ModeRun:

                   // If we left AutoTare
                   pShm->AutoTareStep = 0;

                   // Subtract the tare
                   WEIGH_SHACKLE(i, pShm).weight[i] -= TARE_SHACKLE(i, pShm);

                   // Add span bias
                   {
                       __int64 spn_bias;
                       spn_bias = ((WEIGH_SHACKLE(i, pShm).weight[i] * pShm->scl_set.SpanBias[i]) / 1000);
                       WEIGH_SHACKLE(i, pShm).weight[i] += (__int64) spn_bias;
                   }

//----- Find a drop

                   //RtPrintf("WBird Weight %ld\n", WEIGH_SHACKLE(i, pShm).weight[i] );
                   //RtPrintf("WZero0 %d WZero1 %d GZero %d cok %d\n",
                   //          pShm->WeighZero[0],pShm->WeighZero[1],
                   //          grade_zeroed,(int) config_Ok);

                  if ( pShm->WeighZero[i] &&
                       grade_zeroed[0]    &&
					   grade_zeroed[1]    &&
                       config_Ok )
                  {
                        cfg_err_sent = false;
                        FindDrops(i+1);
                  }
                  else
                  {
                      if ( (!config_Ok) && (!cfg_err_sent) )
                      {
                           sprintf(app_err_buf,"Cannot run configuration error\n");
                           GenError(warning, app_err_buf);
                           cfg_err_sent = true;
                      }
                  }
                  break;

              default:
                    RtPrintf("Error OpMode %d file %s, line %d \n", pShm->OpMode, _FILE_, __LINE__);
                  break;
            }

            // Update data for display.
            pShm->sys_stat.DispShackle.GradeIndex[i] = WEIGH_SHACKLE(i, pShm).GradeIndex[i];  // added 3/16/2006
            pShm->sys_stat.DispShackle.shackle[i] = pShm->WeighShackle[i];
            pShm->sys_stat.DispShackle.weight [i] = WEIGH_SHACKLE(i, pShm).weight[i];
            pShm->sys_stat.DispShackle.drop   [i] = WEIGH_SHACKLE(i, pShm).drop[i];

//----- Update host. If one scale send, if two scales send after 2nd pass

            //if ( ((loop_cnt == 1) && (i == 0)) ||
            //     ((loop_cnt == 2) && (i == 1)) )
			//if(1)
            {
                if (pShm->WeighShackle[i] != last_shackle[i])
                {
                    //WDn_shack = pShm->WeighShackle[0];

                    //if ( (i == 0) && (WAct_shack != WDn_shack) )
                        //RtPrintf("!!! WAct_shack %d WDn_shack %d", WAct_shack,WDn_shack);

                    DebugTrace(_WEIGHT_,"WBird\tWshk\t%d\tDshk\t%d\tlst\t%d\n",
                              pShm->WeighShackle[0],
                              pShm->sys_stat.DispShackle.shackle[i],
                              last_shackle[i]);
                    shm_updates[DISP_DATA-1] = true;
                    last_shackle[i]             = pShm->WeighShackle[i];
                }
            }
        }
    }
}

//--------------------------------------------------------
//  SetOutput
//--------------------------------------------------------

PUCHAR port_array[3] = {PCM_3724_1_PORT_A1, PCM_3724_1_PORT_B1,
                        PCM_3724_1_PORT_C1};

void overhead::SetOutput(int num, DBOOL active)
{
    int byte;
    int bit;

    if (simulation_mode) return;

    byte = ((num - 1) / 8);
    bit  = num - 1;

    if (byte < MAXOUTPUTBYTS)
    {

//----- set/clear the output_byte bits

        if (active)
        {
            SETBIT(output_byte[byte], bit);
            output_timer[num-1] = (int) pShm->sys_set.DropOn;
        }
        else
        {
            CLRBIT(output_byte[byte], bit);
        }

//----- Write the outputs

        switch (byte)
        {
            case OUTPUT_0:
            case OUTPUT_1:
            case OUTPUT_2:
                RtWritePortUchar(port_array[byte], (unsigned char) ~output_byte[byte]);
                break;

// VSB6 On board outputs are swapped (B0 = B15)
#ifdef _VSBC6_IO_  // 2 secondary i/o ports
#ifdef VSBC6_LOWIN_HIOUT

			case OUTPUT_3:
				if (BoardIsEPM15)
				{
					iop_3.writeOutputs(OUTPUT_3);
				}
				// TG - Adding EPM19 Support
				else if (BoardIsEPM19)
				{
					// TG - TODO (Copying EPM15 for now)
					iop_4.writeOutputs(OUTPUT_3);
				}
				else
				{
					VSBC6_SWAP_AND_WRITE(output_byte[OUTPUT_3],VSBC6_HI)
				}
				break;
#endif // VSBC6_LOWIN_HIOUT
	// Both bytes are outputs
#ifdef VSBC6_LOWOUT_HIOUT

			case OUTPUT_3:
				if (BoardIsEPM15)
				{
					iop_3->writeOutput(OUTPUT_3);
				}
				// TG - Adding EPM19 Support
				else if (BoardIsEPM19)
				{
					// TG - TODO (Copying EPM15 for now)
					iop_4->writeOutput(OUTPUT_3);
				}
				else
				{
					int port = VSBC6_HI;
					VSBC6_SWAP_AND_WRITE(output_byte[OUTPUT_3],port)
				}
				break;

			case OUTPUT_4:
				if (BoardIsEPM15)
				{
					iop_3->writeOutput(OUTPUT_4);
				}
				// TG - Adding EPM19 Support
				else if (BoardIsEPM19)
				{
					// TG - TODO (Copying EPM15 for now)
					iop_4->writeOutput(OUTPUT_4);
				}
				else
				{
					int port = VSBC6_LO;
					VSBC6_SWAP_AND_WRITE(output_byte[OUTPUT_4], port)
				}
				break;
#endif // VSBC6_LOWOUT_HIOUT
#endif //_VSBC6_IO_

            default:
                RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
                break;
		} // end switch (byte)

    }
    else
        RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
}

//--------------------------------------------------------
//  SubtractBird
//--------------------------------------------------------
void overhead::SubtractBird(int grade_idx, int drop_no, __int64 wt)
{
    int drop_minus1;

    if (drop_no > 0)
    {
        drop_minus1 = drop_no - 1;

        // Grand Totals

        if (pShm->sys_stat.TotalCount > 0)
            pShm->sys_stat.TotalCount--;

        if (pShm->sys_stat.TotalWeight > wt)
            pShm->sys_stat.TotalWeight -= wt;

        // Grade Area
        if (pShm->sys_stat.GradeCount[grade_idx] > 0)
            pShm->sys_stat.GradeCount[grade_idx]--;

        // Drop State

        if (pShm->sys_stat.DropStatus[drop_minus1].BCount > 0)
            pShm->sys_stat.DropStatus[drop_minus1].BCount--;
        else
            pShm->sys_stat.DropStatus[drop_minus1].BCount  = 0;

        if (pShm->sys_stat.DropStatus[drop_minus1].BWeight > 0)
            pShm->sys_stat.DropStatus[drop_minus1].BWeight -= wt;
        else
            pShm->sys_stat.DropStatus[drop_minus1].BWeight = 0;

        if DBG_DROP(drop_minus1, pShm)
        {
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
            {
                sprintf((char*) &tmp_trc_buf[MAINBUFID],"Undrop Books\tDropStatus\t[%d-1]\tBCount\t%u\tBWeight\t%u\n", drop_no,
                                (int) pShm->sys_stat.DropStatus[drop_minus1].BCount,
                                (int) pShm->sys_stat.DropStatus[drop_minus1].BWeight );
                strcat( (char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
            }
        }
        // DropInfo

        if (pShm->sys_stat.DropInfo[drop_minus1].BWeight > wt)
            pShm->sys_stat.DropInfo[drop_minus1].BWeight -= wt;
        else
            pShm->sys_stat.DropInfo[drop_minus1].BWeight = 0;

        if (pShm->sys_stat.DropInfo[drop_minus1].BCount > 0)
            pShm->sys_stat.DropInfo[drop_minus1].BCount--;
        else
            pShm->sys_stat.DropInfo[drop_minus1].BCount = 0;

        if (pShm->sys_stat.DropInfo[drop_minus1].GrdWeight[grade_idx] > wt)
            pShm->sys_stat.DropInfo[drop_minus1].GrdWeight[grade_idx] -= wt;
        else
            pShm->sys_stat.DropInfo[drop_minus1].GrdWeight[grade_idx] = 0;

        if (pShm->sys_stat.DropInfo[drop_minus1].GrdCount[grade_idx] > 0)
            pShm->sys_stat.DropInfo[drop_minus1].GrdCount[grade_idx]--;
        else
            pShm->sys_stat.DropInfo[drop_minus1].GrdCount[grade_idx] = 0;

        if DBG_DROP(drop_minus1, pShm)
        {
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
            {
                sprintf( (char*) &tmp_trc_buf[MAINBUFID],"Undrop Books\tDropInfo\t[%d-1]\tBCount\t%u\tBWeight\t%u\n\t\tGrdCnt\t%u\tGrdWt\t%u\n",
                                 drop_no,
                                (int) pShm->sys_stat.DropInfo[drop_minus1].BCount,
                                (int) pShm->sys_stat.DropInfo[drop_minus1].BWeight,
                                (int) pShm->sys_stat.DropInfo[drop_minus1].GrdCount[grade_idx],
                                (int) pShm->sys_stat.DropInfo[drop_minus1].GrdWeight[grade_idx]);
                strcat( (char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
            }
        }
    }
    else
        RtPrintf("Error file %s, line %d\n",_FILE_, __LINE__);

}

//--------------------------------------------------------
//  AddMissedBird
//--------------------------------------------------------

void overhead::AddMissedBird(int grade_idx, int drop_no, __int64 wt)
{
    int drop_minus1;

    if (drop_no > 0)
    {
        drop_minus1 = drop_no - 1;

        pShm->sys_stat.MissedDropInfo[drop_minus1].BWeight += wt;
        pShm->sys_stat.MissedDropInfo[drop_minus1].BCount++;
        pShm->sys_stat.MissedDropInfo[drop_minus1].GrdWeight[grade_idx] +=  wt;
        pShm->sys_stat.MissedDropInfo[drop_minus1].GrdCount [grade_idx]++;

        if DBG_DROP(drop_minus1, pShm)
        {
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
            {
                sprintf( (char*) &tmp_trc_buf[MAINBUFID],"Missed Books MissedDropInfo[%d]\tBCnt\t%u\tBWt\t%u\n\t\t\t\tGrd\t%c\tGrdCnt\t%u\tGrdWt\t%u\n",
                             drop_no,
                             (int) pShm->sys_stat.MissedDropInfo[drop_minus1].BCount,
                             (int) pShm->sys_stat.MissedDropInfo[drop_minus1].BWeight,
                                   pShm->sys_set.GradeArea[grade_idx].grade,
                             (int) pShm->sys_stat.MissedDropInfo[drop_minus1].GrdCount[grade_idx],
                             (int) pShm->sys_stat.MissedDropInfo[drop_minus1].GrdWeight[grade_idx]);
                strcat( (char*) &trc_buf[MAINBUFID],  (char*) &tmp_trc_buf[MAINBUFID] );
            }
        }
    }
    else
    {
        // tack no drop onto the end This means the unassigned birds
        pShm->sys_stat.MissedDropInfo[MAXDROPS].BWeight += wt;
        pShm->sys_stat.MissedDropInfo[MAXDROPS].BCount++;
        pShm->sys_stat.MissedDropInfo[MAXDROPS].GrdWeight[grade_idx] +=  wt;
        pShm->sys_stat.MissedDropInfo[MAXDROPS].GrdCount [grade_idx]++;

    }

}

//--------------------------------------------------------
//  AddBird
//--------------------------------------------------------

void overhead::AddBird(int grade_idx, int drop_no, __int64 wt)
{
    int drop_minus1;

    if (drop_no > 0)
    {

        drop_minus1 = drop_no - 1;

//----- Some Grand totals

        pShm->sys_stat.TotalCount++;
        pShm->sys_stat.TotalWeight += wt;

        pShm->sys_stat.GradeCount[grade_idx]++;

        if DBG_DROP(drop_minus1, pShm)
        {
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
            {
                sprintf( (char*) &tmp_trc_buf[MAINBUFID],"Drop Books Grand Total\t\tTCnt\t%u\tTWt\t%u\n",
                                 (int) pShm->sys_stat.TotalCount, (int) pShm->sys_stat.TotalWeight);
                strcat( (char*)  &trc_buf[MAINBUFID],  (char*) &tmp_trc_buf[MAINBUFID] );

                sprintf( (char*) &tmp_trc_buf[MAINBUFID],"Drop Books GradeArea\t[%d]\tTCnt\t%u\n",
                                 grade_idx, (int) pShm->sys_stat.GradeCount[grade_idx]);
                strcat( (char*)  &trc_buf[MAINBUFID],  (char*) &tmp_trc_buf[MAINBUFID] );
            }
         }

//----- Some drop specific totals

        // DropState
        pShm->sys_stat.DropStatus[drop_minus1].BWeight += wt;
        pShm->sys_stat.DropStatus[drop_minus1].BCount++;

        if DBG_DROP(drop_minus1, pShm)
        {
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
            {
                sprintf((char*) &tmp_trc_buf[MAINBUFID],"Drop Books DropStatus\t[%d-1]\tBCnt\t%u\tBWt\t%u\n",
                                 drop_no,
                                 (int) pShm->sys_stat.DropStatus[drop_minus1].BCount,
                                 (int) pShm->sys_stat.DropStatus[drop_minus1].BWeight );
                strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
            }
        }

        // DropInfo
        pShm->sys_stat.DropInfo[drop_minus1].BWeight += wt;
        pShm->sys_stat.DropInfo[drop_minus1].BCount++;

        pShm->sys_stat.DropInfo[drop_minus1].GrdWeight[grade_idx] += wt;
        pShm->sys_stat.DropInfo[drop_minus1].GrdCount [grade_idx]++;

        if DBG_DROP(drop_minus1, pShm)
        {
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
            {
                sprintf((char*) &tmp_trc_buf[MAINBUFID],"Drop Books DropInfo\t[%d-1]\tBCnt\t%u\tBWt\t%u\n\t\t\t\tGrd\t%c\tGrdCnt\t%u\tGrdWt\t%u\n",
                                    drop_no,
                                    (int) pShm->sys_stat.DropInfo[drop_minus1].BCount,
                                    (int) pShm->sys_stat.DropInfo[drop_minus1].BWeight,
                                          pShm->sys_set.GradeArea[grade_idx].grade,
                                    (int) pShm->sys_stat.DropInfo[drop_minus1].GrdCount[grade_idx],
                                    (int) pShm->sys_stat.DropInfo[drop_minus1].GrdWeight[grade_idx]);

                strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
            }
        }
    }
    else
        RtPrintf("Error file %s, line %d\n",_FILE_, __LINE__);
}

//--------------------------------------------------------
//   AutoTare
//
// This routine handles storing all shackle tares during
// the calibration process.
//--------------------------------------------------------
void overhead::AutoTare ( int s )
{
    static int end_shackle = 0;
    __int64 wt = WEIGH_SHACKLE(s, pShm).weight[s];

    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _AUTOZ_ ) )
    {
        //if (s == 0)
        //{
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"ATare\tShkl\t%d\trawwt\t%-10ld\tStep\t%d\tloops\t%d\n",
                                pShm->WeighShackle[s], (int)wt, pShm->AutoTareStep, pShm->sys_set.TareTimes);
            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID]);
        //}
    }

//----- The step (AutoTareStep) will increment each loop.  If the step
//      is the final step, the TareTime will equal

    switch (pShm->AutoTareStep)
    {
        case 0:

            // Just get the starting shackle number,go to next step and read
            //if (s == 0)  // RCB - WHY????
            //{
                tare_start_shkl    = pShm->WeighShackle[0];

                if (tare_start_shkl != 1)
                    end_shackle = tare_start_shkl - 1;
                else
                    end_shackle = pShm->sys_set.Shackles;

                pShm->AutoTareStep++;
            //}


		case 1:
        case 2:
		case 3:
		case 4:

			if (pShm->AutoTareStep == 1)
			{
				// 1st read, just set tare = raw weight
				TARE_SHACKLE(s, pShm) = wt;
			}
			else
			{
	            // successive reads, add to existing
	            TARE_SHACKLE(s, pShm) += wt;
			}

				// Done, calculate average and go back to run mode
				if (pShm->sys_set.TareTimes == pShm->AutoTareStep)
				{
					TARE_SHACKLE(s, pShm) /= pShm->AutoTareStep;
					if (pShm->WeighShackle[0] == end_shackle)
					{
						if ((pShm->OpMode == ModeATare3) && (s == 1))
						{
							pShm->AutoTareStep = 5;
						}
						else if (pShm->OpMode != ModeATare3)
						{
							pShm->AutoTareStep = 5;
						}
					}

				}
				else
				{
					// If start shackle - 1, go to run mode or go to next tare step
					if (pShm->WeighShackle[0] == end_shackle)
					{
						if ((pShm->OpMode == ModeATare3) && (s == 1))
						{
							pShm->AutoTareStep++;
						}
						else if (pShm->OpMode != ModeATare3)
						{
							pShm->AutoTareStep++;
						}
					}
				}

            break;


        case 5:
            fsave_grp_tbl[shm_tbl[TARES-1].group].changed = true;
            fsave_grp_tbl[shm_tbl[TARES-1].group].loaded  = true;

            //{
            //    for(int t = 0; t < pShm->sys_set.Shackles; t++)
            //    {
            //            RtPrintf("Shackle %d tare0 %d tare1 %d\n",
            //                      t+1, pShm->ShackleTares[t].tare[0],
            //                           pShm->ShackleTares[t].tare[1]);
            //    }
            //}

            pShm->OpMode          = ModeRun;
            pShm->AutoTareStep    = 0;
            shm_updates[TARES-1]  = true;
            shm_updates[OPMODE-1] = true;
            break;

        default:
            pShm->AutoTareStep = 0;
            //RtPrintf("Error file %s, line %d\n",_FILE_, __LINE__);
            break;
    }

    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _AUTOZ_ ) )
    {
        //if (s == 0)
        //{
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"ATare\tStep\t%d\tShkl\t%d\ttare\t%d\n",
                                 pShm->AutoTareStep,
                                 pShm->WeighShackle[s],
                                 (int)TARE_SHACKLE(s, pShm));
            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
        //}
    }
}

//--------------------------------------------------------
//  BpmStats
//--------------------------------------------------------

void overhead::BpmStats()
{
    int i, loop_cnt;

    // This resets the counters and triggers for the BPM processes

    pShm->sys_stat.ShacklesPerMinute = shackle_count;
    //RtPrintf("pShm->sys_stat.ShacklesPerMinute %d\n",pShm->sys_stat.ShacklesPerMinute);
    shackle_count = 0;

    loop_cnt = (dual_scale ? 2 : 1);

    for (i = 0; i < loop_cnt; i++)
    {
      pShm->sys_stat.DropsPerMinute[i] = bpm_act_count[i];
      bpm_act_count[i] = 0;
    }

    if( (!trc[GPBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
    {
        sprintf((char*) &tmp_trc_buf[GPBUFID],"Reset BPM\tSuspnd's\n");
        strcat ((char*) &trc_buf[GPBUFID], (char*) &tmp_trc_buf[GPBUFID] );
    }

    // Send a reset command to slave lines
    if (ISYS_ENABLE && ISYS_MASTER) sendBpmReset = true;

    for (i = 0; i < pShm->sys_set.NumDrops; i++)
    {
        pShm->sys_stat.ScheduleStatus [i].PPMPrevious = pShm->sys_stat.ScheduleStatus[i].PPMCount;
        pShm->sys_stat.ScheduleStatus [i].PPMCount    = 0;
        pShm->sys_stat.DropStatus     [i].Suspended   = false;

        // clear intersystems data
        for (int j = 0; j < MAXIISYSLINES; j++)
        {
            pShm->IsysDropStatus[i].PPMPrevious[j]  = pShm->IsysDropStatus[i].PPMCount[j];
            pShm->IsysDropStatus[i].PPMCount   [j]  = 0;
            pShm->IsysDropStatus[i].flags      [j] &= ~ISYS_DROP_SUSPENDED;
        }
    }
}

//--------------------------------------------------------
//  TimePPM
//
// Used to increments the seconds counter for PPM and
// miscellaneous calculations.
//--------------------------------------------------------

void overhead::TimePPM()
{
    int i, line_idx1, line_idx2, isys_tmp_drp_cnt;
    __int64 isys_tmp_avg_wt;
    bool reset_stats     = false;
    int  reset_condition = 0;

    app->pShm->sys_stat.PPMTimer++;

    //RtPrintf("PPMTimer %d\n", pShm->sys_stat.PPMTimer);

//---- Calculate slowdown setpoints for drops

    if (ISYS_ENABLE)
    {
        // reached a new maximum
        if (app->isys_shksec_sec_cnt > app->isys_shksec_max_cnt)
            app->isys_shksec_max_cnt = app->isys_shksec_sec_cnt;

        // don't allow zero
        if (app->isys_shksec_max_cnt == 0)
            app->isys_shksec_max_cnt = 1;

        app->isys_shksec_sec_cnt = 0;

        for (line_idx1 = 0; line_idx1 < MAXIISYSLINES; line_idx1++)
        {
            if (app->pShm->sys_set.IsysLineSettings.active[line_idx1] )
            {
                // calculate slowdown weight by active lines, active drops and average normal weight
                for (i = 0; i < app->pShm->sys_set.NumDrops; i++)
                {
                    isys_tmp_drp_cnt = app->isys_shksec_max_cnt;


                    for (line_idx2 = 0; line_idx2 < MAXIISYSLINES; line_idx2++)
                    {
                        if ( app->pShm->sys_set.IsysLineSettings.active[line_idx2] &&
                             app->pShm->sys_set.IsysDropSettings[i].active[line_idx2] )
                             isys_tmp_drp_cnt++;
                    }

                    app->isys_cnt_slwdn[i] = isys_tmp_drp_cnt;
                    isys_tmp_avg_wt   = (app->pShm->Schedule[i].MaxWeight + app->pShm->Schedule[i].MinWeight)/2;
                    app->isys_wt_slwdn[i]  = isys_tmp_avg_wt * isys_tmp_drp_cnt;

                }
            }
        }

		//	Recalculate Intersystems fastest indices
		app->SetIsysFastestIndices(-1, false);
    }

//----- Not isystems or isystems and the master

   //RtPrintf("BPM timer enabled %x master %x\n", isys_enabled, isys_master);
   if ((!ISYS_ENABLE) || ISYS_MASTER)
   {
        if (app->pShm->sys_stat.PPMTimer >= 61)
        {
            RtPrintf("BPM reset (master).\n");

            if( (!trc[GPBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
            {
                sprintf((char*) &tmp_trc_buf[GPBUFID],"Rst BPM\tMaster\n");
                strcat((char*) &trc_buf[GPBUFID], (char*) &tmp_trc_buf[GPBUFID] );
            }
            reset_stats = true;
        }
   }

//----- For a isystems slave, reset on command from host or give up on host if not
//      reset in a reasonable time.

   else
   {
       // reset from a master
       if (app->isys_reset_bpm)
           reset_condition = 1;
       // excessive wait, just do it
       if (app->pShm->sys_stat.PPMTimer >= 64)
           reset_condition = 2;
       // not going to get a reset
       if ((app->someone_is_connected == false) && (app->pShm->sys_stat.PPMTimer >= 61))
           reset_condition = 3;

        switch(reset_condition)
        {
            case 1:
                if( (!trc[GPBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"PPM reset (from master). Timer %d.\n", app->pShm->sys_stat.PPMTimer);
                    strcat((char*) &trc_buf[GPBUFID], (char*) &tmp_trc_buf[GPBUFID] );
                }
                else
                    RtPrintf("PPM reset (from master). Timer %d.\n", app->pShm->sys_stat.PPMTimer);

                break;

            case 2:
                if( (!trc[GPBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"PPM reset (late). Timer %d.\n",
						app->pShm->sys_stat.PPMTimer);
                    strcat((char*) &trc_buf[GPBUFID], (char*) &tmp_trc_buf[GPBUFID] );
                }
                else
				{
                    //RtPrintf("PPM reset (late). Timer %d.\n", pShm->sys_stat.PPMTimer);
					sprintf(app_err_buf,
							"PPM reset (late). Timer %d.\n", app->pShm->sys_stat.PPMTimer);
					RtPrintf("%s", app_err_buf);
					app->GenError(warning, app_err_buf);
				}

                break;

            case 3:
                if( (!trc[GPBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[GPBUFID],"PPM reset (no comms).\n");
                    strcat((char*) &trc_buf[GPBUFID], (char*) &tmp_trc_buf[GPBUFID] );
                }
                else
                    RtPrintf("PPM reset (no comms).\n");
                break;
        }

        if (reset_condition > 0) reset_stats = true;
   }

   if (reset_stats)
   {
      app->pShm->sys_stat.PPMTimer = 1;
      app->isys_reset_bpm = false;
      BpmStats();
   }

//******************************************************************
// This block triggers two differant processes. The first is Bird per
// minute distribution and the second is bird per minute trickle mode
// bpm_qrtr Triggers the distribution and allocation of the correct
// number of birds in the current minute

    switch(pShm->sys_stat.PPMTimer)
    {
        case 6:
        case 11:
        case 21:
        case 26:
        case 36:
        case 41:
        case 51:
        case 56:
            trickle_flag = true;
            break;
        case 1:
            bpm_qrtr = 1;
            trickle_flag = true;
           break;
        case 16:
            bpm_qrtr = 2;
            trickle_flag = true;
            break;
        case 31:
            bpm_qrtr = 3;
            trickle_flag = true;
            break;
        case 46:
            bpm_qrtr = 4;
            trickle_flag = true;
            break;
        default:
            break;
    }

    // process trickle mode counts
    if (trickle_flag) TrickleCounts();
}

//--------------------------------------------------------
//  TrickleCounts
//--------------------------------------------------------
void overhead::TrickleCounts()
{
	//	Reset flag to show we have been called
	trickle_flag = false;

    // Waiting on birds to start being assigned.
    if (!trickle_active)
        return;

	//	Set period duration
	float fPeriodDuration = 60.0 / (float)TRICKLE_SAMPLES;

	// Calculate what period we are in (1 - TRICKLE_SAMPLES) and insure that it can never be higher than TRICKLE_SAMPLES
	int nCurrentPeriod = int((float)(pShm->sys_stat.PPMTimer) / fPeriodDuration) + 1;
	while (nCurrentPeriod > TRICKLE_SAMPLES)
		nCurrentPeriod -= TRICKLE_SAMPLES;

	//	For each drop
	for (int drop = 0; drop < MAXDROPS; drop++)
	{
		// if this drop is trickle mode and it is active
        if ((pShm->Schedule [drop].BpmMode == bpm_trickle ) && pShm->sys_set.DropSettings[drop].Active )
		{
			switch (pShm->Schedule[drop].DistMode)
			{
			case mode_4_rate:
			case mode_5_batch_rate:
			case mode_6_batch_alt_rate:
			case mode_7_batch_alt_rate:
				{
			
					//
					//	Calculate target for both the previous and coming time slice periods
					//

					float fBPMSetPoint = (float)(pShm->Schedule[drop].BPMSetpoint);
					
					// Calculate the total target birds from beginning of the minute through the end of the previous period
					int nPrevTarget = int(((fBPMSetPoint * (float) (nCurrentPeriod - 1) * fPeriodDuration) / 60.0) + .51);

					// Calculate the total target birds from the beginning of the minute through the end of the current period
					int nCurrTarget = int(((fBPMSetPoint * (float) nCurrentPeriod * fPeriodDuration) / 60.0) + .51);

					//	If this drop is not an InterSys drop
					if (!ISYS_ENABLE || !ISYS_DROP(drop, pShm))
					{
					
						//
						//	Adjust drops shortage count based on actual birds dropped vs. target last period
						//

						// if actual bird count for previous period was less than the target
						if (pShm->sys_stat.ScheduleStatus[drop].BPMTrickleCount < target_trickle_count[drop])
						{
							// We have a shortage
							// Add shortage from last period to total shortage
							trickle_short_cnt[drop] += target_trickle_count[drop] - pShm->sys_stat.ScheduleStatus[drop].BPMTrickleCount;
							// Calculate shortage limit as 1/4 of the total BPM
							int short_limit = int(((float)pShm->Schedule[drop].BPMSetpoint / 4.0) + .5);

							// if total shortage is greather than shortage limit, make total shortage equal to shortage limit
							if (trickle_short_cnt[drop] > short_limit)
								trickle_short_cnt[drop] = short_limit;
						}
						else
						{
							// if actual bird count for previous period was more than the target
							if (pShm->sys_stat.ScheduleStatus[drop].BPMTrickleCount > target_trickle_count[drop])
							{
								//	Deduct extra birds from previous period from shortage
								trickle_short_cnt[drop] -=  pShm->sys_stat.ScheduleStatus[drop].BPMTrickleCount - target_trickle_count[drop];
								// if there were more birds than the shortage, make the shortage zero
								if (trickle_short_cnt[drop] < 0)
									trickle_short_cnt[drop] = 0;
							}
						}
					
						//
						//	Calculate target for the coming period
						//

						target_trickle_count[drop] = nCurrTarget - nPrevTarget;

						//
						//	Calculate shortage make up
						//

						// if there are still shortages
						if (trickle_short_cnt[drop])
						{
							// Calculate how many make up birds we are allowed per minute
							int nShortagePeriods = (fBPMSetPoint > 0) ? int(((float)TRICKLE_SAMPLES / (fBPMSetPoint * float(0.10))) +.5) : 0;

							// If BPM is so large that nShortagePeriods became zero, make nShortagePeriods equal to 1 in order to try to make
							//		up a bird every period
							nShortagePeriods = max(nShortagePeriods, 1);
							
							// if we are in a shortage makeup period, add it to the target for the coming period and subtract it from the short count
							if ((nCurrentPeriod % nShortagePeriods) == 0)
							{
								target_trickle_count[drop]++;
								trickle_shorts_added[drop]++;
								trickle_short_cnt[drop]--;
							}
						}
					
						//
						//	Finished calculating target for the coming period
						//

						//	Set Suspended status of drop based on actual BPM vs target BPM
						pShm->sys_stat.DropStatus[drop].Suspended = (pShm->sys_stat.DropStatus[drop].Batched | (pShm->sys_stat.ScheduleStatus[drop].PPMCount < (int(fBPMSetPoint) + trickle_shorts_added[drop]))) ? false : true;

						//	if trickle count from last period was > 0 write line to trace buffer and reset count to zero
						if (pShm->sys_stat.ScheduleStatus[drop].BPMTrickleCount > 0)
						{
							if( (!trc[GPBUFID].buffer_full) && (TraceMask & _TRICKLE_ ) )
							{
								sprintf((char*) &tmp_trc_buf[GPBUFID],"TrklCnts\tReset\tdrop\t%d\n",drop+1);
								strcat((char*) &trc_buf[GPBUFID], (char*) &tmp_trc_buf[GPBUFID] );
							}
							pShm->sys_stat.ScheduleStatus[drop].BPMTrickleCount = 0;
						}

						//	After last drop suspend decision for the minute, reset the count of added birds to zero
						if (pShm->sys_stat.PPMTimer == 1)
							trickle_shorts_added[drop] = 0;

					}	//	End of if not InterSys
					else
					{	//	It is an Intersys drop
						
						//	Gather total of all connected Intersys drops into TotalPPMCount
						int	TotalPPMCount = 0;
		//			    TScheduleSettings*  psched = &pShm->Schedule[drop];  RED - Removed because not used
						TScheduleStatus*    psched_stat = &pShm->sys_stat.ScheduleStatus[drop];

						GET_BPM_TOTAL(drop)

						// Set new target
						target_trickle_count[drop] = min(nCurrTarget - nPrevTarget + TotalPPMCount, nCurrTarget);

						//	Set Suspended status of drop based on actual BPM vs. target_trickle_count for end of this time slice
						pShm->sys_stat.DropStatus[drop].Suspended = (pShm->sys_stat.DropStatus[drop].Batched | (TotalPPMCount < target_trickle_count[drop])) ? false : true;

					}	// End of if Intersys Drop
					break;
				}	//	End of case statement
			default:
				break;
			}	//	End of switch
		}	//	End of if drop is trickle mode and active

	}	// End of for each drop
}

//--------------------------------------------------------
//  SearchLoopUtility
//--------------------------------------------------------

int overhead::SearchLoopUtility(int scale, int cmd, int drp_index)
{
    int start_drp_index = drp_index;
    int loops           = 0;
    int i;

    switch(cmd)
    {
        case clear_drops:

           for (;;)
            {
                if GOOD_INDEX(scale,drp_index)
                {
                    //RtPrintf("clrdrps drop %d\n", drp_index+1);
                    if (!pShm->sys_set.DropSettings[drp_index].Active)
                    {
                        // Do not clear the batch counts for inactive batched drops
                        switch (pShm->Schedule[drp_index].DistMode)
                        {
                            case mode_1:
                            case mode_2:
                            case mode_4_rate:
                                CLEAR_DROP_BATCH(drp_index, pShm)
                                CLEAR_ISYS_DROP_BATCH(drp_index, pShm)
                                break;
                        }

                    }
                    else
                    {
                       CLEAR_DROP_BATCH(drp_index, pShm)
                        CLEAR_ISYS_DROP_BATCH(drp_index, pShm)
                    }

                    if DBG_DROP(drp_index, pShm)
                    {
                        if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                        {
                            sprintf((char*) &tmp_trc_buf[MAINBUFID],"Cleared\tdrop\t%d\n",drp_index + 1);
                            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
                        }
                    }

                    drp_index = pShm->Schedule[drp_index].NextEntry - 1;
                }
                else
                    return ERROR_OCCURED;

                if FINISHED_LOOP(loops)
                    return ERROR_OCCURED;
            }
            break;

        case search_locals:
			{

				int mode_6_suspended_count = 0;

				for (;;)
				{
					if GOOD_INDEX(scale,drp_index)
					{

					  if ( !DROP_OK(drp_index) )
					  {
							SHOW_DROP_STATUS(drp_index, pShm->Schedule[drp_index].DistMode);

							// Mode 6 just looks for one to use next (not susp or batched).
							// Mode 7 doesn't go to the next drop unless BPM_WAIT is
							// false. BPM_WAIT = true means it is ok to run, but suspended
							// on bpm. If false, it falls through and looks at the NextEntry.

							if (pShm->Schedule[drp_index].DistMode == mode_7_batch_alt_rate)
							{

								if ( DROP_USABLE(drp_index)    &&
									(pShm->sys_stat.DropStatus[drp_index].Batched == 0) &&
									(pShm->sys_stat.DropStatus[drp_index].Suspended != 0))
										return wait_on_drop;

								// If intersystems drop, check other lines.
								if ( pShm->sys_set.IsysLineSettings.active[this_lineid-1]            &&
									 pShm->sys_set.IsysDropSettings[drp_index].active[this_lineid-1] &&
									 (isys_comm_fail[this_lineid-1] == 0) )
								{
									for (i = 0; i < MAXIISYSLINES; i++)
									{
										if ( (pShm->sys_set.IsysLineSettings.active[i]            != 0) &&
											 (pShm->sys_set.IsysDropSettings[drp_index].active[i] != 0) &&
											 (isys_comm_fail[i] == 0)  &&
											 (this_lineid - 1 != i) )
										{
											// RED 05-05-10 added test for not batched but suspended
											if ( ((pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_BATCHED)   == 0 ) ||
												 (((pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_BATCHED)   == 0 ) &&
												 ((pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_SUSPENDED) != 0 ) ))
											{
												//RtPrintf("Loc  drop %d waiting line %d drop %d flgs %x\n",
												//         drp_index+1,i+1,
												//         pShm->sys_set.IsysDropSettings[drp_index].drop[i],
												//         pShm->IsysDropStatus[drp_index].flags[i]);

												return wait_on_drop;
											}
										}
									}
								}
							}
							// RED 05-06-10 If we are in mode 6 keep track of all suspended but not batched drops
							if (pShm->Schedule[drp_index].DistMode == mode_6_batch_alt_rate)
							{
								if (DROP_USABLE(drp_index) &&
									(pShm->sys_stat.DropStatus[drp_index].Batched == 0) &&
									(pShm->sys_stat.DropStatus[drp_index].Suspended != 0))
								{
									mode_6_suspended_count++;
								}
							}

							// This does mode 6 also, it just moves to the next one until
							// a usable drop.
							drp_index = pShm->Schedule[drp_index].NextEntry - 1;
						}
						// Found one
						else
							return (drp_index);

						// RED 05-06-10 added return of wait_on_drop if we counted any mode 6 suspended drops
						//		If non of the drops were mode 6, mode_6_suspended_count will always be zero
						if FINISHED_LOOP(loops)
							return (mode_6_suspended_count >  0) ? wait_on_drop : ERROR_OCCURED;
					}
					else
						return ERROR_OCCURED;
				}
			}
            break;

        case search_remotes:

            // Mode 6 goes through all the drops above and if all batched or suspended, we arrive here.
            // If remotes haven't batched, this will be a wait. If Mode 6 wasn't batched above, the
            // suspended flag will clear and continue dropping using search_locals.
            // Mode 7 should be batched completely by this time.

            for (;;)
            {
                if GOOD_INDEX(scale,drp_index)
                {
                    if ( !DROP_OK(drp_index) )
                    {

                        // Let search_locals take care of it when suspend goes false
                        if ( DROP_USABLE(drp_index)   &&
							(pShm->sys_stat.DropStatus[drp_index].Batched == 0) &&
                            (pShm->sys_stat.DropStatus[drp_index].Suspended != 0) )
                            return wait_on_drop;

                        for (i = 0; i < MAXIISYSLINES; i++)
                        {   // Line & drop active, flags not bached, suspended and not this line
                            //RtPrintf("srchrem0 drop %d line %d flgs %x\n",
                            //              drp_index+1, i+1, pShm->IsysDropStatus[drp_index].flags[i]);
							// RED 05-05-10 added test for not batched but suspended                           
							if ( (pShm->sys_set.IsysLineSettings.active[i]            != 0) &&
                                 (pShm->sys_set.IsysDropSettings[drp_index].active[i] != 0) &&
                                (((pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_BATCHED)   == 0 ) ||
                                (((pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_BATCHED)   == 0 ) &&
                                 ((pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_SUSPENDED) != 0 ))) &&
                                 (isys_comm_fail[i] == 0) )
                            {
                                //RtPrintf("Rem drop %d waiting line %d drop %d flgs %x\n",
                                //         drp_index+1,i+1,
                                //         pShm->sys_set.IsysDropSettings[drp_index].drop[i],
                                //         pShm->IsysDropStatus[drp_index].flags[i]);
                                return wait_on_drop;
                            }
                        }
                    }
                    drp_index = pShm->Schedule[drp_index].NextEntry - 1;
                }
                else
                    return ERROR_OCCURED;

                if FINISHED_LOOP(loops)
                    return ERROR_OCCURED;
            }
            break;

        default:
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            break;
    }
    return ERROR_OCCURED;
}

//--------------------------------------------------------
//  SearchLoop
//--------------------------------------------------------

int overhead::SearchLoop(int scale, int start_indx)
{
    int drop_master = 0;
    int next_index;

//----- Look for a local drop which is ok to assign

    if ((next_index = SearchLoopUtility(scale, search_locals, start_indx))      != ERROR_OCCURED)
        return next_index;

//----- All are batched, start over.

    GET_DROP_MASTER(start_indx, pShm)

    // Reset the isys drops, local and remote
    if ( (drop_master == this_lineid) || (drop_master == NOT_AN_ISYS_DROP) )
    {
//----- Local wasn't assigned, see if a remote is still working

        if (drop_master == this_lineid)
        {
            if ((next_index = SearchLoopUtility(scale, search_remotes, start_indx)) != ERROR_OCCURED)
                return wait_on_drop;

            SearchLoopUtility(scale, clear_drops, start_indx);

            //RtPrintf("Sending batch reset ldrop %d\n",start_indx + 1);
            SendDrpManager( DROP_MAINT, start_indx + 1, NULL, isys_drop_batch_loop_reset, isys_send_common, NULL );
        }
        else
            SearchLoopUtility(scale, clear_drops, start_indx);
    }
// If the suspend is reset it would result in more birds being added
// than desired and is not reset above. Leave it suspended, and
// find the next one which is not suspended. The BPM routines
// will clear the suspend flag at the start of the next minute.

    if ((next_index = SearchLoopUtility(scale, search_locals, start_indx)) != ERROR_OCCURED)
        return next_index;

    return no_drop_avaiable;
}

//--------------------------------------------------------
//  SendLineMsg
//--------------------------------------------------------

int overhead::SendLineMsg( int cmd, int line, int var, BYTE *data, int len)
{
    static HANDLE      hLineShm, hLineMutex, hLineSemPost, hLineSemAck;
    static BOOL        bLineInit = FALSE;
    static PAPPIPCMSG  pLMsg;
    TAppIpcMsg*        pLineMsg;
    LONG   i;

    // needs valid configuration
    if (this_lineid == 0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // validate message
    if ( (cmd <= APP_MSG_START) || (cmd >= LAST_APP_MSG) )
        return ERROR_OCCURED;

    if(RtWaitForSingleObject(hInterfaceThreadsOk,WAIT50MS)==WAIT_FAILED)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);

        if (++pShm->mbx_status[LINE_MBX].hung_cnt >= MBX_HUNG_CNT)
            pShm->mbx_status[LINE_MBX].hung = true;
        return ERROR_OCCURED;
    }

    // no problem with mailbox mutex
    pShm->mbx_status[LINE_MBX].hung_cnt = 0;
    pShm->mbx_status[LINE_MBX].hung     = false;

    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bLineInit)
    {
        hLineMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_MUTEX);

        if (hLineMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hLineShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_SHM, (LPVOID*) &pLMsg);

        if (hLineShm==NULL)
        {
            RtCloseHandle(hLineMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hLineSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_POST);

        if (hLineSemPost==NULL)
        {
            RtCloseHandle(hLineShm);
            RtCloseHandle(hLineMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hLineSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_ACK);

        if (hLineSemAck==NULL)
        {
            RtCloseHandle(hLineShm);
            RtCloseHandle(hLineMutex);
            RtCloseHandle(hLineSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bLineInit = TRUE;
    }

    // Don't be recursive here. Don't use TRACE.
    //RtPrintf("App->Line\t[%s]\n",app_msg[cmd - APP_MSG_START]);

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hLineMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    memset(&pLMsg->Buffer, 0, sizeof(mhdr) + len + 1);

    // set a pointer to fill data
    pLineMsg                     = (TAppIpcMsg*) &pLMsg->Buffer;
    pLineMsg->gen.hdr.cmd        = cmd;
    pLineMsg->gen.hdr.destLineId = line;
    pLineMsg->gen.hdr.srcLineId  = this_lineid;

    // miscellaneous stuff
    switch(cmd)
    {
        case ERROR_MSG:
            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->errMsg.text[i] = data[i];

            // make sure it is null terminated
            pLineMsg->errMsg.text[len] = '\0';
            len++;

            pLineMsg->errMsg.severity  = var;
            len += sizeof(var);          // add in the severity length

            pLineMsg->errMsg.hdr.len = len;
            break;

        case DEBUG_MSG:

            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->gen.data[i] = data[i];

            // make sure it is null terminated
            pLineMsg->gen.data[len]   = '\0';
            len++;

            pLineMsg->gen.hdr.len     = len;
            break;

        case SLAVE_PRE_LABEL:
            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->gen.data[i] = data[i];
            pLineMsg->gen.hdr.len     = len;
            break;

        case SHM_WRITE:

            for (i = 0; i < len; i++ )
                pLineMsg->rw_shm.data[i] = data[i];

            pLineMsg->rw_shm.shm_id  = var;
            len += sizeof(pLineMsg->rw_shm.shm_id); // add shm_id to len
            pLineMsg->rw_shm.hdr.len = len;
            break;

        case MASTER_STOPPED:
            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->gen.data[i] = data[i];
            pLineMsg->gen.hdr.len     = len;
            break;

        case MASTER_STARTED:
            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->gen.data[i] = data[i];
            pLineMsg->gen.hdr.len     = len;
            break;

		case LAST_BIRD_CLEARED:
            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->gen.data[i] = data[i];
            pLineMsg->gen.hdr.len     = len;
			break;

		case REMOTE_BATCH_RESET:
            // copy data
            for (i = 0; i < len; i++ )
                pLineMsg->gen.data[i] = data[i];
            pLineMsg->gen.hdr.len     = len;
			break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            RtReleaseMutex( hLineMutex);
            return ERROR_OCCURED;
            break;
    }

    // tell net client where to send and how much
    pLMsg->LineId = line;
    pLMsg->Len    = sizeof(mhdr) + len; // add header length

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pLMsg->Ack = 0;

    if(!RtReleaseSemaphore(hLineSemPost, lAppReleaseCount, NULL))
    {
        RtReleaseMutex( hLineMutex );
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    for (;;)
    {
        if(RtWaitForSingleObject( hLineSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hLineMutex );
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pLMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hLineMutex ))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  SendHostMsg
//--------------------------------------------------------

int overhead::SendHostMsg( int cmd, int var, BYTE *data, int len)
{
    static HANDLE       hShm, hMutex, hSemPost, hSemAck;
    static BOOL         bInit = FALSE;
    static PAPPIPCMSG   pMsg;
    TAppIpcMsg*         pappMsg;
    int i;

    // needs valid configuration
    if (this_lineid == 0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // validate message
    if ( (cmd <= APP_MSG_START) || (cmd >= LAST_APP_MSG) )
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(RtWaitForSingleObject(hInterfaceThreadsOk,WAIT50MS)!=WAIT_OBJECT_0)
    {
        // Interface threads not ready yet (or died) - don't try to send
        return ERROR_OCCURED;
    }

    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bInit)
    {
        hMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_HST_MUTEX);

        if (hMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_HST_SHM, (LPVOID*) &pMsg);

        if (hShm==NULL)
        {
            RtCloseHandle(hMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_HST_SEM_POST);

        if (hSemPost==NULL)
        {
            RtCloseHandle(hShm);
            RtCloseHandle(hMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_HST_SEM_ACK);

        if (hSemAck==NULL)
        {
            RtCloseHandle(hShm);
            RtCloseHandle(hMutex);
            RtCloseHandle(hSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bInit = TRUE;
    }

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);

        if (++pShm->mbx_status[HOST_MBX].hung_cnt >= MBX_HUNG_CNT)
            pShm->mbx_status[HOST_MBX].hung = true;
        return ERROR_OCCURED;
    }

    // no problem with mailbox mutex
    pShm->mbx_status[HOST_MBX].hung_cnt = 0;
    pShm->mbx_status[HOST_MBX].hung     = false;

    // Don't be recursive here. Don't use TRACE.
    //RtPrintf("App->Hst\t[%s]\n",app_msg[cmd - APP_MSG_START]);

    memset(&pMsg->Buffer, 0, sizeof(mhdr) + len + 1);

    // set a pointer to fill data
    pappMsg                     = (TAppIpcMsg*) &pMsg->Buffer;
    pappMsg->gen.hdr.cmd        = cmd;
    pappMsg->gen.hdr.destLineId = HOST_ID;
    pappMsg->gen.hdr.srcLineId  = this_lineid;

    // miscellaneous stuff
    switch(cmd)
    {
        case BLK_DREC_REQ:
            //RtPrintf("Send DREC Block Request\n");
            break;

        case ERROR_MSG:

            // copy data
            for (i = 0; i < len; i++ )
                pappMsg->errMsg.text[i] = data[i];

            // make sure it is null terminated
            pappMsg->errMsg.text[len]   = '\0';
            len++;

            pappMsg->errMsg.severity    = var;
            len += sizeof(var); // add in the severity length

            pappMsg->errMsg.hdr.len     = len;
            break;

        case SHM_READ:
            pappMsg->rw_shm.shm_id      = var;
            pappMsg->rw_shm.hdr.len     = len;
            break;

        case DEBUG_MSG:

            // copy data
            for (i = 0; i < len; i++ )
                pappMsg->gen.data[i] = data[i];

            // make sure it is null terminated
            pappMsg->gen.data[len]   = '\0';
            len++;

            pappMsg->errMsg.hdr.len  = len;
            break;

        case DREC_MSG:
/*
            if (cmd == DREC_MSG)
            {
                int drlen  = len/sizeof(app_drp_rec);
                int lenchk = len % sizeof(app_drp_rec);
                app_drp_rec* dr = (app_drp_rec*) &data[0];

                if (lenchk != 0) RtPrintf("Odd length %d\n",lenchk);

                for (int i = 0; i < drlen; i++)
                {
                    RtPrintf("seq\t%x\tshkl\t%d\tweight\t%d\tdrop\t%d\tlst\t%d\tbch\t%d\n",
                              dr[i].seq_num,
                              dr[i].d_rec.shackle,
                              dr[i].d_rec.weight [0],
                              dr[i].d_rec.drop   [0],
                              dr[i].d_rec.last_in_batch[0] & 1,
                              dr[i].d_rec.batch_number [0] & BATCH_MASK);
                }
            }
*/
        case PRN_BCH_REQ:
        case PRN_BCH_PRE_REQ:
        case DREC_RESP:

            for (i = 0; i < len; i++ )
                pappMsg->gen.data[i] = data[i];
            pappMsg->gen.hdr.len     = len;

            // set drop rec recovery bit or other flag bit
            pappMsg->gen.hdr.flags   = var;
            break;

        case LC_CAPTURE_INFO:

            {
//                int *rd1  = (int*) data;		RED - Removed becuase it is not referenced
//                int *rd2  = (int*) &data[4];	RED - Removed becuase it is not referenced
//                int *dta  = (int*) &data[8];	RED - Removed becuase it is not referenced

                // copy message data
                for (i = 0; i < len; i++ )
                    pappMsg->gen.data[i] = data[i];

                pappMsg->gen.hdr.len = len;

                //RtPrintf("\nWeight read1 %d read2 %d samples %d\n",
                //          *rd1,*rd2, ((len/4) - 2) );
/*
                // the loop is just debug (NEEDS UPDATING, now has 4 read indexes
                for (i = 0; i < ((len/4) - 2); i++ )
                {
                    if ( (i != *rd1) && (i != *rd2 ) )
                        RtPrintf("%11d ", *dta);
                    else
                        RtPrintf("%11d*", *dta);
                    if ( (i%8) == 0) RtPrintf("\n");

                    dta++;
                }
            RtPrintf("\n");
*/
            }
            break;

        case SHM_WRITE:

            for (i = 0; i < len; i++ )
                pappMsg->rw_shm.data[i] = data[i];

            pappMsg->rw_shm.shm_id  = var;
            len += sizeof(pappMsg->rw_shm.shm_id); // add shm_id to len
            pappMsg->rw_shm.hdr.len = len;

#ifdef _SHMW_TIMING_TEST_
            {
                static unsigned int snd_seq_num = 1;

                // if flags == 1, host will send ack
                pappMsg->gen.hdr.flags   = 1;
                // Ok to get a new sequence number to check
                if (shmw_seq_done == 0)
                {
                    RtGetClockTime(CLOCK_FASTEST,&shmw_StartTime);
                    shmw_seq_done            = snd_seq_num;
                    pappMsg->gen.hdr.seq_num = snd_seq_num;
                    shmw_seq_tmo             = SHMW_TMO;

                    if(++snd_seq_num >= 0xFFFFFFF0)
                        snd_seq_num   = 1;
                }
            }
#endif
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            RtReleaseMutex( hMutex);
            return ERROR_OCCURED;
            break;
    }

    // tell net client where to send and how much
    pMsg->LineId = HOST_ID;
    pMsg->Len    = sizeof(mhdr) + len; // add header length

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pMsg->Ack = 0;

    if(!RtReleaseSemaphore(hSemPost, lAppReleaseCount, NULL))
    {
        RtReleaseMutex( hMutex);
        RtPrintf("Error file %s, line %d hnd %u\n",_FILE_, __LINE__,(int) hSemPost);
        return ERROR_OCCURED;
    }

// Response timing check for pipe blocked duration
#ifdef _HOST_TIMING_TEST_
    RtGetClockTime(CLOCK_FASTEST,&msg_StartTime);
#endif

   {
        // Wait for ACK with timeout (prevents permanent deadlock if interface dies)
        int ack_retries = 0;
        for (;;)
        {
            int ack_result = RtWaitForSingleObject( hSemAck, 2000);  // 2 second timeout
            if (ack_result == WAIT_FAILED)
            {
                RtReleaseMutex( hMutex);
                RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
                return ERROR_OCCURED;
            }

            if (pMsg->Ack != (LONG) 0)
                break;

            // Timeout or spurious wakeup - retry with limit
            if (ack_result == WAIT_TIMEOUT || ++ack_retries > 5)
            {
                RtReleaseMutex( hMutex);
                if (ack_retries > 5)
                    RtPrintf("SendHostMsg: ACK not received after retries, cmd=%d file %s line %d\n", cmd, _FILE_, __LINE__);
                return ERROR_OCCURED;
            }
        }
    }

#ifdef _HOST_TIMING_TEST_

    RtGetClockTime(CLOCK_FASTEST,&msg_EndTime);
    msg_time = (UINT) ((msg_EndTime.QuadPart - msg_StartTime.QuadPart)/10000);

    if (msg_time > 20)
    {
        if (cmd == SHM_WRITE)
	        RtPrintf("SendHst\tcmd %d id %d time %u ms\n", cmd, var, msg_time);
        else
	        RtPrintf("SendHst\tcmd %d time %u ms\n", cmd, msg_time);
    }
#endif

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hMutex))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    //RtPrintf("EApp->Hst\t[%s]\n",app_msg[cmd - APP_MSG_START]);
    return NO_ERRORS;
}

//--------------------------------------------------------
//  SendDrpManager
//
//  This routine fills in the Ipc message structure,
//  and sends the command.
//--------------------------------------------------------

int overhead::SendDrpManager(int cmd, int drop, int var1, int var2, int var3, int var4)
{
    TDrpMgrIpcMsg dm_msg;
    CHAR*         pdm_msg;
    int i;

    // needs valid configuration
    if (this_lineid == 0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Due to the volume of IPC messages, these will not go to host
    DebugTrace(_IPC_, "App->DMgr\t[%s]\n", dmgr_msg[cmd - DRPMGR_MSG_START]);

    if(RtWaitForSingleObject(hInterfaceThreadsOk,WAIT100MS)==WAIT_FAILED)
        return ERROR_OCCURED;

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hDrpMgrMutex, WAIT100MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d\n",_FILE_, __LINE__);

        if (++pShm->mbx_status[DMGR_MBX].hung_cnt >= MBX_HUNG_CNT)
            pShm->mbx_status[DMGR_MBX].hung = true;

        return ERROR_OCCURED;
    }

    // no problem with mailbox mutex
    pShm->mbx_status[DMGR_MBX].hung_cnt = 0;
    pShm->mbx_status[DMGR_MBX].hung     = false;

    // Build message to send other line via drop manager
    switch(cmd)
    {
        case DROP_MAINT:
            dm_msg.maint.cmd        = cmd;
            dm_msg.maint.drop_num   = drop;
            dm_msg.maint.slave_indx = var1;
            dm_msg.maint.action     = var2;
            dm_msg.maint.extent     = var3;
            dm_msg.maint.batch      = var4;
            break;

        case CHECK_DROP_LOCAL:
            dm_msg.ck_drp.cmd       = cmd;
            dm_msg.ck_drp.drop_num  = drop;
            dm_msg.ck_drp.scale     = var1;
            dm_msg.ck_drp.shackle   = var2;
            dm_msg.ck_drp.nothing   = 0;
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            RtReleaseMutex( hDrpMgrMutex );
            return ERROR_OCCURED;
            break;
    }

    //
    // Copy the message string.
    //
    pdm_msg = (CHAR*) &dm_msg;
    for (i = 0; i < sizeof(TDrpMgrIpcMsg); i++ )
        pDrpMgrIpcMsg->Buffer[i] = pdm_msg[i];

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pDrpMgrIpcMsg->Ack = 0;

    if(!RtReleaseSemaphore(hDrpMgrSemPost, lAppReleaseCount, NULL))
    {
        RtReleaseMutex( hDrpMgrMutex);
        RtPrintf("Error file %s, line %d err %d\n",_FILE_, __LINE__, GetLastError());
        return ERROR_OCCURED;
    }

    for (;;)
    {

        if(RtWaitForSingleObject( hDrpMgrSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hDrpMgrMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pDrpMgrIpcMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hDrpMgrMutex))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    DebugTrace(_IPC_, "App->DMgr\t[%s]\tOk\n", dmgr_msg[cmd - DRPMGR_MSG_START]);

    return NO_ERRORS;
}

//--------------------------------------------------------
//  SendIsysMsg
//--------------------------------------------------------

int overhead::SendIsysMsg( int cmd, int lineId, BYTE *data, int len)
{
	len;	//	RED - Added to eliminate non-referenced formal parameter error
	data;	//	RED - Added to eliminate non-referenced formal parameter error
    TIsysIpcMsg isys_msg;
    CHAR*       pisys_msg;
    int i;

    // needs valid configuration
    if (this_lineid == 0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(RtWaitForSingleObject(hInterfaceThreadsOk,WAIT50MS)==WAIT_FAILED)
        return ERROR_OCCURED;

    memset(&isys_msg,0,sizeof(isys_msg));

    // Due to the volume of IPC messages, these will not go to host
    DebugTrace(_IPC_, "App->Isys\t[%s]\n",isys_msg_desc[cmd - ISYS_MSG_START]);

    // Build message for intersystems

    switch(cmd)
    {
        case ISYS_PORTCHK:
            isys_msg.imsg.cmd    = cmd;
            isys_msg.imsg.lineId = lineId;
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            return ERROR_OCCURED;
            break;
    }

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hIsysMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error %d file %s, line %d \n",GetLastError(), _FILE_, __LINE__);

        if (++pShm->mbx_status[ISYS_MBX].hung_cnt >= MBX_HUNG_CNT)
            pShm->mbx_status[ISYS_MBX].hung = true;

        return ERROR_OCCURED;
    }

    // no problem with mailbox mutex
    pShm->mbx_status[ISYS_MBX].hung_cnt = 0;
    pShm->mbx_status[ISYS_MBX].hung     = false;

    //
    // Copy the message string.
    //
    pisys_msg = (CHAR*) &isys_msg;
    for (i = 0; i < sizeof(TIsysIpcMsg); i++ )
        pIsysIpcMsg->Buffer[i] = pisys_msg[i];

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pIsysIpcMsg->Ack = 0;

    if(!RtReleaseSemaphore(hIsysSemPost, lAppReleaseCount, NULL))
    {
        RtReleaseMutex( hIsysMutex);
        RtPrintf("Error file %s, line %d hnd %u\n",_FILE_, __LINE__,(int) hIsysSemPost);
        return ERROR_OCCURED;
    }

    for (;;)
    {

        if(RtWaitForSingleObject( hIsysSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hIsysMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pIsysIpcMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hIsysMutex))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    DebugTrace(_IPC_, "App->Isys\t[%s]\tOk\n",isys_msg_desc[cmd - ISYS_MSG_START]);

    return NO_ERRORS;
}

//--------------------------------------------------------
//  AssignDrop
//--------------------------------------------------------

bool overhead::AssignDrop(int scale, int drop, TShackleStatus* pShk )
{
    TScheduleSettings *pSch;
    TScheduleStatus   *pSchStat;
//	TShackleStatus    *pShk;	RED replaced with passed in parameter
    int  drop_plus1 = drop + 1;
//    bool bch_stat   = false;	RED - Removed because not referenced

//----- Short cuts

    pSch     = &pShm->Schedule[drop];
    pSchStat = &pShm->sys_stat.ScheduleStatus[drop];
//    pShk     = &WEIGH_SHACKLE(scale, pShm);

//----- Show assigned

    if DBG_DROP(drop, pShm)
    {
        if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
        {
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"Assgn'd\tdrop\t%d\tshkl\t%d\twt\t%d\tgrd\t%c\n", drop_plus1,
                                 pShm->WeighShackle[scale],
                                 pShk->weight[scale],
                                 pShm->sys_set.GradeArea[pShk->GradeIndex[scale]].grade);  // added 3/16/2006
            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID]);
        }
    }

//----- Miscellaneous

    if (pSch->Shrinkage > 0)
        pShk->weight[scale] -= pSch->Shrinkage;

    pShk->drop[scale] = drop_plus1;

//----- Add to drop Books

    AddBird(pShk->GradeIndex[scale], drop_plus1, pShk->weight[scale]); // added 3/16/2006

//----- Non intersystems, stop dropping when:

    // 1. Batch(BchCount) reached
    // 2. Batch(BchWtSp) reached
    // 3. BPM reached

    switch(pSch->DistMode)
    {
        case mode_3_batch:
        case mode_5_batch_rate:
        case mode_6_batch_alt_rate:
        case mode_7_batch_alt_rate:

            // 1. Batch(BchCount)
            pSchStat->BchActCnt++;
            pShk->batch_number[scale] = pShm->sys_stat.DropStatus[drop].batch_number;

            if (pSch->BchCntSp > 0)
            {
                if (pSchStat->BchActCnt >= pSch->BchCntSp)
                {
                    // intersystems will decide whether batched in drop manager
                    if (!ISYS_DROP(drop, pShm))
                    {
                        pShm->sys_stat.DropStatus[drop].Batched  = 1;
                        pShk->last_in_batch[scale] = true;

                        if DBG_DROP(drop, pShm)
                        {
                            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & ( _FDROPS_ | _LABELS_ )) )
                            {
                                sprintf((char*) &tmp_trc_buf[MAINBUFID],"Btch'd* Drop\tdrop\t%d\tBchCount\t%u\tActCount\t%u\tBchNo\t%u\tLst\t%d\n",
                                                 drop_plus1, (UINT) pSch->BchCntSp, (UINT) pSchStat->BchActCnt,
                                                 pShm->sys_stat.DropStatus[drop].batch_number & BATCH_MASK,
                                                 pShm->sys_stat.DropStatus[drop].last_in_batch & 1);
                                strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
                            }
                        }
                    }
                    //break; //commented out for debugging jdc
                }
            }

            // 2. Batch(BchWtSp)
            pSchStat->BchActWt += pShk->weight[scale];

            if ( pSch->BchWtSp > 0)
            {

                if (pSchStat->BchActWt >= pSch->BchWtSp)
                {
                    // intersystems will decide whether batched in drop manager
                    if (!ISYS_DROP(drop, pShm))
                    {
                        pShm->sys_stat.DropStatus[drop].Batched  = 1;
                        pShk->last_in_batch[scale] = true;

                        if DBG_DROP(drop, pShm)
                        {
                            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & ( _FDROPS_ | _LABELS_ )) )
                            {
                                sprintf((char*) &tmp_trc_buf[MAINBUFID],"Btch'd* Drop\tdrop\t%d\tBchWtSp\t%u\tBchActWt\t%u\tBchNo\t%u\tLst\t%d\n",
                                                drop_plus1, (UINT) pSch->BchWtSp, (UINT) pSchStat->BchActWt,
                                                pShm->sys_stat.DropStatus[drop].batch_number & BATCH_MASK,
                                                pShm->sys_stat.DropStatus[drop].last_in_batch & 1);
                                strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
                            }
                        }
                    }
                }
            }

            break;

        default:
            break;
    }

    // 3. BPM count

    switch(pSch->DistMode)
    {
        // Modes not based on BPM inc for interface
        case mode_1:
        case mode_2:
        case mode_3_batch:
            bpm_act_count[scale]++;
            pSchStat->PPMCount++;
            break;

        case mode_4_rate:
        case mode_5_batch_rate:
        case mode_6_batch_alt_rate:
        case mode_7_batch_alt_rate:

           bpm_act_count[scale]++;
           pSchStat->PPMCount++;

		   // If Intersystems drop Intersystems will decide whether to batch or suspend
		   if (!ISYS_DROP(drop, pShm))
		   {

			   // inc trickle count
				if (pSch->BpmMode == bpm_trickle)
				{
					pSchStat->BPMTrickleCount++;

					TRICKLE_INFO("Assign'd",drop_plus1);

					if (pSchStat->BPMTrickleCount >= target_trickle_count[drop])
					{
						pShm->sys_stat.DropStatus[drop].Suspended  = true;
						TRICKLE_INFO("TSusp'd*",drop_plus1);
					}
				}

				if (pSch->BPMSetpoint > 0)
				{
					if (pSchStat->PPMCount >= pSch->BPMSetpoint)
					{
						// intersystems will decide whether batched in drop manager
						pShm->sys_stat.DropStatus[drop].Suspended  = true;

						if DBG_DROP(drop, pShm)
						{
							if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
							{
								sprintf((char*) &tmp_trc_buf[MAINBUFID],"Susp'd* Drop\tdrop\t%d\tBPMSetpt\t%u\tPPMCount\t%u\n",
										drop_plus1, (UINT) pSch->BPMSetpoint, (UINT) pSchStat->PPMCount);
								strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
							}
						}
					}
				}
		   }

			break;

        default:
            break;
    }

//----- If inter-systems drop, let drop manager handle it.

    if ( ISYS_ENABLE && ISYS_DROP(drop, pShm) )
        SendDrpManager( CHECK_DROP_LOCAL, drop /* -1 for CHECK_DROP_LOCAL */, scale, pShm->WeighShackle[scale], NULL, NULL);

    // always return true, a shortcut to set drop_found
    return true;
}

//--------------------------------------------------------
//  FindDrops
//
//  Compare weight with limits and put drop number
//  into shackle data entry.
//--------------------------------------------------------

void overhead::FindDrops(int Scale, int StartAt, int Shackle)
{
   __int64  gib;
   int  drp_chk;
   int  scale       = Scale - 1;
   bool drop_found  = false;
   int  order_index = 0;
   int  chk_cnt     = 0;
   int  chkrng_ret_cd;
   int  drop_master;
   bool assign_ok;
   TShackleStatus* pShk = &pShm->ShackleStatus[(StartAt == 0) ? pShm->WeighShackle[scale] : Shackle];

#ifdef _TEST_RIG_
   double tscale1,tscale2;
#endif

//----- Check to see if there is a bird

	if (pShk->weight[scale] < pShm->sys_set.MinBird)
        return;

    if (dual_scale)
    {

      if (scale == 1)
      {

#ifdef _SIMULATION_MODE_
        if (pShk->drop[0] == 7) //wkr, for debugging scale 2
        {
#endif

#ifdef _TEST_RIG_
            // scale 1 counts/lb = 1559
            //       2              450
            tscale1 = (double) pShk->weight[0];
            tscale2 = pShk->weight[1];

            RtPrintf("WShkl[1] %d Drp[0] %d Wt[0] %d  Drp[0] %d Wt[1] %d\n",
                      pShm->WeighShackle[1],
                      pShk->drop[0],
                      (int) tscale1,
                      pShk->drop[1],
                      (int) tscale2);

             if (pShm->sys_set.GibMin > 0)
                gib  = (int) (tscale2 - tscale1);
#else
             //RtPrintf("WShkl[1] %d Drp[0] %d Wt[0] %d  Drp[0] %d Wt[1] %d\n",
             //         pShm->WeighShackle[1],
             //         pShk->drop[0],
             //         pShk->weight[0],
             //         pShk->drop[1],
             //         pShk->weight[1]);

             if (pShm->sys_set.GibMin > 0)
                gib  = pShk->weight[1] -
                       pShk->weight[0];
#endif
             else
                // no gibmin, fake gib weight, just let it go normally by schedule
                gib  = pShm->sys_set.GibMin + 50;

             //RtPrintf("gib %d gibmin %d\n", gib, pShm->sys_set.GibMin);

             // didn't pick up enough weight or
             // not using gib feature (should have been taken off),
             // deposit it at gibdrop
             if ( (gib < pShm->sys_set.GibMin) || (pShk->drop[1] != 0) )
             {
                pShk->drop[1] = pShm->sys_set.GibDrop;
                AddBird(pShk->GradeIndex[1], // added 3/16/2006
                          pShk->drop[1],
                          pShk->weight[1]);

                if DBG_DROP(pShk->drop[1] - 1, pShm)
                {
                    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[MAINBUFID],"FindDrops\tgib\tdrop\t%d\tshkl\t%d\twt\t%d\n",
                                      pShk->drop[1],
                                      pShm->WeighShackle[1],
                                      pShk->weight[1]);
                        strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
                    }
                }

                return;
             }

#ifdef _SIMULATION_MODE_
          }
          else return;
#endif
      }
   }//end dual scale

//----- Bird is greater than MinBird

   while ( (order_index < pShm->sys_set.NumDrops) && (!drop_found))
    {
        drp_chk  = pShm->sys_set.Order[order_index].Unload - 1;

		if (drp_chk >= StartAt)
		{

			//----- Be sure this drop is safe

			if GOOD_INDEX(scale,drp_chk)
			{

	//----- In case the mode changed or diasabled with flags set.
	//      Clear unrelated susp/batch status.

				//	CLEAR_MODE_DATA
				//----- A few resets, in case the mode changed with flags set. 
				//      The drop will not restart if they aren't cleared. Under
				//      bpm_trickle, a drop should not be assigned if the target 
				//      value is 0 (convienent place to put this) and trickle_active 
				//      is set true to run trickle mode.
        

				if (pShm->sys_set.DropSettings [drp_chk].Active)
				{
					switch(pShm->Schedule[drp_chk].DistMode)
					{
						case mode_1:
						case mode_2:
							pShm->sys_stat.DropStatus[drp_chk].Suspended = false;
						case mode_4_rate:
							if (pShm->sys_stat.DropStatus[drp_chk].batch_number != 0)
							{
								CLEAR_DROP_BATCH(drp_chk, pShm)
							}
							break;
						case mode_3_batch:
							pShm->sys_stat.DropStatus[drp_chk].Suspended = false;
							break;
						default:
						   break;
					}

					switch(pShm->Schedule[drp_chk].BpmMode)
					{
						case bpm_normal:
							trickle_short_cnt[drp_chk] = 0;
							break;
						case bpm_trickle:
							if (target_trickle_count[drp_chk] == 0)
								pShm->sys_stat.DropStatus[drp_chk].Suspended = true;
							trickle_active = true;
							break;
						default:
						   break;
					}
				}
	//			else
	//			{
	//				CLEAR_DROP_BATCH(drp_chk, pShm)
	//				pShm->sys_stat.ScheduleStatus [drp_chk].PPMCount   = 0;
	//				pShm->sys_stat.DropStatus[drp_chk].Suspended       = false;
	//				trickle_short_cnt[drp_chk]                         = 0;
	//			}

	//----- Validate drop and examine the status

				switch(pShm->Schedule[drp_chk].DistMode)
				{
					// No loop.
					case mode_1:
					case mode_2:
					case mode_3_batch:
					case mode_4_rate:
					case mode_5_batch_rate:

						if DROP_OK(drp_chk)
						{
							GET_DROP_MASTER(drp_chk, pShm)
							APPLY_BATCH_NUMBER(drp_chk, drop_master)
							SET_ASSIGN_OK(assign_ok, drop_master)

							if (assign_ok)
							{
								if CHK_RANGE(drp_chk, scale, pShk)
									drop_found = AssignDrop(scale, drp_chk, pShk);
								else
									CHK_RANGE_STATUS(drp_chk, pShm, scale)
							}
						}
						else
						{
							SHOW_DROP_STATUS(drp_chk,pShm->Schedule[drp_chk].DistMode);
						}
						break;

					// Loop Modes

					// Mode 6 just looks through the loop for the next one to use
					// Mode 7 must complete batch on a drop before going to the next one.

					case mode_6_batch_alt_rate:
					case mode_7_batch_alt_rate:

						// Always start with the beginning of a loop (LoopOrder == 1).
						if (pShm->Schedule[drp_chk].LoopOrder == 1)
						{
							drp_chk = SearchLoop(scale,drp_chk);

							if GOOD_INDEX(scale,drp_chk)
							{
								GET_DROP_MASTER(drp_chk, pShm)
								APPLY_BATCH_NUMBER(drp_chk, drop_master)
								SET_ASSIGN_OK(assign_ok, drop_master)

								if (assign_ok)
								{
									if CHK_RANGE(drp_chk, scale, pShk)
										drop_found = AssignDrop(scale, drp_chk, pShk);
									else
										CHK_RANGE_STATUS(drp_chk, pShm, scale)
								}
							}
						}
						break;

					default:
					   // Don't need this message, just let the bad mode live
					   // in peace. It should be up to the interface to check this.
						//RtPrintf("Mode Error Drop %d file %s, line %d \n", drp_chk + 1, _FILE_, __LINE__);
					   break;
				}

			} // drop is safe

			//RtPrintf("Bad Drop %d scale %d\n", drp_chk + 1, scale);
		}		//	End if (drp_chk >= StartAt)

        if (!drop_found )
        {
           chk_cnt++;

           // Looped through all the drops
           if (chk_cnt >= pShm->sys_set.NumDrops)
               return; //************ add new over/under code here JC **************

           // Increment index or start at the begining of order
           if (order_index < pShm->sys_set.NumDrops - 1)
                order_index++;
           else
                order_index = 0;
        }
    } // while (... && (! drop_found)) do
}

//--------------------------------------------------------
//  CheckRange
//--------------------------------------------------------

int overhead::CheckRange(int drop, int scale, TShackleStatus* pShk)
{
    int i;
    TScheduleSettings*  psched;
    TScheduleStatus*    psched_stat;
    DBOOL               BPMLast15sec_ok       = true;
    DBOOL               BPMPrevLast15sec_ok   = true;
    bool                grade_OK              = false;
    bool                grade_ext_OK          = false;
    bool                weight_OK             = false;
    int                 BPMSetpoint           = 0;
    int                 TotalPPMCount         = 0;
    __int64             weight;
    int                 grade;

    psched      = &pShm->Schedule[drop];
    psched_stat = &pShm->sys_stat.ScheduleStatus[drop];

//----- Some shortcuts/fudging to simplify things

    // for weight

    weight = pShk->weight[scale];

    if (psched->Shrinkage > 0)
        weight -= psched->Shrinkage;

    if  WEIGHT_CHECK( weight, psched->MinWeight, psched->MaxWeight )
        weight_OK = true;

    // for grading, extended grade is checked in rate limiting cases.
    // If no no grading, just say it's OK so logic works for both.

    if (pShm->sys_set.Grading) 
	{
		//	CHECK_GRADE
		//----- See if the grade assigned to the shackle matches one in the schedule

		if (pShm->sys_set.ResetGrading.ResetScale1Grade > 0)
			grade = pShm->sys_set.GradeArea[pShk->GradeIndex[scale]].grade;
		else\
			grade = pShm->sys_set.GradeArea[pShk->GradeIndex[0]].grade;
//DRW Added for resetting grade on empty shackles only
		if (pShm->sys_set.ResetGrading.ResetS1Empty > 0)
			grade = pShm->sys_set.GradeArea[pShk->GradeIndex[scale]].grade;
		else
			grade = pShm->sys_set.GradeArea[pShk->GradeIndex[0]].grade;



		for (i = 0; i <= MAXGRADES - 1; i++)
		{
			if (psched->Grade[i] == grade)
			{
				grade_OK  = true;
				break;
			}
		}
	}
    else
    {
        grade_OK = true;
        grade    = 'A';
    }

//----- Distribution specific logic

    switch(psched->DistMode)
    {

        case mode_1: // no grading
            grade_OK = true;
        case mode_2:
        case mode_3_batch:

            //----- Only normal weight/grade for these modes

            if ( (weight_OK) && (grade_OK) )
                {
                     CHK_RANGE_OK("       NORMAL", pShm)
                }

            break;

        case mode_4_rate:
        case mode_5_batch_rate:
        case mode_6_batch_alt_rate:
        case mode_7_batch_alt_rate:

//----- Calculate total BPM, adds isys BPMs if enabled

            GET_BPM_TOTAL(drop)

//----- Figure out the last 30-60 second performance based on last 2 quarters
//      of the last minute and set current quarter to ok/not ok for next quarter.

            //	CHECK_BPM_PERFORMANCE
			//----- Figure out by bpm_qrtr, how the rate modes are doing

			switch(bpm_qrtr)
			{
			
				case 1:
					BPMLast15sec_ok             = psched_stat->BPM_4thQ_ok;
					BPMPrevLast15sec_ok         = psched_stat->BPM_3rdQ_ok;
					BPMSetpoint                 = psched->BPMSetpoint / 4;
					psched_stat->BPM_1stQ_ok    = ((TotalPPMCount < BPMSetpoint) ? false : true);
					break;
				case 2:
					BPMLast15sec_ok             = psched_stat->BPM_1stQ_ok;
					BPMPrevLast15sec_ok         = psched_stat->BPM_4thQ_ok;
					BPMSetpoint                 = psched->BPMSetpoint / 2;
					psched_stat->BPM_2ndQ_ok    = ((TotalPPMCount < BPMSetpoint) ? false : true);
					break;
				case 3:
					BPMLast15sec_ok             = psched_stat->BPM_2ndQ_ok;
					BPMPrevLast15sec_ok         = psched_stat->BPM_1stQ_ok;
					BPMSetpoint                 = (psched->BPMSetpoint / 4) * 3;
					psched_stat->BPM_3rdQ_ok    = ((TotalPPMCount < BPMSetpoint) ? false : true);
					break;
				case 4:
					BPMLast15sec_ok             = psched_stat->BPM_3rdQ_ok;
					BPMPrevLast15sec_ok         = psched_stat->BPM_2ndQ_ok;
					BPMSetpoint                 = psched->BPMSetpoint;
					psched_stat->BPM_4thQ_ok    = ((TotalPPMCount < BPMSetpoint) ? false : true);
					break;
				default:
					RtPrintf("Error bpm_qrtr %d file %s, line %d \n",
							  bpm_qrtr, __FILE__, __LINE__);
					return (false);
					break;
			}

            // Check extended grade
            if (pShm->sys_set.Grading)
			{
				//	CHECK_EXT_GRADE
				if (pShm->sys_set.DropSettings[drop].Extgrade)
				{
					for (i = 0; i <= MAXGRADES - 1; i++)
					{
						if (psched->Extgrade[i] == grade)
						{
							grade_ext_OK  = true;
							break;
						}
					}
				}
			}

//----- Normal weight, may take an extended grade if needed

            if ( weight_OK )
            {
                if ( grade_OK )
                {
                     CHK_RANGE_OK("       NORMAL", pShm)
                }
                else if ( ( grade_ext_OK ) && (!BPMLast15sec_ok) )
                {
                     CHK_RANGE_OK("EXT GRADE", pShm)
                    grade_OK  = true;
                }
            }

//---- Weight not OK & BPM is low for 15 seconds, check extended weight/grade

            else
            {
                if ( (grade_OK || grade_ext_OK) && (!BPMLast15sec_ok) )
                {
                    // weight is not ok, check extended
                    if WEIGHT_CHECK( weight, psched->ExtMinWeight, psched->ExtMaxWeight )
                    {
                        weight_OK  = true;

                        if ( grade_OK )
                        {
                             CHK_RANGE_OK("   EXT WEIGHT", pShm)
                        }
                        else
                        {
                             CHK_RANGE_OK("   EXT WT&GRD", pShm)
                            grade_OK   = true;
                        }
                    }

//---- Weight not OK & BPM is low for last two 15 sec. periods, Check extended 2 grade/range

                    else if ( (!BPMPrevLast15sec_ok)          &&
                              WEIGHT_CHECK( weight, psched->Ext2MinWeight, psched->Ext2MaxWeight) )
                    {
                        weight_OK  = true;

                        if ( grade_OK )
                        {
                             CHK_RANGE_OK(  "EXT2 WEIGHT", pShm)
                        }
                        else
                        {
                             CHK_RANGE_OK("  EXT2 WT&GRD", pShm)
                            grade_OK   = true;
                        }
                    }
                    // No weights in range, show it is really a weight
                    // problem (for debug below, hits grade_OK check first).
                    else grade_OK   = true;
                }
            }
            break; // cases doing rate limiting

            default:
            break;

    } // switch dist mode

//----- Drum roll... the decision is...

    if ( weight_OK && grade_OK )
        // rah! rah!
        return drop_it;

//----- Debug info below

    else
    {

// Mabye something below will help find the problem

        //	SHOW_MISC_INFO
		if (DBG_DROP(drop, pShm))
		{
			if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _CHKRNG_ ) )
			{
				sprintf((char*) &tmp_trc_buf[MAINBUFID],"\nCheckRange --\tFail\tdrop\t%d\tshkl\t%d\tmode\t%u\tbpmd\t%u\n",
									drop+1, pShm->WeighShackle[scale], psched->DistMode, psched->BpmMode);
				strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
			}
		}

// rate related

        switch(psched->DistMode)
        {

            case mode_4_rate:
            case mode_5_batch_rate:
            case mode_6_batch_alt_rate:
            case mode_7_batch_alt_rate:
                //	SHOW_BPM_INFO
				//----- If rate modes, show BPM info
				if (DBG_DROP(drop, pShm))
				{
					if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _CHKRNG_ ) )
					{
					  sprintf((char*) &tmp_trc_buf[MAINBUFID],"\t\t\tLst15ok\t%1.1x\tPrv15ok\t%1.1x\tTBPMCt\t%u\tBPMSp\t%u\n",
										  BPMLast15sec_ok, BPMPrevLast15sec_ok, TotalPPMCount, BPMSetpoint);
					  strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
					}
				}
	            break;
            default:
	            break;
        }

// grade

        if (!grade_OK)
        {
            //	SHOW_GRADE_INFO
			//----- If failed on grade, show grade info
			if (DBG_DROP(drop, pShm))
			{
				if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _CHKRNG_ ) )
				{
				  sprintf((char*) &tmp_trc_buf[MAINBUFID],"\t\t\tgrading\t%1.1x\tgrdOK\t%1.1x\texgrdOK\t%1.1x\textgrd\t%4.4s\n",
									   pShm->sys_set.Grading,  grade_OK,  grade_ext_OK, &psched->Extgrade);
				  strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
				  sprintf((char*) &tmp_trc_buf[MAINBUFID],"\t\t\tprigrd\t%4.4s\tareagrd[%d]\t%1c\n",
									   &psched->Grade,
									   WEIGH_SHACKLE(scale, pShm).GradeIndex[scale],
									   pShm->sys_set.GradeArea[WEIGH_SHACKLE(scale, pShm).GradeIndex[scale]].grade);
				  strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
				}
			}
            return grade_out_of_range;
        }

// weight

        else if (!weight_OK)
        {
            //	SHOW_WEIGHT_INFO
			//----- If failed on weight, show weight info
			if (DBG_DROP(drop, pShm))
			{
				if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _CHKRNG_ ) )
				{
				  sprintf((char*) &tmp_trc_buf[MAINBUFID],"\t\twt\t%u\tMin\t%u\tMax\t%u\n",
									  weight, psched->MinWeight, psched->MaxWeight);
				  strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
				}
			}
            return weight_out_of_range;
        }

// not a clue

        else
            return hole_in_logic;
    }
}

//--------------------------------------------------------
// GradeProcess
//--------------------------------------------------------
void overhead::GradeProcess(int GradeSyncIndex)
{
    const int grade_bit  [MAXGRADES] = {NULL, GRADE1_BIT, GRADE2_BIT,
        GRADE3_BIT, NULL /* GRADE4_BIT */};
    const int grade_index[MAXGRADES] = {0,1,2,3,4};

    int i, shackle;

	// RCB - We should always reset the grade since we must assume the bird was dropped
	if(MB_ENABLED(0))
	{
		pShm->sys_set.ResetGrading.ResetGrade = true;
	}

    //RtPrintf("Grade process\n");
    for (i = 1; i < MAXGRADES; i++)
    {
        if ((grade_bit[i] != NULL) && (pShm->sys_set.GradeArea[grade_index[i]].GradeSyncUsed == GradeSyncIndex))
        {
            shackle = RingSub(pShm->grade_shackle[GradeSyncIndex],
                              pShm->sys_set.GradeArea[grade_index[i]].offset,
                              pShm->sys_set.Shackles);

            //I added the three elements to the if statement below because:
			//  The !resetgrade feature was not working when the grade of the bird
			//  came from the default gradezone. (GradeIndex == 0) This is because the software
			//  used gradezone == 0 for its reset grade indicator. If the bird was
			//  already from grade zone = 0, the system couldn't tell if it was an an empty
			//  shackle or if it came from gradezone 0.
			//
			//  This solution is probably not the best solution in terms of speed but, in terms
			//    of not wanting to cause side effects, it seems to work.
            if ( (BITSET(switch_in[0], grade_bit[i])) &&
                 (pShm->ShackleStatus[shackle].GradeIndex[0] == 0) &&  // added 3/16/2006 LATER-J
				  (
				  (pShm->sys_set.ResetGrading.ResetGrade) || //if reset grade is ON, proceed (don't even evaluate the rest)
				  ((pShm->ShackleStatus[shackle].dropped[0] == 1) && //was it dropped or empty
				  (pShm->ShackleStatus[shackle].dropped[1] == 1))) //same for scale two
				  )
            {
                pShm->ShackleStatus[shackle].GradeIndex[0] = (unsigned char) grade_index[i]; // added 3/16/2006
                pShm->ShackleStatus[shackle].GradeIndex[1] = (unsigned char) grade_index[i]; // DRW added 5/25/2014 this sets the grade on scale 2
                //RtPrintf("set grade\tidx\t%d shackle num %d\n",grade_index[i], shackle);
				//DebugTrace(_GRADE_, "Grade Assigned\t%C\tshackle\t%d\tsw_in[0]\t%X\n",
                //    pShm->sys_set.GradeArea[grade_index[i]].grade, shackle, switch_in[0]);
                if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _GRADE_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[MAINBUFID],"Grade Assigned\t%C\tshackle\t%d\tsw_in[0]\t%X\tsync%d\n",
                    pShm->sys_set.GradeArea[grade_index[i]].grade, shackle, switch_in[0], GradeSyncIndex);
                    strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID]);
                }
            }
        }
    }  // end - for-loop

	//shackle = RingSub(pShm->grade_shackle[GradeSyncIndex],
	//		pShm->sys_set.GradeArea[grade_index[MAXGRADES - 1]].offset - 2,
	//		pShm->sys_set.Shackles);

    // Time to reset the grade, unless ResetGrade is false
	if (pShm->sys_set.ResetGrading.ResetGrade)
	{
		if (pShm->sys_set.GradeArea[grade_index[MAXGRADES - 1]].GradeSyncUsed == GradeSyncIndex)
		{
			shackle = RingSub(pShm->grade_shackle[GradeSyncIndex],
				pShm->sys_set.GradeArea[grade_index[MAXGRADES - 1]].offset - 2,
				pShm->sys_set.Shackles);

			pShm->ShackleStatus[shackle].GradeIndex[0] = 0; // added 3/16/2006 LATER-J
			pShm->ShackleStatus[shackle].GradeIndex[1] = 0; // DRW added 5/25/2014  This resets the grade on scale 2
			//DebugTrace(_GRADE_, "Grade Reset\tshackle\t%d\n", shackle);
			if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _GRADE_ ) )
				{
				sprintf((char*) &tmp_trc_buf[MAINBUFID],"Grade Reset\tshackle\t%d\tsync%d\n",
					shackle, GradeSyncIndex);
				strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
				}
		}

	}  // if reset-grade enabled

}

//--------------------------------------------------------
// ZeroAverage
//--------------------------------------------------------

__int64 overhead::ZeroAverage(int scale)
{
    int j;
    __int64 tot = 0;
    int cnt = 0;
    int max_avg;

    // Can't do an average
    if (pShm->scl_set.ZeroNumber == 0)
        return WEIGH_SHACKLE(scale, pShm).weight[scale];

    if (pShm->scl_set.ZeroNumber <= MAXAVGCNT)
        max_avg = pShm->scl_set.ZeroNumber;
    else
        max_avg = MAXAVGCNT;

    for (j = 0; j < max_avg; j++)
    {
        tot += pShm->ZeroAverage[j][scale];
        cnt++;
    }

    // Increment place pointer.
    pShm->ZeroCounter[scale]++;

    if ( pShm->ZeroCounter[scale] >= max_avg )
         pShm->ZeroCounter[scale] = 0;

    // The average
    return (tot/cnt);
}

//--------------------------------------------------------
// AutoZero
//--------------------------------------------------------

void overhead::AutoZero(int s)
{
    __int64 average_zero;
    __int64 wt = WEIGH_SHACKLE(s, pShm).weight[s];

//----- Stuff average buffer the first time through, compute average
//      and assign to AutoBias.

    if (get_weight_init[s] == false)
    {
        for (int sample = 0; sample < MAXAVGCNT; sample++)
            pShm->ZeroAverage[sample][s] = wt;

        pShm->AutoBias[s] = average_zero = ZeroAverage(s);

        if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WEIGHT_ ) )
        {
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"Init\tABias\t%d\n", average_zero);
            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID]);
        }

        get_weight_init[s] = true;
    }

//----- Add another weight to the average buffer and calc new zero average.

    pShm->ZeroAverage[pShm->ZeroCounter[s]][s] = wt;
    average_zero                               = ZeroAverage(s);

//----- Check to see whether to use average (bit 1 is zero average used).

    if ( zero_bias_mode == USE_ZERO_BIAS_AVG )
        pShm->AutoBias[s] = (int) average_zero;

    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WEIGHT_ ) )
    {
        sprintf((char*) &tmp_trc_buf[MAINBUFID],"wt[%d][1]\t%d\tAvg\t%d\tLim\t%d\n",
                        s, wt,
                        (int) average_zero,
                        pShm->scl_set.AutoBiasLimit);
        strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID]);
    }

//----- Check to see whether to use limit checking ( bit 2 ).

    if ( zero_bias_mode == USE_ZERO_BIAS_LIM )
    {
        // Out of limits, something must have fallen on/off the scale
        if ((wt > (average_zero + pShm->scl_set.AutoBiasLimit)) ||
            (wt < (average_zero - pShm->scl_set.AutoBiasLimit)))
        {
             if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _WEIGHT_ ) )
             {
                 sprintf((char*) &tmp_trc_buf[MAINBUFID],"New\tABias\tfrom\t%d\tto\t%d\n",
                                (int) pShm->AutoBias[s], wt);
                 strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID]);
             }

            pShm->AutoBias[s] = wt;
        }
        else
            pShm->AutoBias[s] = average_zero;
    }

    return;
}

//--------------------------------------------------------
//  WriteDropRecords:
//--------------------------------------------------------

void overhead::WriteDropRecords(int cnt)
{
    char   fname[64];
    bool   write_Ok    = false;

//----- Save the records

    sprintf( fname, DREC_FILE_PATH, sav_drp_rec_file_info.cur_file);

    if (Write_File( OPEN_EXISTING, fname,
                    (BYTE*) &sav_rec_buf,
                    sizeof(app_drp_rec) * cnt) == ERROR_OCCURED)
    {
        if (Write_File( CREATE_NEW, fname,
                        (BYTE*) &sav_rec_buf,
                        sizeof(app_drp_rec) * cnt) == ERROR_OCCURED)
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        else
            write_Ok = true;
    }
    else
        write_Ok = true;

//----- Update the drop record recovery info.

    if (write_Ok)
    {
        sav_drp_rec_file_info.cur_cnt += cnt;

        // Have enough records for this file,
        // reset counts and start a new file.
        if (sav_drp_rec_file_info.cur_cnt >= MAXSAVERECS)
        {
           sav_drp_rec_file_info.cur_cnt = 0;

           if (++sav_drp_rec_file_info.cur_file >= MAXDSAVFILES)
                sav_drp_rec_file_info.cur_file = 0;

            // delete the file if it exists and start a new one
            sprintf( fname, DREC_FILE_PATH, sav_drp_rec_file_info.cur_file);
            DeleteFile(fname);
        }

//----- Save the drop record recovery info.

        if (Write_File(CREATE_ALWAYS, DREC_INFO_PATH,
                 (BYTE*) &sav_drp_rec_file_info,
                 sizeof(sav_drp_rec_file_info)) == ERROR_OCCURED)
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
    }
/*
    RtPrintf("SavDrec\tcurfile\t%d\tnxtsnd\t%d\tcurcnt\t%d\tnxtsqnum\t%d\n",
                          sav_drp_rec_file_info.cur_file,
                          sav_drp_rec_file_info.nxt2_send,
                          sav_drp_rec_file_info.cur_cnt,
                          sav_drp_rec_file_info.nxt_seqnum);
*/
}

//--------------------------------------------------------
//  SaveDropRecords: look for records to send to host.
//
// It examines HostDropRec for used slots, then adds a
// sequence number and appends the drop record to the file
// currently being used. Later when communications is restored,
// these records will be sent to the host.
//--------------------------------------------------------

void overhead::SaveDropRecords(bool save_all)
{
    int i, save_cnt = 0;
    int line_mult = this_lineid << 24;


#ifdef _DREC_TIMING_TEST_
    // drop record timing test started will not get a response
    drec_seq_done = 0;
#endif

    // don't bother with saving dribs and drabs unless shutting down.
    if (!save_all)
    {
        // Determine how many records have been added.
        // Don't bother if less than MINDSAVREC.
        for (i = 0; i < MAXDROPRECS; i++)
        {
            // search for used records
            if ( (pShm->HostDropRec[i].flags == PCRECUSED) ||
                 (drp_rec_resp_q[i].seq_num != 0) )
                save_cnt++;
        }

        if (save_cnt < MINDSAVREC)
            return;
    }

    // ok, must have enough to save
    save_cnt = 0;

//----- Save sent but not ack'd entries first (drp_rec_resp_q)

    for (i = 0; i < MAXDROPRECS; i++)
    {
        // search for used records
        if (drp_rec_resp_q[i].seq_num != 0)
        {
            // debug
            //if (drp_rec_resp_q[i].d_rec.last_in_batch[0] != 0)
            //{
            //    RtPrintf("SndDrpRec\tshkl\t%d\tdrop\t%d\tbatch\t%X\tlst\t%d\n",
            //                  drp_rec_resp_q[i].d_rec.shackle,
            //                  drp_rec_resp_q[i].d_rec.drop[0],
            //                  drp_rec_resp_q[i].d_rec.batch_number[0],
            //                  drp_rec_resp_q[i].d_rec.last_in_batch[0]);
            //}

            // copy send record into save buffer
            memcpy((void*) &sav_rec_buf[save_cnt],
                           &drp_rec_resp_q[i].seq_num,
                           sizeof(app_drp_rec));

            // clear record
            memset(&drp_rec_resp_q[i], 0, sizeof(drp_rec_sendQ));
            save_cnt++;
        }

    }

//----- Search for new records. (HostDropRec)

    for (i = 0; i < MAXDROPRECS; i++)
    {
        // search for used records
        if (pShm->HostDropRec[i].flags == PCRECUSED)
        {
            // start saving with the next sequence number
            sav_rec_buf[save_cnt].seq_num = sav_drp_rec_file_info.nxt_seqnum + line_mult;

            // copy drop record into save buffer
            memcpy((void*) &sav_rec_buf[save_cnt].d_rec,
                           &pShm->HostDropRec[i],
                           sizeof(THostDropRec));
            // clear the drop record
            memset((void*) &pShm->HostDropRec[i], 0, sizeof(THostDropRec));
            save_cnt++;

            // bump sequence number, highest nibble gets the line #
            if(++sav_drp_rec_file_info.nxt_seqnum >= 0x0FFFFFFF)
                sav_drp_rec_file_info.nxt_seqnum = 1;
        }

    }
    WriteDropRecords(save_cnt);
}

//--------------------------------------------------------
//  RequestToSendDropRecordBlock:
//--------------------------------------------------------

void overhead::RequestToSendDropRecordBlock()
{
    // Request Mutex for send
    if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    else
    {
        //DUMP_SAVED_DRECS (/* len */)
        SendHostMsg( BLK_DREC_REQ, NULL, NULL, NULL);

        if(!RtReleaseMutex(trc[GPBUFID].mutex))
            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    }
}

//--------------------------------------------------------
//  SendSavedDropRecords:
//--------------------------------------------------------

void overhead::SendSavedDropRecords()
{
    UINT   len = 0;
    char   fname[64];

//----- Read the records

    sprintf( fname, DREC_FILE_PATH, sav_drp_rec_file_info.nxt2_send);

    // correct length
    if (Read_File( OPEN_EXISTING, fname, (BYTE*) &sav_rec_buf,
                   &len, LEN_UNKNOWN) == ERROR_OCCURED)
    {
        RtPrintf("SndSvdDrpRec file %s does not exist\n", fname);
        DeleteFile(fname);
        UPDATE_FILE_INFO
    }
    else
    {
        // do a simple length check, should be zero
        if ( ((len % sizeof(app_drp_rec)) == 0) && (len != 0) )
        {
            RtPrintf("SndSvdDrpRec\tfile\t%s\n", fname, len);
            // only saving the 1st sequence num. When sending set recovery bit
            // in flags for host, only one sequence num will be returned for
            // MAXDROPRECS record chunk.
            sav_beg_seq_num = sav_rec_buf[0].seq_num;

            // Request Mutex for send
            if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {
                //DUMP_SAVED_DRECS (len)
                sendBlockRequested = true;
                SendHostMsg( DREC_MSG, DRECRECOVERY, (BYTE*) &sav_rec_buf, len);

                if(!RtReleaseMutex(trc[GPBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }
        }
        else
        {
            DeleteFile(fname);
            UPDATE_FILE_INFO
            RtPrintf("File length %d error file %s, line %d\n", len, _FILE_, __LINE__);
        }
    }
}

//--------------------------------------------------------
//  RemoveSavedDropRecords:
//--------------------------------------------------------

void overhead::RemoveSavedDropRecords()
{
    char   fname[64];

    sav_beg_seq_num = 0;

    //RtPrintf(  "RemSDrpRec0\tcur\t%d\tnxt\t%d\n",
    //            sav_drp_rec_file_info.cur_file,
    //            sav_drp_rec_file_info.nxt2_send);

//----- Delete the file which was sent. It will always be nxt2_send.

    sprintf( fname, DREC_FILE_PATH, sav_drp_rec_file_info.nxt2_send);

    if (DeleteFile(fname) == ERROR_OCCURED)
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);

    UPDATE_FILE_INFO

    //RtPrintf(  "RemSDrpRec1\tcur\t%d\tnxt\t%d\n",
    //            sav_drp_rec_file_info.cur_file,
    //            sav_drp_rec_file_info.nxt2_send);

}

//--------------------------------------------------------
//  SendDropRecords: look for records to send to host.
//
// It examines HostDropRec for used slots, builds a message
// to send to host in in drp_rec_send_buf and saves the
// record in drp_rec_resp_q with a sequence number. The host
// responds with sequence numbers of the drop records
// proccessed by the host. If the sequence number is not
// ack'd after retries, the record is saved to a file to
// send later.
//
//--------------------------------------------------------

void overhead::SendDropRecords()
{
    int   i,j;
    bool  begin_msg = false;
    int   send_cnt  = 0;
    int   save_cnt  = 0;
//    bool  sav_mutex = false;	RED - Removed because not referenced
    int line_mult = this_lineid << 24;

    // Search for entries to retry first
    for (i = 0; i < MAXDROPRECS; i++)
    {

//----- if hasn't been acknowledged, try to send it again

        if (drp_rec_resp_q[i].seq_num != 0)
        {
            // don't give up until 3 tries
            if (++drp_rec_resp_q[i].send_tries <= 3)
            {
                if(send_cnt < MAXDROPRECS)
                {
				    // This is where the record transmit actually occurs.

					// build retry message
                    drp_rec_send_buf[send_cnt].seq_num = drp_rec_resp_q[i].seq_num;
                    memcpy(&drp_rec_send_buf[send_cnt].d_rec, 
						   &drp_rec_resp_q[i].d_rec, 
						   sizeof(THostDropRec));

                    send_cnt++;

					// Enable this for checking if a re-transmit message is being sent.
					// RtPrintf("DrpRec\tseqnum =\t%x\t\tRetry Attempt  o o o o\n",drp_rec_resp_q[i].seq_num);

// Response timing check for drop records
#ifdef _DREC_TIMING_TEST_

                    // Ok to get a new sequence number to check host response time
                    if (drec_seq_done == 0)
                    {
                        RtGetClockTime(CLOCK_FASTEST,&drec_StartTime);
                        drec_seq_done = drp_rec_resp_q[i].seq_num;
                    }
#endif

// Response timing check for print label, drop records that contain last in batch
#ifdef _LREC_TIMING_TEST_

                    // Ok to check host response time on next batch
                    if ( (ldrec_seq_done == drp_rec_resp_q[i].seq_num) &&
                          drp_rec_resp_q[i].d_rec.last_in_batch[0] )
                    {
		                RtPrintf("ldrec\tretry\tseq\t%x\n", drp_rec_resp_q[i].seq_num);
                        RtGetClockTime(CLOCK_FASTEST,&ldrec_StartTime);
                        ldrec_seq_done = drp_rec_resp_q[i].seq_num;
                    }
#endif
                    //RtPrintf("seq*\t%x\tshkl\t%d\tweight[0]\t%d\tdrop[0]\t%d\n",
                    //          drp_rec_resp_q[i].seq_num,
                    //          drp_rec_resp_q[i].d_rec.shackle,
                    //          drp_rec_resp_q[i].d_rec.weight [0],
                    //          drp_rec_resp_q[i].d_rec.drop   [0]);

                    // this part appears to be for debug tracing only
                    if ( (!trc[GPBUFID].buffer_full)  && (TraceMask & _DROPS_ ) )
                    {
                        if(!begin_msg)
                        {
                            sprintf((char*) &tmp_trc_buf[GPBUFID],"%s", "SndDrpRecs\n");
                            strcat ((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID]);
                            begin_msg = true;
                        }

						// added grade indexing - March 24, 2006  -  Jim W.
						// added additional printf format - March 29, 2006 - Jim W.
                        sprintf((char*) &tmp_trc_buf[GPBUFID],"idx*\t%d\tseq\t%x\tshkl\t%d\tgrade[0]\t%1c\n\tdrpd[0]\t%d\tweight[0]\t%d\tdrop[0]\t%d\n\tgrade[1]\t%1c\tdrpd[1]\t%d\tweight[1]\t%d\tdrop[1]\t%d\n",
                                              i,
                                              drp_rec_resp_q[i].seq_num,
                                              drp_rec_resp_q[i].d_rec.shackle,
                                              drp_rec_resp_q[i].d_rec.grade  [0],
                                              drp_rec_resp_q[i].d_rec.dropped[0],
                                              drp_rec_resp_q[i].d_rec.weight [0],
                                              drp_rec_resp_q[i].d_rec.drop   [0],
                                              drp_rec_resp_q[i].d_rec.grade  [1],
                                              drp_rec_resp_q[i].d_rec.dropped[1],
                                              drp_rec_resp_q[i].d_rec.weight [1],
                                              drp_rec_resp_q[i].d_rec.drop   [1]);

                        strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID]);

                    }  // end - trace buffer NOT full - and - we are doing a trace on DROPS

                }  // end - send count < max-drop-recs

                else  i = MAXDROPRECS;
			//	{
			//	     i = MAXDROPRECS;	
            //       RtPrintf("DrpRec\tsend_cnt set to max-drop-recs\n");  // jjj 3/29/2006  remove later
			//	}

            }  // end - don't give up until 3 tries

//----- Retries expired, save them to a file for later

            else  // tried and failed 3 times
            {

#ifdef _DREC_TIMING_TEST_

                if (drec_seq_done == drp_rec_resp_q[i].seq_num)
                {
		            RtPrintf("drec\tnot\tack'd\tseq\t%x\n", drp_rec_resp_q[i].seq_num);
                    RtGetClockTime(CLOCK_FASTEST,&drec_EndTime);
		            RtPrintf("DREC\ttmo\ttime\t%u ms\n",
                            (UINT) ((drec_EndTime.QuadPart - drec_StartTime.QuadPart)/10000));
                    drec_seq_done = 0;
                }
#endif

#ifdef _LREC_TIMING_TEST_

                if (ldrec_seq_done == drp_rec_resp_q[i].seq_num)
                {
		            RtPrintf("ldrec\tnot\tack'd\tseq\t%x\n", drp_rec_resp_q[i].seq_num);
                    RtGetClockTime(CLOCK_FASTEST,&ldrec_EndTime);
		            RtPrintf("LDREC\ttmo\ttime\t%u ms\n",
                            (UINT) ((ldrec_EndTime.QuadPart - ldrec_StartTime.QuadPart)/10000));
                    ldrec_seq_done = 0;
                }
#endif

                // copy send record into save buffer
                memcpy((void*) &sav_rec_buf[save_cnt],
                               &drp_rec_resp_q[i].seq_num,
                               sizeof(app_drp_rec));
                save_cnt++;

                // wkr, add error
                RtPrintf("DrpRec\tseq\t%x\t!Ack'd\tsaving to file.\n",drp_rec_resp_q[i].seq_num);
                
                memset(&drp_rec_resp_q[i], 0, sizeof(drp_rec_sendQ));  // clear record

            }  // end - Retries expired, save them to a file for later

        }  // end - hasn't been acknowledged, try to send it again

    }  // end - search for entries to re-try

//----- Save records which were not acknowledged

    if (save_cnt) WriteDropRecords(save_cnt);

//----- At last, search for new entries

    for (i = 0; i < MAXDROPRECS; i++)
    {
        // search for used records
        if (pShm->HostDropRec[i].flags == PCRECUSED)
        {
            // look for a free slot in table
            for (j = 0; j < MAXDROPRECS; j++)
            {
                if (drp_rec_resp_q[j].seq_num == 0)
                {
                    if(send_cnt < MAXDROPRECS)
                    {
 				        // This is where the record transmit actually occurs.

                        // build message
                        drp_rec_send_buf[send_cnt].seq_num = sav_drp_rec_file_info.nxt_seqnum + line_mult;
                        // for verification
                        drp_rec_resp_q[j].seq_num          = drp_rec_send_buf[send_cnt].seq_num;
                        drp_rec_resp_q[j].send_tries       = 0;
                        // copy drop record into send buffer
                        memcpy((void*) &drp_rec_send_buf[send_cnt].d_rec,
                                       &pShm->HostDropRec[i],
                                       sizeof(THostDropRec));
                        // copy drop record into sent q
                        memcpy((void*) &drp_rec_resp_q[j].d_rec,
                                       &pShm->HostDropRec[i],
                                       sizeof(THostDropRec));
                        // clear the drop record
                        memset((void*) &pShm->HostDropRec[i], 0,
                                       sizeof(THostDropRec));
                        send_cnt++;

					    // Enable this for checking if an initial transmit message is being sent.
                        // RtPrintf("DrpRec\tseqnum =\t%x\t\tInitial Send Attempt  x x x x\n",drp_rec_resp_q[j].seq_num);

						if(++sav_drp_rec_file_info.nxt_seqnum >= 0x0FFFFFFF)
                            sav_drp_rec_file_info.nxt_seqnum   = 1;

                        //RtPrintf("seq\t%x\tshkl\t%d\tweight[0]\t%d\tdrop[0]\t%d\n",
                        //          drp_rec_resp_q[j].seq_num,
                        //          drp_rec_resp_q[j].d_rec.shackle,
                        //          drp_rec_resp_q[j].d_rec.weight [0],
                        //          drp_rec_resp_q[j].d_rec.drop   [0]);
#ifdef _DREC_TIMING_TEST_

                        // Ok to get a new sequence number to check
                        if (drec_seq_done == 0)
                        {
                            RtGetClockTime(CLOCK_FASTEST,&drec_StartTime);
                            drec_seq_done = drp_rec_resp_q[j].seq_num;
                        }
#endif

// Response timing check for print label, drop records that contain last in batch
#ifdef _LREC_TIMING_TEST_

                        // Ok to check host response time on next batch
                        if ( (ldrec_seq_done == 0) &&
                             (drp_rec_resp_q[j].d_rec.last_in_batch[0] != 0) )
                        {
                            RtGetClockTime(CLOCK_FASTEST,&ldrec_StartTime);
                            ldrec_seq_done = drp_rec_resp_q[j].seq_num;
                        }
#endif
                        // this part appears to be for debug tracing only
                        if (drp_rec_resp_q[j].d_rec.last_in_batch[0] != 0)
                        {
                            if( (!trc[GPBUFID].buffer_full) && (TraceMask & _LABELS_) )
                            {
                                sprintf((char*) &tmp_trc_buf[GPBUFID],"SndDrpRec\tshkl\t%d\tdrop\t%d\tbatch\t%u\tlst\t%d\n",
                                              drp_rec_resp_q[j].d_rec.shackle,
                                              drp_rec_resp_q[j].d_rec.drop[0],
                                              drp_rec_resp_q[j].d_rec.batch_number[0] & BATCH_MASK,
                                              drp_rec_resp_q[j].d_rec.last_in_batch[0]);
                                strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID]);
                            }
                        }

                        if( (!trc[GPBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
                        {
                            if(!begin_msg)
                            {
                                sprintf((char*) &tmp_trc_buf[GPBUFID],"%s", "SndDrpRecs\n");
                                strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID]);
                                begin_msg = true;
                            }

							// added grade indexing - March 24, 2006  -  Jim W.
						    // added additional printf format - March 29, 2006 - Jim W.
                            sprintf((char*) &tmp_trc_buf[GPBUFID],"idx\t%d\tseq\t%x\tshkl\t%d\tgrade[0]\t%1c\n\tdrpd[0]\t%d\tweight[0]\t%d\tdrop[0]\t%d\n\tgrade[1]\t%1c\tdrpd[1]\t%d\tweight[1]\t%d\tdrop[1]\t%d\n",
                                      j,
                                      drp_rec_resp_q[j].seq_num,
                                      drp_rec_resp_q[j].d_rec.shackle,
                                      drp_rec_resp_q[j].d_rec.grade[0],
                                      drp_rec_resp_q[j].d_rec.dropped[0],
                                      drp_rec_resp_q[j].d_rec.weight[0],
                                      drp_rec_resp_q[j].d_rec.drop[0],
                                      drp_rec_resp_q[j].d_rec.grade[1],
                                      drp_rec_resp_q[j].d_rec.dropped[1],
                                      drp_rec_resp_q[j].d_rec.weight[1],
                                      drp_rec_resp_q[j].d_rec.drop[1]);

                            strcat((char*) &trc_buf[GPBUFID],(char*) &tmp_trc_buf[GPBUFID]);

                            // use this print to size ttrc_buf above, if anything is added
                            // RtPrintf("Chk %d \n",strlen(ttrc_buf));
                        }
                        break;
                    }
                    else
                    {
                        i = j = MAXDROPRECS;
                    }

                }
            }
        }
    }

    if (send_cnt > 0)
    {
        // Request Mutex for send
        if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        else
        {
            //RtPrintf("Sending\tdrprec\t%d\n", send_cnt);
            if HOST_OK
                SendHostMsg( DREC_MSG, NULL, (BYTE*) &drp_rec_send_buf, sizeof(app_drp_rec) * send_cnt);

            if(!RtReleaseMutex(trc[GPBUFID].mutex))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }
    }
}

//--------------------------------------------------------
//  RemoveDropRecords: look for records to remove
//--------------------------------------------------------

void overhead::RemoveDropRecords(int cnt, UINT* seqnums)
{
    int i,idx = 0;
    bool found = false;

    //RtPrintf("RemDrpRec\tcnt\t%d\n",cnt);

    // debug trace
    if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
    {
        sprintf((char*) &tmp_trc_buf[EVTBUFID],"RemDrpRec\tcnt\t%d\n",cnt);
        strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID]);
    }

	int forever = 1;
    do
    {
        found = false;

        for (i = 0; i < MAXDROPRECS; i++)
        {
            if (seqnums[idx] > 0)
            {
                // a match
                if ( drp_rec_resp_q[i].seq_num == seqnums[idx] )
                {
                    // debug trace
                    if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[EVTBUFID],"idx\t%d\tseq#\t%x\n",
                                         i, drp_rec_resp_q[i].seq_num);
                        strcat((char*) &trc_buf[EVTBUFID], (char*) &tmp_trc_buf[EVTBUFID]);
                        // RtPrintf("Rem %d \n",strlen(ttrc_buf));
                    }

                    // just clear seq num
                    drp_rec_resp_q[i].seq_num  = 0;
                    found = true;
                    idx++;
                    i = MAXDROPRECS;
                }
            }
            else
            {
                // This was commented after we expanded HostDropRec for 
				// the grade-reset-between-scales feature  3-17-2006
				// It started always firing...?  Re-enable later?  Jim W.
				//  RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);  
                idx++;
            }
        }

        // oh well
        if (!found)
        {
            // This was commented after we expanded HostDropRec for 
			// the grade-reset-between-scales feature  3-17-2006
			// It started always firing...?  Re-enable later?  Jim W.
            // RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            idx++;
        }

        // done removing?
        if (idx >= cnt) break;

    } while (forever);

    // debug trace
    if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _DROPS_ ) )
    {
        strcat((char*) &trc_buf[EVTBUFID], "\n");
        //RtPrintf("Sending Remove dbgmsg size %d\n", strlen((char*) &trc_buf[EVTBUFID]) );
    }
}

//--------------------------------------------------------
//  AddDropRecord
//--------------------------------------------------------

void overhead::AddDropRecord(int shackle)
{

    TShackleStatus       *pshk = &pShm->ShackleStatus[shackle];
    static  THostDropRec *rec  = &pShm->HostDropRec[0];
    int     loop_cnt = 0;
    bool    sent     = false;
    time_t  long_time;
    struct tm *ptime;
//	char    tmpGrade;

    // bail out if BOTH scales recorded less-than-min bird-weight
    if ( (pshk->weight[0] < pShm->sys_set.MinBird) &&
         (pshk->weight[1] < pShm->sys_set.MinBird) )
        return;

    // send a record if EITHER scale recorded a valid bird-weight
    for (;;)
    {
        // Search for a place to put it
        if (!rec->flags)
        {

            // date/time stamp
            time( &long_time );
            ptime = localtime( &long_time );
            memcpy(&rec->time, ptime, sizeof(tm));

            rec->shackle          = shackle;

            if (pshk->weight[0] >= pShm->sys_set.MinBird)
			{
				rec->dropped[0]       = pshk->dropped[0];
				rec->weight[0]        = pshk->weight[0];
				rec->drop[0]          = pshk->drop[0];
				rec->batch_number[0]  = pshk->batch_number[0];
				rec->last_in_batch[0] = pshk->last_in_batch[0];
				//RtPrintf("shackle %d drop %d batch %d last %d\n",
				//        rec->shackle,rec->drop[0],rec->batch_number[0],rec->last_in_batch[0] & 1);
			}

            if (dual_scale) // && (pshk->weight[1] >= pShm->sys_set.MinBird))

            {
                rec->dropped[1]       = pshk->dropped[1];
                rec->weight[1]        = pshk->weight[1];
                rec->drop[1]          = pshk->drop[1];
                rec->batch_number[1]  = pshk->batch_number[1];
                rec->last_in_batch[1] = pshk->last_in_batch[1];
            }  // end - dual-scale


            if (pShm->sys_set.Grading)
            {
                if (pShm->WeighZero[0] && (grade_zeroed[0] & grade_zeroed[1]))
                {
                    if (pshk->weight[0] >= pShm->sys_set.MinBird)
					{
						rec->grade[0] = pShm->sys_set.GradeArea[pshk->GradeIndex[0]].grade;
					}

                    // added 2nd rec for dual-scale dual-grade case   3/16/2006 
					if (dual_scale)
					{
                       if (pshk->weight[1] >= pShm->sys_set.MinBird)
					   {
						   rec->grade[1] = pShm->sys_set.GradeArea[pshk->GradeIndex[1]].grade;
					   }
					}
					//DebugTrace(_GRADE_, "AddDropRecord\tshackle\t%d\tGrade\t%x\tWeight\t%d\n",
					//		shackle, rec->grade, rec->weight[0]);

                    // The grade may need to be preserved, if not dropped.
                    // GradeProcess will handle the rest.
                    // if Reset-Grade box is unchecked, this is the only code that
					// resets grade for dropped birds.
                    if (!dual_scale)
                    {
                        if (pshk->dropped[0])
						{
                            pshk->GradeIndex[0] = 0;// added 3/16/2006
						}
                    }
                    else
                    {
                        if (pshk->dropped[0] ||
                            pshk->dropped[1] )
                            pshk->GradeIndex[0] = 0;// added 3/16/2006

                    }

				}
                else
				{
                    rec->grade[0] = 'Z';
                    rec->grade[1] = 'Z';
				}
            }
            else
			{
                rec->grade[0] = ' ';
                rec->grade[1] = ' ';
			}

#ifndef _NO_INTERFACE_
            rec->flags = PCRECUSED;
#endif
            //RtPrintf("SndRec\tshkl\t%d\n", rec->shackle);
            sent = true;
        }

        rec++;

        if (rec >= &pShm->HostDropRec[MAXDROPRECS - 1])
            rec = &pShm->HostDropRec[0];

        if (sent) break;

        // Give up and print error
        if (loop_cnt++ >= MAXDROPRECS)
        {
            sprintf(app_err_buf,"Drop rec buf full %s line %d\n", _FILE_, __LINE__);
            GenError(warning, app_err_buf);
            break;
        }
    }
}

//--------------------------------------------------------
//  GenError
//--------------------------------------------------------

void overhead::GenError(int sev, char* txt)
{
    if ((sev = informational) != 0)
        RtPrintf("GenStatus:  %s",txt);  // change output from Error to Status for info-msgs.  Jim W.  3/20/2006
	else
        RtPrintf("GenError: %s",txt);

    send_error.sev      = sev;
    send_error.err_addr = txt;
    send_error.send     = true;

    if (sev == critical)
    {
        // give time for message to be sent
        Sleep(5000);
        RtPrintf("\n***********************************************\n");
        RtPrintf("*   Critical error overhead.rtss shutdown     *\n");
        RtPrintf("***********************************************\n");
        pShm->AppFlags = 0;
    }
}

//--------------------------------------------------------
//  SendError
//--------------------------------------------------------

void overhead::SendError()
{
    int len = strlen(send_error.err_addr);

    if (len >= MAXERRMBUFSIZE)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return;
    }
    // Request Mutex for send
    if(RtWaitForSingleObject(trc[GPBUFID].mutex, WAIT100MS) != WAIT_OBJECT_0)
        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    else
    {
        if HOST_OK
            SendHostMsg( ERROR_MSG, send_error.sev, (BYTE*) send_error.err_addr, strlen(send_error.err_addr));

        if(!RtReleaseMutex(trc[GPBUFID].mutex))
            RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    }

    send_error.sev      = 0;
    send_error.err_addr = NULL;
}

//--------------------------------------------------------
// MissedBirdCheck
// Adeed: Reset the schackle grade index to index GradeArea 4.
// GradeArea is controlled by Front End. SchackleStatus[shackle].GradeIndex holds the index
//of the GradeArea: GradeArea[MAXGRADES] has upto four values:
//--------------------------------------------------------

bool overhead::MissedBirdCheck(int curr_mb, int shackle, int mb_bit_state)
{
    bool bird_wt[2];
    bool drop[2];
    bool return_val = false;
//	int adeed_trc = 0;
	bool reset_flag = false;
    TShackleStatus* shk;

	//RtPrintf("Entered MissedBirdCheck State = %d\n",mb_bit_state);

    shk        = &pShm->ShackleStatus[shackle];
    bird_wt[0] = (shk->weight[0] > pShm->sys_set.MinBird);  // bird weight, scale-1
    bird_wt[1] = (shk->weight[1] > pShm->sys_set.MinBird);  // bird weight, scale-2

    // nothing to process
    if ((!bird_wt[0]) && (!bird_wt[1]))  // both scales show no weight for this shackle
	{
		shk->dropped[0] = 1;//for grading
		shk->dropped[1] = 1;//for grading

//DRW I dont think ResetS1Grade is needeed here because we are only resetting empty shackles only				
		// Adeed's added code - February, 2006
    	// if reset-grade-between-scale feature enabled     -and-  looking at 1st MB sensor
		if (((pShm->sys_set.ResetGrading.ResetScale1Grade > 0) && (curr_mb == 0)) || ((pShm->sys_set.ResetGrading.ResetS1Empty > 0) && (curr_mb == 0)))
		{	
			pShm->ShackleStatus[shackle].GradeIndex[1] = 4;   //pShm->sys_set.ResetGrading.ResetScale1Grade; // Reset Shackles here: Adeed
		//	RtPrintf(" No bird on shackle - Feature ON - Reset @ MB-1 - Grade: %d   Shkle: %d:\n",pShm->sys_set.GradeArea[shk->GradeIndex[1]].grade,shackle);
			reset_flag = true;

		}

		else	
        // end of Adeed's added code - February, 2006
		{
			// reset the grade - jim d courtney
			pShm->ShackleStatus[shackle].GradeIndex[0] = 0;  // pre-Adeed code; resets grade to default  --> safe
    	//	RtPrintf(" No bird on shackle - Feature OFF - Default @ MB-1 - Grade: %d   Shkle: %d:\n",pShm->sys_set.GradeArea[shk->GradeIndex[0]].grade,shackle);
		}
		return false;  // exit

	}  // end - both scales show no weight

    drop[0]   = (shk->drop[0] != 0);
    drop[1]   = (shk->drop[1] != 0);
   // adeed_trc++;
   // RtPrintf("Entered here: %d\n",adeed_trc);   //Adeed: Remove

	if(MB_ENABLED(0)) //RCB - Verify this
	{
		mb_bit_state = MB_NSET;
	}

    switch(mb_bit_state)
    {

//----- Missed bird bit set ---> There is a Missed Bird present in the shackle

        case MB_SET:

   		//	RtPrintf("Entered MB_SET: %d\n",adeed_trc);   //Adeed: Remove

	       if (!dual_scale)  // Single scale  ***************
           {
                if (bird_wt[0])  // bird showed weight on scale-1
                {
                    // One MB sensor -and- current MB for scale-1
					if (MB_ENABLED(1) && (curr_mb == 0))  
                    {
                        UNDROP_SCALE(0, pShm, shk)
                        MISSED(0, shk)
						//DebugTrace(_GRADE_,
						//	"MB_SET:dual_scale = %d, curr_mb = %d\tshackle\t%d\n",
						//	dual_scale, curr_mb, shackle);
                        return true;

                    }  // end - One MB sensor -and- current MB for scale-1

					// Note:  you can't have 2 MB sensors if you only have one scale...

                }  // end - bird showed weight on scale-1
				
		   }  // end - single-scale


            else  // Dual scale  ***************
            {
                if (bird_wt[0]) // bird showed weight on scale-1 
                {
                    // If one missed bird detect, send OK (return true).
                    if (MB_ENABLED(1)  && (curr_mb == 0))
                    {
						if (drop[0])			// Bird was counted for scale 1
						{
							UNDROP_SCALE(0, pShm, shk)		// Remove bird from scale 1 count

							// If bird not assigned a drop for scale 2
							//if (!(drop[1]))
							//{

								// Even if assigned for scale2, count as missed for
								// maintance reasons. This will skew the missed bird
								// totals but allows one to see that a scale 1 drop
								// is missing birds.
								MISSED(0, shk)			// Should be counted as a missed bird
								return_val = true;
							//}
						}
                    }  // end for single MB-sensor case


				// Still within the 'bird showed weight on scale-1' scenario:

                    // If two missed bird detects -and- now at first detect      
                    if (MB_ENABLED(2) && (curr_mb == 0))
                    {
                        //	UNDROP_SCALE1_SEND_GIB  // reassign at gib drop
						if (drop[0])
						{
							SubtractBird(shk->GradeIndex[0], shk->drop[0], shk->weight[0]);
							shk->drop[1] = pShm->sys_set.GibDrop;
							if DBG_DROP(shk->drop[0] - 1, pShm)
							{
								if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
								{
									sprintf((char*) &tmp_trc_buf[MAINBUFID],"MBUndrp\tmb\t%d\tdrop\t%d\tshkl\t%d\twt\t%d\n",
												  curr_mb, shk->drop[0], shackle, shk->weight[0]);
									strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
								}
							}
						}
   			if (pShm->sys_set.ResetGrading.ResetS1Empty > 0)// Reset-grade-scale-1 feature is enabled
				{
				    pShm->ShackleStatus[shackle].GradeIndex[1] =  pShm->ShackleStatus[shackle].GradeIndex[0];  
//				    RtPrintf("MB NOT set - Feature ON ----- Reset @ MB-1 - Grade: %d   Shkle: %d:\n",pShm->sys_set.GradeArea[shk->GradeIndex[1]].grade,shackle);
				}
						MISSED(0, shk)     // flag it missed for scale-1
						// there was no "return_val = true" here...?
                    }

				// Still within the 'bird showed weight on scale-1' scenario:

                    // // If two missed bird detects -and- now at second detect.
                    if (MB_ENABLED(2) && (curr_mb == 1))
						return_val = true;		

                } // end - bird weighed on scale-1


				
                if (bird_wt[1])  // if the bird has been weighed on scale-2  
                {
				    // RtPrintf("Dual-scale, Bird weighed on scale-2\n");
                    // If one or two missed bird detects, send on second detect.
                    if ( (MB_ENABLED(1)  && (curr_mb == 0)) ||
                         (MB_ENABLED(2) && (curr_mb == 1)) )
                    {
					    // RtPrintf("Dual-scale, Bird weighed on scale-2, curr_mb = %d\n",curr_mb);

						// If bird is assigned a drop for scale 2
						if (drop[1])
						{
							UNDROP_SCALE(1, pShm, shk)		// Bird was counted for scale2, remove it.
							MISSED(1, shk)	// Since the bird should have been dropped, count it as missed 
						    // RtPrintf("Dual-scale, Bird weighed on scale-2, drop for scale-2 = undrop-sc-1 and miss-1\n");
						}
						else  // Not assigned a drop for scale-2
						{
							if (!(drop[0]))  // If also not assigned a drop for scale-1 
							{
								MISSED(1, shk)	// Add to the unasssigned count for scale-2
							}
						}
                        return_val = true;

                    }  // One MB -and- @ MB for scale-1 --or-- Two MBs -and- @ MB for scale-2

                } // end - bird weighed on scale-2

                return return_val;

            }  // end - dual-scale

            break;

//----- Missed bird bit not set ---> There is NOT a Missed Bird here, shackle is empty

        case MB_NSET:

			//DebugTrace(_GRADE_,"MB_NSET:dual_scale = %d, curr_mb = %d\tshackle\t%d\n",
			//	dual_scale, curr_mb, shackle);

 			// adeed_trc++;
			// RtPrintf("Entered MB_NSET: %d\n",adeed_trc);   //Adeed: Remove
// DRW Added the or for Reset1Empty for resetting the grade of the empty shackles only at the first missed bird detect
			// Adeed's added code - February, 2006
			if (curr_mb == 0)  // sensor = MB-1
			{	
    			if ((pShm->sys_set.ResetGrading.ResetScale1Grade > 0) || (pShm->sys_set.ResetGrading.ResetS1Empty > 0))// Reset-grade-scale-1 feature is enabled
				{
				    pShm->ShackleStatus[shackle].GradeIndex[1] = 4;  // Reset Shackles here: Adeed
//				    RtPrintf("MB NOT set - Feature ON ----- Reset @ MB-1 - Grade: %d   Shkle: %d:\n",pShm->sys_set.GradeArea[shk->GradeIndex[1]].grade,shackle);
				}
			}  // end of Adeed's added code - February, 2006


			// Single scale with NO Missed Bird ***********************************

			if ((!dual_scale) && (curr_mb == 0))
			{
				// Need to keep track of unassigned birds, could be a hole in the schedule
				if (!drop[0] && bird_wt[0] )
				{
					MISSED(0, shk)
				}

				// Must assume it was dropped
				shk->dropped[0] = 1;
				return true;
            }  // single-scale

            // Dual scale with NO Missed Bird ***********************************

            else
            {

               // If no bird detects, flagged as dropped @ curr_mb = 0.
                if ( MB_ENABLED(0) )
                {
					// scale-1 drop assigned -and- bird weighed @ scale-1 -and- now @ MB for scale-1
                    if (drop[0] && bird_wt[0] && (curr_mb == 0) )
                    {
                        shk->dropped[0] = 1;
                        return_val = true;
                    }

					// scale-2 drop assigned -and- bird weighed @ scale-2 -and- now @ MB for scale-1
                    if (drop[1] && bird_wt[1] && (curr_mb == 0) )
                    {
                        shk->dropped[1] = 1;
                        return_val = true;
                    }

					// NOT scale-2 drop assigned -and- bird weighed @ scale-2 -and- now @ MB for scale-1
                    if (!drop[1] && bird_wt[1] && (curr_mb == 0) ) //check if drop is assigned to drop 0 jdc
                    {
                        MISSED(0, shk)
                        return_val = true;
                    }

					if(return_val == false)
					{
						// reset the grade jdc
						//DRW Added for 
						if (pShm->sys_set.ResetGrading.ResetS1Empty < 1)
						{
    					pShm->ShackleStatus[shackle].GradeIndex[0] = 0;  // added 3/16/2006
						}
						DebugTrace(_GRADE_,
							"MissedBirdCheck(NO_MB_ENABLES):Setting GradeIndex = 0 shackle\t%d\n",
							shackle);
					}

                    return return_val;
                }  // end - no missed-bird enabled


				// At least one Missed Bird detect enabled
                if (drop[0] && bird_wt[0])
                {
                    // If one missed bird detect,  send OK on first mb.
                    // If two missed bird detects, send on second detect.
                    if ( (MB_ENABLED(1)  && (curr_mb == 0)) ||
                         (MB_ENABLED(2) && (curr_mb == 1)) )
                    {
                        shk->dropped[0] = 1;
                        return_val = true;
                    }

				 //	adeed_trc++;
			     //   RtPrintf("Entered MB_NSET: %d %d \n",adeed_trc,curr_mb);   //Adeed: Remove
			
                } // end - at least one MB enabled


                // If two missed bird detects, send on second detect.
                if (drop[1] && bird_wt[1])
                {
                    // If one missed bird detect,  send OK on first mb.
                    // If two missed bird detects, send on second detect.
                    if ( (MB_ENABLED(1) && (curr_mb == 0)) ||
                         (MB_ENABLED(2) && (curr_mb == 1)) )
                    {
                        shk->dropped[1] = 1;
                        return_val = true;
                    }

					// if only one detect, undrop drop assigned for first scale
                    // because it is assumed it was supposed to drop after 1st scale
					// if ( MB_ENABLED(1) && (curr_mb == 0) )
                    if ( MB_ENABLED(1) && (curr_mb == 0) && drop[0] ) // RCB - Only if assigned a drop
                    {
                        UNDROP_SCALE(0, pShm, shk)
                        MISSED(0, shk)
                    }
                } // end - two MB detecs, send on 2nd


				// No drop assignment
				if (!drop[0] && !drop[1] && bird_wt[1] &&		
					((MB_ENABLED(1) && (curr_mb == 0)) ||
					(MB_ENABLED(2) && (curr_mb == 1))) )
				{
					MISSED(1, shk)
					return_val = true;
				}  // end - No drop assignment

				// Incorrect 'reset-the-grade' commented out here on March 10, 2006.

                return return_val;

            }  // dual-scale

            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
           break;

        }  // end - case stmt

	DebugTrace(_GRADE_,"Fall thru MissedBirdCheck:\tshackle\t%d\n", shackle);
	return false;
}

//--------------------------------------------------------
// ProcessSyncs:
//
// This is the main sync counter and error handling loop
// This looks at the inputs from each OPTO module in order and takes
// approiate action when each respective bit is made. It triggers the
// weighing, drop assignment, grade process, drop process, and  sends
// a record of each bird to the front end Delphi application.
//
//--------------------------------------------------------

void overhead::ProcessSyncs()
{
    int    shk, i, j, k, lbl, loop_cnt, byte;
    int    temp;
    const  UINT missedBirdBits[2] = {MB_BIT1,MB_BIT2};
//	int	MyLineId = pShm->sys_set.IsysLineSettings.this_line_id;	RED - Removed because not referenced
//	int line = 0;	RED - Removed because not referenced
	// Count actual configured syncs from SyncSettings.
	// Layout: indices 0-1 = scale syncs, indices 2-7 = drop syncs.
	// NumDrops is sorting BINS (up to 32), NOT sync sensors (up to 6).
	// Max: 2 scale syncs + 6 drop syncs = 8.
	int NumSyncs = pShm->scl_set.NumScales; // 1 or 2 scale syncs
	for (int ds = 2; ds < 8; ds++) { // drop sync indices 2-7
		if (pShm->sys_set.SyncSettings[ds].first > 0)
			NumSyncs = ds + 1; // extend to cover this sync
	}
	if (NumSyncs > 8) NumSyncs = 8; // hard max: 2 scale + 6 drop

	// If grading is disabled, grade_zeroed must be true so zero processing works.
	// GradeSyncs() doesn't run when grading is off, so grade_zeroed would never
	// get set otherwise. Init at line 1706 unconditionally sets it false.
	if (!pShm->sys_set.Grading) {
		grade_zeroed[0] = true;
		grade_zeroed[1] = true;
	}

	static bool	sync_zero_triggered[MAXSYNCS]; //GLC Added for Carolina turkeys 3/8/05

	// Check for BOAZ mode and set number of syncs accordingly
	if (app->pShm->sys_set.MiscFeatures.EnableBoazMode)
	{
		NumSyncs = BOAZSYNCS;
	}

	for (i = 0; i < NumSyncs; i++)
    {
         byte = i / 8;
         pSyncSet  = &pShm->sys_set.SyncSettings[i];
         pSyncStat = &pShm->SyncStatus[i];

		//GLC for Carolina Turkeys, check that the zero sensor is triggered before the sync
		 if (app->pShm->sys_set.MiscFeatures.RequireZeroBeforeSync)
		 {
			 if (BITSET(sync_zero[byte], i)) //GLC added 3/8/05
			 {
				 sync_zero_triggered[i] = true; //GLC added 3/8/05
			 }
		 }
		 else
		 {
			 sync_zero_triggered[i] = true; //GLC added 3/8/05
		 }

		 //if (BITSET(sync_in[byte], i) && sync_armed[i])
		 if (BITSET(sync_in[byte], i) && sync_armed[i] && sync_zero_triggered[i]/*GLC added 3/8/05*/)
         {
             if (sync_debounce[i] > 0) sync_debounce[i]--;

             // Slight Delay
             if (sync_debounce[i] == 0)
             {

                 sync_armed[i] = false;
				 sync_zero_triggered[i] = false; //GLC added 3/8/05

                 if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _SYNCS_ ) )
                 {
                     sprintf((char*) &tmp_trc_buf[MAINBUFID],"Sync\t%d\tOK\tShk\t%d\n",
                             i, pSyncStat->shackleno);
                     strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
                 }

				//----- Zero flag detected -----

 				 // Only zero the sync if the grade syncs have already zeroed. This is to prevent misgrading
				 // in the time period between a scale sync zero and grade sync zero (if the zero flag passes
				 // the scale before it passes the grade sync).
                 if (BITSET(sync_zero[byte], i) && (grade_zeroed[0] & grade_zeroed[1])) //RCB test
                 {

					 // GLC 2/14/05
					 // If this sync has already zeroed before, show late or early zero flag if applicable
					if ((pSyncStat->shackleno < true_shackle_count[i]) &&
						(true_shackle_count[i] > pShm->sys_set.Shackles) &&
						pSyncStat->zeroed)
                    {
                        sprintf(app_err_buf,"Late Zero Flag Detected. %s late count %ld\n",
							sync_desc[i], (long) (true_shackle_count[i] - pShm->sys_set.Shackles));
                        GenError(warning, app_err_buf);

                    }
                    else if ((pSyncStat->shackleno < pShm->sys_set.Shackles) && pSyncStat->zeroed)
                    {
                        sprintf(app_err_buf,"Early Zero Flag Detected. %s shkl %d expected shkl %d\n",
							sync_desc[i], pSyncStat->shackleno, pShm->sys_set.Shackles);
                        GenError(warning, app_err_buf);
                    }

					//GLC added 2/15/05
					// Show informational message the first time the sync is zeroed
					if (!pSyncStat->zeroed)
					{
						sprintf(app_err_buf,"Initial Zero Flag Detected. %s\n", sync_desc[i]);
						GenError(informational, app_err_buf);
					}

                    // Reset sync status
                    trolly_counters[i]                  = 0;
                    pSyncStat->shackleno                = 1;
                    true_shackle_count[i]				= 1; //GLC added 2/14/05
                    pSyncStat->zeroed                   = true;
					shm_updates[78-1]                   = true;  // shmID 78 = SyncStatus, notify host of zeroed state
					pShm->sys_stat.dbg_sync[ZERO_ON][i] = 0;
					pShm->sys_stat.dbg_sync[SYNC_ON][i]++; //added jc

                    // Miscellaneous
                    if (i == SCALE1SYNCBIT)
                    {
                         // clear diagnostic counters
                         for (int sync_ctr = 0; sync_ctr <= MAXSYNCS; sync_ctr++)
                            pShm->sys_stat.dbg_sync[SYNC_ON][sync_ctr] = 0;
                    }

                    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _ZEROS_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[MAINBUFID],"Zero\t%d\tOK\n", i);
                        strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
                    }
                 }

//----- No Zero flag detected

                else
                {
                    // Increment shackle numbers skip or no skip
                    if (pShm->sys_set.SkipTrollies > 0)
                    {
                        //RtPrintf("**ProcessSync: SkipTrollies > 0\n");
	   				   trolly_counters[i]++;

                       if (trolly_counters[i] > pShm->sys_set.SkipTrollies)
                       {
						   //RtPrintf("**ProcessSync: trolly_counters[i] > pShm->sys_set.SkipTrollies\n");
							pSyncStat->shackleno++;
		                    true_shackle_count[i]++; //GLC added 2/14/05
							pShm->sys_stat.dbg_sync[SYNC_ON][i]++;
							pShm->sys_stat.dbg_sync[ZERO_ON][i]++;
                           trolly_counters[i] = 0;
                       }
                    }
                    else
					{
						/*
						if (i==0)
						{
							RtPrintf("**ProcessSync: SkipTrollies = 0 Shkl %d TrueShkl %d\n",
								pSyncStat->shackleno, true_shackle_count[i]);
						}
						*/
						pSyncStat->shackleno++;
		                true_shackle_count[i]++; //GLC added 2/14/05
						pShm->sys_stat.dbg_sync[SYNC_ON][i]++;
						pShm->sys_stat.dbg_sync[ZERO_ON][i]++;
					}

                    /*if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _SYNCS_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[MAINBUFID],"Sync\t%d\tOK\tShk\t%d\n",
                                i, pSyncStat->shackleno);
                        strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
                    }*/
                }

//----- Too many shackles, flag error

                // GLC Only report late zeroes on syncs if grade sync has already zeroed or grading is disabled
				if ((pSyncStat->shackleno > pShm->sys_set.Shackles) && grade_zeroed[0] && grade_zeroed[1])
                {
                     {
                        sprintf(app_err_buf,"Zero Flag NOT Detected. %s shackle %d expected %d\n",
							sync_desc[i], pSyncStat->shackleno, pShm->sys_set.Shackles); //GLC added 2/14/05
                        GenError(warning, app_err_buf);
                     }
					//RtPrintf("**ProcessSync: Trolly counter reset to 0\n");
                    trolly_counters[i]   = 0;
                    pSyncStat->shackleno = 1;
                }

//----- Reset status at scale 1 and trigger weighing

                if ( SYNC_OK && (i == SCALE1SYNCBIT))
                {
                    // clear drop & weight at scale 1
                    pShm->ShackleStatus[pSyncStat->shackleno].drop[0]   = 0;
                    //pShm->ShackleStatus[pSyncStat->shackleno].drop[1]   = 0;
                    pShm->ShackleStatus[pSyncStat->shackleno].weight[0] = 0;
                    //pShm->ShackleStatus[pSyncStat->shackleno].weight[1] = 0;
                    pShm->ShackleStatus[pSyncStat->shackleno].batch_number[0]  = 0;
                    //pShm->ShackleStatus[pSyncStat->shackleno].batch_number[1]  = 0;
                    pShm->ShackleStatus[pSyncStat->shackleno].last_in_batch[0] = false;
                    //pShm->ShackleStatus[pSyncStat->shackleno].last_in_batch[1] = false;

                    if (!pShm->WeighZero[0]) {
                        pShm->WeighZero[0] = true;
                        shm_updates[SCLWGHZ-1] = true;  // notify host only on transition
                    }
                    pShm->WeighShackle[0] = pSyncStat->shackleno;
                    weigh_state[0]        = WeighActive;
					DebugTrace(_SYNCS_, "SC1-Weighing triggered. Shackle = %d, trolley = %d\n",pSyncStat->shackleno, trolly_counters[i]);
                }

                // trigger weighing for scale 2
                if ( SYNC_OK && dual_scale && (i == SCALE2SYNCBIT) )
                {
                    pShm->ShackleStatus[pSyncStat->shackleno].drop[1]   = 0;
					pShm->ShackleStatus[pSyncStat->shackleno].weight[1] = 0;
					pShm->ShackleStatus[pSyncStat->shackleno].batch_number[1]  = 0;
					pShm->ShackleStatus[pSyncStat->shackleno].last_in_batch[1] = false;

                    if (!pShm->WeighZero[1]) {
                        pShm->WeighZero[1] = true;
                        shm_updates[SCLWGHZ-1] = true;  // notify host only on transition
                    }
                    pShm->WeighShackle[1] = pSyncStat->shackleno;
                    weigh_state[1]        = WeighActive;
					DebugTrace(_SYNCS_, "SC2-Weighing triggered. Shackle = %d, trolley = %d\n",pSyncStat->shackleno, trolly_counters[i]);
                }

//----- Weight average/capture section

                // recalculate shackle to shackle ticks on every other shackle
                if ( i == SCALE1SYNCBIT )
                {
                    isys_shksec_sec_cnt++; // not part of avg/capt

                    // The offsets can vary from install to install. Skip all of these
                    // to be out of the way of any automatic resets which may occur.
                    if ( (pSyncStat->shackleno > 5) &&
                         (pSyncStat->shackleno < (pShm->sys_set.Shackles - 5)) )
                    {
 						calc_shk2shk_ticks = ( (pSyncStat->shackleno & 1) ? true : false );

                        if (!calc_shk2shk_ticks) //calc_shk2shk_ticks enabled every other shackle
                        {
                            if ( app->cur_shk2shk_ticks > 0 )
                            {
                                //jdc ... added the following SkipTrollies blurb to change the timing waveform
                                app->shk2shk_ticks = app->cur_shk2shk_ticks / (pShm->sys_set.SkipTrollies>1?2:1);//cur_shk2shk_ticks is incremented in App_Timer_Main
                                app->cur_shk2shk_ticks = 0;
                            }
                        }
                    }
                    else
                    {
                        calc_shk2shk_ticks = 0;
                        app->cur_shk2shk_ticks = 0;
                    }
                }

				//added previousShackle1 to counter the effect of having skip trollys
				//something other then zero.
                // capture scale 1
                if (( i == SCALE1SYNCBIT) && (app->previousShackle1 != app->pSyncStat->shackleno))
                {
                    app->previousShackle1 = app->pSyncStat->shackleno;
					if ( (capt_wt.scale == 1) &&
                         (capt_wt.mode  == prod_capt) )
                    {
                        capt_wt.capture = true;
                        capture_period  = app->shk2shk_ticks - 3;
                    }

                    // Clear wt average info and set trigger counter.
                    // Make counter a few counts less to ensure the
                    // average finishes before the next shackle.
                    memset(&w_avg[i], 0, sizeof(t_wt_avg));
                    w_avg[i].avg_trigger_cntr = app->shk2shk_ticks - 3;

                    //RtPrintf("shkl %d prevShackle %d\n",
                    //          app->pSyncStat->shackleno, app->previousShackle1);
                }


				//added previousShackle2 to counter the effect of having skip trollys
				//something other then zero.
                // capture scale 2
                if ((( i == SCALE2SYNCBIT) && (app->previousShackle2 != app->pSyncStat->shackleno)) &&
                      dual_scale )
                {
                    app->previousShackle2 = app->pSyncStat->shackleno;
					if ( (capt_wt.scale == 2) &&
                          (capt_wt.mode  == prod_capt) )
                    {
                        capt_wt.capture = true;
                        capture_period  = app->shk2shk_ticks - 3;
                    }

                    // Clear wt average info and set trigger counter.
                    // Make counter a few counts less to ensure the
                    // average finishes before the next shackle.
                    memset(&w_avg[i], 0, sizeof(t_wt_avg));
                    w_avg[i].avg_trigger_cntr = app->shk2shk_ticks - 3;

                    //RtPrintf("cap_prd %d shkl %d\n",
                    //          w_avg[i].avg_trigger_cntr,
                    //          pSyncStat->shackleno);
                    //RtPrintf("shkl %d prevShackle %d\n",
                    //          app->pSyncStat->shackleno, app->previousShackle2);
                }


//----- Drop section

                if ( SYNC_OK                  &&
                    (dual_scale               ||
                    (!dual_scale              &&
                    (i != SCALE2SYNCBIT)))    &&
                    (pShm->OpMode == ModeRun) )
                {
                    //If this sync supports drops, see if the shackle has a drop assigned

                    if ((pSyncSet->first > 0) && (pSyncSet->last > 0))
                    {
                        for (j = pSyncSet->first; j <= pSyncSet->last; j++)
                        {
                            // Get the Drop location based on this shackle
                            shk = RingSub(pSyncStat->shackleno,
                                          pShm->sys_set.DropSettings[j-1].Offset,
                                          pShm->sys_set.Shackles);

//----- Test fire drop

                            if ((shk == 1) && (pShm->dbg_set.TestFireDrop > 0) )
                            {
                                SetOutput(j,true);

                                if DBG_DROP(j-1, pShm)
                                {
                                    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                                    {
                                        sprintf((char*) &tmp_trc_buf[MAINBUFID],"*Test\tFire\tdrop\t%d\tshkl\t%d\n",j, shk);
                                        strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
                                    }
                                }
                            }

                            if SHACKLE_OK(shk)
                            {
                                // look for drops at both scales
                                loop_cnt = (dual_scale ? 2 : 1);

                                for (k = 0; k < loop_cnt; k++)
                                {
                                    // Was this shackle assigned to this drop?
                                    if (j == pShm->ShackleStatus[shk].drop[k])
                                    {
										// Drop it
										SetOutput(j,true);

										switch(pShm->sys_set.DropSettings[j-1].DropMode)
										{
										case DM_REMOTERESET:
											//	Set flag to say that final bird of batch has been dropped
											if (pShm->ShackleStatus[shk].last_in_batch[k])
											{
												last_in_batch_dropped[j-1] = true;
												//	If this is InterSystem drop, notify other lines this has happened
												if (ISYS_ENABLE && ISYS_DROP(j-1, pShm))
												{
													for (int i = 0; i < MAXIISYSLINES; i++)
													{
														if (((i+1) != this_lineid) &&
															(pShm->sys_set.IsysLineSettings.active[i]) &&
															(pShm->sys_set.IsysDropSettings[j-1].active[i]))
														{
															int line = pShm->sys_set.IsysLineSettings.line_ids[i];
															int drop = pShm->sys_set.IsysDropSettings[j-1].drop[i];
															SendLineMsg(LAST_BIRD_CLEARED, line , NULL, (BYTE*) &drop, sizeof(drop));
														}
													}
												}
											}
											break;
										case DM_REASSIGN:
											int idx = pShm->sys_set.DropSettings[j-1].SwitchLEDIndex - 1;
											Second_3724_DropDisabled[idx] = true;
											Second_3724_Shackle[idx] = shk;
											Second_3724_Scale[idx] = k;
											Second_3724_DropCounter[idx] = -((int) pShm->sys_set.DropOn);
											break;
										}

//----- Send prelabel message to host by setting pre_label_step to 1.

										if (pShm->ShackleStatus[shk].last_in_batch[k])
										{
											int line_no;

											line_no = pShm->ShackleStatus[shk].batch_number[k] >> LINE_SHIFT;

											if (line_no == this_lineid)
											{
												for (lbl = 0; lbl < MAXBCHLABELS; lbl++)
												{
													if (pShm->ShackleStatus[shk].batch_number[k] == batch_label.info[lbl].seq_num)
													{
														if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _LABELS_) )
														{
															sprintf((char*) &tmp_trc_buf[MAINBUFID],"PreLbl\tstep1\tbch#\t%d\tindx\t%d\n",
																  batch_label.info[lbl].seq_num & 0xFFFFF, lbl);
															strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
														}

														batch_label.info[lbl].pre_label_step = 1;
														lbl = MAXBCHLABELS;
													}
												}
											}
											else
											{
												for (int pb = 0; pb < MAXBCHLABELS; pb++)
												{
													if (slave_batch[pb] == 0)
													{
														slave_batch[pb] = pShm->ShackleStatus[shk].batch_number[k];
														pb = MAXBCHLABELS;
													}
												}
											}

										}

										if DBG_DROP(j-1, pShm)
										{
											if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
											{
												sprintf((char*) &tmp_trc_buf[MAINBUFID],"*Drop It\tdrop\t%d\tshkl\t%d\n",j, shk);
												strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
											}	//	End if ( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
										}	//	End if DBG_DROP(j-1, pShm)
									}	//	End if (j == pShm->ShackleStatus[shk].drop[k])
                                }	//	End for (k = 0; k < loop_cnt; k++)
                            }	//	End if SHACKLE_OK(shk)
                            else
                                // this would probably be due to a bad offset
                                // a bad shk# will result in an exception
                                RtPrintf("Error file %s, line %d drp %d shk %d\n",
                                          _FILE_, __LINE__, j, shk);

                        }	//	End for (j = pSyncSet->first; j <= pSyncSet->last; j++)
                    }	//	End if ((pSyncSet->first > 0) && (pSyncSet->last > 0))

//----- Missed bird eye(s), check for missed birds

                    for (k = 0; k < MAXSCALES; k++)
                    {
                        if ( i == pShm->sys_set.MBSync[k] + 1)
                        {
                           shk = RingSub(pSyncStat->shackleno, pShm->sys_set.MBOffset[k], pShm->sys_set.Shackles);
                           temp = ~RtReadPortUchar(PCM_3724_1_PORT_C0);
						   //show GradeIndex before next call jdc
//RtPrintf("GradeIndex = %d; shackle = %d\n",pShm->ShackleStatus[pSyncStat->shackleno].GradeIndex[0], pSyncStat->shackleno);
//RtPrintf("GradeIndex = %d; shackle = %d\n",pShm->ShackleStatus[shk].GradeIndex[0], shk);
//RtPrintf("GradeIndex = %d; shackle = %d\n\n",pShm->ShackleStatus[17].GradeIndex[0], 17);


                           if (MissedBirdCheck(k, shk, (BITSET(temp, missedBirdBits[k]) & 1) ) )
						   {
							  //DebugTrace(_GRADE_,
							//	  "ProcessSyncs:GradeIndex = %d; shackle = %d\n",
							//	  pShm->ShackleStatus[shk].GradeIndex[0], shk);
                              AddDropRecord(shk);
							  //pShm->ShackleStatus[shk].GradeIndex[?] = 0; //RCB-JWC 04/26/04

						   }

                        } // if (pShm->sys_set.MBSync == i)
                    } // for loop
                } // if (dual_scale... i != SCALE2SYNCBIT)
           }  // if sync_debounce[i] = 0
         } // if sync_in
         else if (!BITSET(sync_in[byte], i))
         {
             sync_armed   [i] = true;
             sync_debounce[i] = pShm->sys_set.SyncOn;
         }
		 else if (BITSET(sync_in[byte], i) && sync_armed[i] && (!sync_zero_triggered[i]))/*GLC added 3/9/05*/
		 {
			if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _SYNCS_ ) )
			{
				sprintf((char*) &tmp_trc_buf[MAINBUFID],"Sync\t%d\tno zero\tShk\t%d\n",
						i, pSyncStat->shackleno);
				strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] );
			}
		 }
    } // for i  = 0 to MAXSYNCS
}

//--------------------------------------------------------
//  DebugThread
//--------------------------------------------------------

void __stdcall overhead::DebugThread(PVOID unused)
{
	unused;		// RED - To eliminate unreferenced formal parameter warning
    int i, k, id_ndx, dbg_size;
    char*    pDbg_char;
    int*     pDbg_int   = 0;
    __int64* pDbg_real  = 0;
    int      dbg_id     = 0;   // id from Delphi
    int      dbg_idx    = 0;   // for arrays
//    int      dbg_loop   = 0;

    // Delay to not mess up start messages
	Sleep (5000);

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X Debugger\n",hDebug);
#endif

    SetUnhandledExceptionFilter(appDebugUnhandledExceptionFilter);

    // wait for lookup table to become valid
    for (;;)
    {
        if (app->shm_valid)
		{
            break;
		}
        Sleep (1000);
    }

    for (;;)
    {

        // Update the debug inputs. The syncs were turned into counters
        for (i = 0; i < MAXINPUTBYTS; i++)
        {
            app->pShm->sys_stat.dbg_zero[i]   = app->sync_zero[i];
            app->pShm->sys_stat.dbg_switch[i] = app->switch_in[i];
        }

        // Test Fire Outputs
        if (app->pShm->dbg_set.dbg_output)
        {
            app->SetOutput(app->pShm->dbg_set.dbg_output,true);
            app->pShm->dbg_set.dbg_output = 0;
        }

        // Stop debugging if someone left it on.
#if 0
        if ( (app->pShm->dbg_set.dbg_trace         != 0) ||
             (app->pShm->dbg_set.debug_id.id_count != 0) )
        {
            if (++dbg_loop >= 60)
            {
                app->pShm->dbg_set.debug_id.id_count = 0;
                app->pShm->dbg_set.dbg_trace         = 0;
                dbg_loop = 0;
            }
        }
        else dbg_loop = 0;
#endif

//-----  Main section

        if ( (app->pShm->dbg_set.debug_id.id_count > 0) &&
             (app->pShm->dbg_set.debug_id.id_count <= MAXDBGIDS) )
        {

            for (i = 1; i <= MAXIDS; i++)
            {

                for (id_ndx = 0; id_ndx < app->pShm->dbg_set.debug_id.id_count; id_ndx++ )
                {
                    // Found one
                    if (app->pShm->dbg_set.debug_id.ids[id_ndx].id == i)
                    {
                        // See if we already printed a description
                        if ((id_ndx == 0) || ((id_ndx > 0) &&
                            (dbg_id != app->pShm->dbg_set.debug_id.ids[id_ndx].id - 1)))
                        {
                            //RtPrintf("dbg_id %d dbg_idx %d\n", dbg_id, dbg_idx);
                            dbg_size  = app->shm_tbl[i-1].num_members;
                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"\nid\t%d\t--%s--\tSize\t%d\n",
                                     i, &app->shm_tbl[i-1].desc,app->shm_tbl[i-1].struct_len);
                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                        }

                        dbg_id  = app->pShm->dbg_set.debug_id.ids[id_ndx].id - 1;
                        dbg_idx = app->pShm->dbg_set.debug_id.ids[id_ndx].index;

                        if (dbg_idx >= app->shm_tbl[dbg_id].num_members)
                        {
                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"\tDebug bad index %d\n", dbg_idx);
                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                        }
                        else
                        {
                            switch (app->shm_tbl[dbg_id].data_type)
                            {
                                case _DBOOL:
                                case _char:

                                    pDbg_char = (char*) app->shm_tbl[dbg_id].data;
                                    pDbg_char += dbg_idx;

                                    if (isalnum(*pDbg_char))
                                    {
                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t%1c\n", *pDbg_char );
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    }
                                    else
                                    {
                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t%1x\n", *pDbg_char);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    }

                                    break;

                                case _uint:

                                    pDbg_int = (int*) app->shm_tbl[dbg_id].data;
                                    pDbg_int += dbg_idx;
                                    sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t%u\n", *pDbg_int);
                                    strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _int:

                                    pDbg_int = (int*) app->shm_tbl[dbg_id].data;
                                    pDbg_int += dbg_idx;
                                    sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t%d\n", *pDbg_int);
                                    strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case ___int64:

                                    pDbg_real = (__int64*) app->shm_tbl[dbg_id].data;
                                    pDbg_real += dbg_idx;
                                    sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t%d\n", (int) *pDbg_real);
                                    strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TShackleTares:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tGrdIdx\t%d\ttare[0]\t%d\ttare[1]\t%d\n",
                                                dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[0],
                                                dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[1],
                                                app->pShm->ShackleTares[dbg_idx].tare[0],
                                                app->pShm->ShackleTares[dbg_idx].tare[1]);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TSyncSettings:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tfirst\t%d\tlast\t%d\n", dbg_idx,
                                                      app->pShm->sys_set.SyncSettings[dbg_idx].first,
                                                      app->pShm->sys_set.SyncSettings[dbg_idx].last);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TScheduleSettings:

                                        //sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tMinWt\t%u\tMaxWt\t%u\tNxtEnt\t%u\tGrade\t%4.4s\tExgrd\t%4.4s\n", dbg_idx,
										sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tMinWt\t%d\tMaxWt\t%d\tNxtEnt\t%d\n", dbg_idx,
                                             (int)app->pShm->Schedule[dbg_idx].MinWeight,
                                             (int)app->pShm->Schedule[dbg_idx].MaxWeight,
                                             (int)app->pShm->Schedule[dbg_idx].NextEntry);
                                             //(char*)&app->pShm->Schedule[dbg_idx].Grade,
                                             //(char*)&app->pShm->Schedule[dbg_idx].Extgrade);

                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"ExMnWt\t%d\tExMxWt\t%d\tEx2MnWt\t%d\tEx2MxWt\t%d\tShrkg\t%u\n",
                                             (int)app->pShm->Schedule[dbg_idx].ExtMinWeight,
                                             (int)app->pShm->Schedule[dbg_idx].ExtMaxWeight,
                                             (int)app->pShm->Schedule[dbg_idx].Ext2MinWeight,
                                             (int)app->pShm->Schedule[dbg_idx].Ext2MaxWeight,
                                             app->pShm->Schedule[dbg_idx].Shrinkage);

                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
//#endif
                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"BchCnt\t%u\tBchWtSp\t%d\tBPMSp\t%u\tDistMd\t%u\tBPMmode\t%u\tLpOrd\t%d\n",
                                             app->pShm->Schedule[dbg_idx].BchCntSp,
                                             (int) app->pShm->Schedule[dbg_idx].BchWtSp,
                                             app->pShm->Schedule[dbg_idx].BPMSetpoint,
                                             app->pShm->Schedule[dbg_idx].DistMode,
                                             app->pShm->Schedule[dbg_idx].BpmMode,
                                             app->pShm->Schedule[dbg_idx].LoopOrder);

                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);


                                    break;

                                case _TOrder:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\t%d\n", dbg_idx, app->pShm->sys_set.Order[dbg_idx]);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TDropSettings:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index %d\tAct %1x\tSync %u\tOffst %d\tExtgrd %1x\tBchSld %u\tBchWtSld %d\tBPMSSld %u\n", dbg_idx,
                                        app->pShm->sys_set.DropSettings[dbg_idx].Active,
                                        app->pShm->sys_set.DropSettings[dbg_idx].Sync,
                                        app->pShm->sys_set.DropSettings[dbg_idx].Offset,
                                        app->pShm->sys_set.DropSettings[dbg_idx].Extgrade,
                                        app->pShm->sys_set.DropSettings[dbg_idx].IsysBchSlowdown,
                                        app->pShm->sys_set.DropSettings[dbg_idx].IsysBchWtSlowdown,
                                        app->pShm->sys_set.DropSettings[dbg_idx].IsysBPMSlowdown);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TGradeSettings:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tgrade\t%1c\toffset\t%d\tgrdsync\t%d\n", dbg_idx,
                                           app->pShm->sys_set.GradeArea[dbg_idx].grade,
                                            app->pShm->sys_set.GradeArea[dbg_idx].offset,
											app->pShm->sys_set.GradeArea[dbg_idx].GradeSyncUsed);

                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TIsysDropSettings:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\n", dbg_idx);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k < MAXIISYSLINES; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"active[%d]\t%d\tdrop\t%d\n", k,
                                                app->pShm->sys_set.IsysDropSettings[dbg_idx].active[k] & 0x1,
                                                app->pShm->sys_set.IsysDropSettings[dbg_idx].drop[k]);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }
                                    break;

                                case _TDebugRec:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"DbgRec\tid_count\t%u\n",
                                            app->pShm->dbg_set.debug_id.id_count);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k < MAXDBGIDS; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"dbgid[%d]\t%u\tdbgindex\t%u\n", k,
                                                    app->pShm->dbg_set.debug_id.ids[k].id,
                                                    app->pShm->dbg_set.debug_id.ids[k].index);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }
                                    break;

                                case _TIsysLineSettings:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"\tiena\t%d\timst\t%d\tlocid\t%d",
                                                 app->pShm->sys_set.IsysLineSettings.isys_enable,
                                                 app->pShm->sys_set.IsysLineSettings.isys_master,
                                                 app->this_lineid);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        {
                                            bool path_valid;

                                            for (k = 0; k <= MAXIISYSLINES; k++)
                                            {
                                                // just a simple test for valid path. The path starts with \\.
                                                if ( (app->pShm->sys_set.IsysLineSettings.pipe_path[k][0] == 0x5c) &&
                                                     (app->pShm->sys_set.IsysLineSettings.pipe_path[k][1] == 0x5c) )
                                                     path_valid = true;
                                                else
                                                     path_valid = false;

                                                if (k < MAXIISYSLINES)
                                                {
                                                    sprintf((char*) &tmp_trc_buf[DBGBUFID],"\nRemId[%d]\t%u\tactive\t%1.1x\tpath\t%s\n",k,
                                                            app->pShm->sys_set.IsysLineSettings.line_ids[k],
                                                            app->pShm->sys_set.IsysLineSettings.active[k] & 0x1,
                                                            (path_valid ? app->pShm->sys_set.IsysLineSettings.pipe_path[k] : "No Path") );
                                                    strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                                }
                                                else
                                                {
                                                    sprintf((char*) &tmp_trc_buf[DBGBUFID],"\n\t\t\t\thost\tpath\t%s\n",
                                                            (path_valid ? app->pShm->sys_set.IsysLineSettings.pipe_path[k] : "No Path") );
                                                    strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                                }
                                            }
                                        }

                                    break;

                                case _TDropStats:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tBWeight\t%u\tBCount\t%u\n", dbg_idx,
                                             (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].BWeight,
                                             (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].BCount);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k < MAXGRADES; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t\tGrd\t%c\tGrdWt\t%u\tGrdCnt\t%u\n",
                                                       app->pShm->sys_set.GradeArea[k].grade,
                                                 (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].GrdWeight[k],
                                                 (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].GrdCount[k]);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }

                                    break;

                                case _TDropStats1:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tBWeight\t%u\tBCount\t%u\n", dbg_idx,
                                             (int) app->pShm->sys_stat.DropInfo[dbg_idx].BWeight,
                                             (int) app->pShm->sys_stat.DropInfo[dbg_idx].BCount);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k <  MAXGRADES; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t\tGrd\t%c\tGrdWt\t%u\tGrdCnt\t%u\n",
                                                           app->pShm->sys_set.GradeArea[k].grade,
                                                     (int) app->pShm->sys_stat.DropInfo[dbg_idx].GrdWeight[k],
                                                     (int) app->pShm->sys_stat.DropInfo[dbg_idx].GrdCount[k]);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }
                                        break;

                                case _THostDropRec:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tflags\t%x\tshkl\t%d\tgrade\t%1.1x\tdrpd[0]\t%d\tweight\t%d\tdrop\t%d\n", dbg_idx,
                                            app->pShm->HostDropRec[dbg_idx].flags,
                                            app->pShm->HostDropRec[dbg_idx].shackle,
                                            app->pShm->HostDropRec[dbg_idx].grade[0],
                                            app->pShm->HostDropRec[dbg_idx].dropped[0],
                                            app->pShm->HostDropRec[dbg_idx].weight[0],
                                            app->pShm->HostDropRec[dbg_idx].drop[0]);

                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"\t\t\t\t\t\t\t\tgrade\t%1.1x\tdrpd[1]\t%d\tweight\t%d\tdrop\t%d\n",
                                            app->pShm->HostDropRec[dbg_idx].grade[1],
                                            app->pShm->HostDropRec[dbg_idx].dropped[1],
                                            app->pShm->HostDropRec[dbg_idx].weight[1],
                                            app->pShm->HostDropRec[dbg_idx].drop[1]);

                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TIsysDropStatus:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\t\n", dbg_idx);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k < MAXIISYSLINES; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"flags[%d]\t%1.1x\tPPMCnt\t%d\tPPMPrev\t%d\tBCnt\t%u\tBWt\t%u\n", k,
                                                    app->pShm->IsysDropStatus[dbg_idx].flags[k],
                                                    app->pShm->IsysDropStatus[dbg_idx].PPMCount[k],
                                                    app->pShm->IsysDropStatus[dbg_idx].PPMPrevious[k],
                                                    (unsigned int) app->pShm->IsysDropStatus[dbg_idx].BCount[k],
                                                    (unsigned int) app->pShm->IsysDropStatus[dbg_idx].BWeight[k]);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }
                                     break;

                                case _TScheduleStatus:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\t\n", dbg_idx);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"BaActCt\t%u\tBaActWt\t%u\tPPMCnt\t%u\tSpare\t%u\tBPMPrv\t%u\t\n",
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].BchActCnt,
                                             (int) app->pShm->sys_stat.ScheduleStatus[dbg_idx].BchActWt,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].PPMCount,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].spare,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].PPMPrevious);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"TklCnt\t%u\tBPM1Qok\t%u\tBPM2Qok\t%u\tBPM3Qok\t%u\tBPM4Qok\t%u\n",
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPMTrickleCount,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_1stQ_ok,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_2ndQ_ok,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_3rdQ_ok,
                                             app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_4thQ_ok);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TSyncStatus:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tshkl\t%u\tzero'd\t%1x\n", dbg_idx,
                                            app->pShm->SyncStatus[dbg_idx].shackleno,
                                            app->pShm->SyncStatus[dbg_idx].zeroed);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TDropStatus:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tBatch\t%u\tSusp\t%1x\tBCnt\t%u\tBWt\t%u\n", dbg_idx,
                                            app->pShm->sys_stat.DropStatus[dbg_idx].Batched,
                                            app->pShm->sys_stat.DropStatus[dbg_idx].Suspended,
                                            (int) app->pShm->sys_stat.DropStatus[dbg_idx].BCount,
                                            (int) app->pShm->sys_stat.DropStatus[dbg_idx].BWeight);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TDisplayShackle:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"grdidx[0]\t%u\tdrop[0]\t%d\tWt[0]\t%u\ngrdidx[1]\t%u\tdrop[1]\t%d\tWt[1]\t%u\n",
                                            app->pShm->sys_stat.DispShackle.GradeIndex[0],
                                            app->pShm->sys_stat.DispShackle.drop[0],
                                            app->pShm->sys_stat.DispShackle.weight[0],
                                            app->pShm->sys_stat.DispShackle.GradeIndex[1],
                                            app->pShm->sys_stat.DispShackle.drop[1],
                                            app->pShm->sys_stat.DispShackle.weight[1]);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;

                                case _TShackleStatus:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"index\t%d\tGrdIdx0\t%d\tGrdIdx1\t%d\n",
                                            dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[0], app->pShm->ShackleStatus[dbg_idx].GradeIndex[1]);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k < MAXSCALES; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"drop\t%d\tweight\t%d\ttare\t%d\n",
                                                          app->pShm->ShackleStatus[dbg_idx].drop[k],
                                                          app->pShm->ShackleStatus[dbg_idx].weight[k],
                                                          app->pShm->ShackleTares[dbg_idx].tare[k]);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }
                                    break;

                                case _TIsysLineStatus:

                                        sprintf((char*) &tmp_trc_buf[DBGBUFID],"pksent\t%d\tpkrec\t%d\tthrds\t%X\n",
                                                  app->pShm->IsysLineStatus.pps_sent,
                                                  app->pShm->IsysLineStatus.pps_recvd,
                                                  app->pShm->IsysLineStatus.app_threads);
                                        strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);

                                        for (k = 0; k < MAXIISYSLINES; k++)
                                        {
                                            sprintf((char*) &tmp_trc_buf[DBGBUFID],"flags[%d]\t%d\tconnected\t%1.1X\n", k,
                                                          app->pShm->IsysLineStatus.flags[k],
                                                          app->pShm->IsysLineStatus.connected[k]);
                                            strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                        }
                                    break;

                                case _DigLCSet:
							        RtPrintf("\nindex\t%d\tmmode\t%d\tASF\t%d\n"
								        "FMD\t%d\tICR\t%d\tLDW\t%d\n"
								        "LWT\t%d\tNOV\t%d\tRSN\t%d\n"
								        "MTD\t%d\tLIC0\t%d\tLIC1\t%d\n"
								        "LIC2\t%d\tLIC3\t%d\tZTR\t%d\n"
								        "ZSE\t%d\tTRC1\t%d\tTRC2\t%d\n"
                                        "TRC3\t%d\tTRC4\t%d\tTRC5\t%d\n\n",
								        dbg_idx,
								        app->pShm->scl_set.DigLCSet[dbg_idx].measure_mode,
								        app->pShm->scl_set.DigLCSet[dbg_idx].ASF,
								        app->pShm->scl_set.DigLCSet[dbg_idx].FMD,
								        app->pShm->scl_set.DigLCSet[dbg_idx].ICR,
								        app->pShm->scl_set.DigLCSet[dbg_idx].LDW,
								        app->pShm->scl_set.DigLCSet[dbg_idx].LWT,
								        app->pShm->scl_set.DigLCSet[dbg_idx].NOV,
								        app->pShm->scl_set.DigLCSet[dbg_idx].RSN,
								        app->pShm->scl_set.DigLCSet[dbg_idx].MTD,
								        app->pShm->scl_set.DigLCSet[dbg_idx].LIC0,
								        app->pShm->scl_set.DigLCSet[dbg_idx].LIC1,
								        app->pShm->scl_set.DigLCSet[dbg_idx].LIC2,
								        app->pShm->scl_set.DigLCSet[dbg_idx].LIC3,
								        app->pShm->scl_set.DigLCSet[dbg_idx].ZTR,
								        app->pShm->scl_set.DigLCSet[dbg_idx].ZSE,
								        app->pShm->scl_set.DigLCSet[dbg_idx].TRC1,
								        app->pShm->scl_set.DigLCSet[dbg_idx].TRC2,
								        app->pShm->scl_set.DigLCSet[dbg_idx].TRC3,
								        app->pShm->scl_set.DigLCSet[dbg_idx].TRC4,
								        app->pShm->scl_set.DigLCSet[dbg_idx].TRC5);


                                        break;
                                case _lc_sample_adj:
							        RtPrintf("\nindex\t%d\tsample_start\t%d\tsample_end\t%d\n",
								        dbg_idx,
								        app->pShm->scl_set.LC_Sample_Adj[dbg_idx].start,
								        app->pShm->scl_set.LC_Sample_Adj[dbg_idx].end);
                                    break;

								case _TMiscFeatures:
									sprintf((char*) &tmp_trc_buf[DBGBUFID],
										"EnableGradSync2\t0x%x\tEnableBoazMode\t0x%x\tRequireZeroBeforeSync\t0x%x\tspare\t0x%x\n",
										app->pShm->sys_set.MiscFeatures.EnableGradeSync2,
										app->pShm->sys_set.MiscFeatures.EnableBoazMode,
										app->pShm->sys_set.MiscFeatures.RequireZeroBeforeSync,
										app->pShm->sys_set.MiscFeatures.spare);
									strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
									break;
								case _TResetGradeSettings:
									sprintf((char*) &tmp_trc_buf[DBGBUFID],
										"ResetGrade\t%d\tResetScale1Grade\t%d\tResetS1Empty\t%d\n",
										app->pShm->sys_set.ResetGrading.ResetGrade,
										app->pShm->sys_set.ResetGrading.ResetScale1Grade,
//DRW Modified from spare to ResetS1Empty for resetting grade on empty shackles only 06/03/2015
										app->pShm->sys_set.ResetGrading.ResetS1Empty);
									strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
									break;

                                default:
                                    sprintf((char*) &tmp_trc_buf[DBGBUFID]," Not functional!\n");
                                    strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
                                    break;
                            }
                        }
                    }
                }   // for (id_ndx = 0; id_ndx
            }       // for (i = 1; i <= MAXIDS; i++)
        }           // if (app->pShm->dbg_set.debug_id.id_count)
        else
        {
            if ( (app->pShm->dbg_set.debug_id.id_count > MAXDBGIDS) ||
                 (app->pShm->dbg_set.debug_id.id_count < 0) )
            {
                sprintf((char*) &tmp_trc_buf[DBGBUFID],"Id count out of range %d (must be 0 to %d)\n",
					app->pShm->dbg_set.debug_id.id_count, MAXDBGIDS);
                strcat((char*) &trc_buf[DBGBUFID],(char*) &tmp_trc_buf[DBGBUFID]);
            }
        }

        // ok to send debug message
        if ( trc[DBGBUFID].send_OK )
        {
            // Request Mutex for send
            if(RtWaitForSingleObject(trc[DBGBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {
// Send debug to self
#ifdef _REDIRECT_DEBUG_
				app->SendLineMsg( DEBUG_MSG, app->this_lineid, NULL,
                              (BYTE*) &trc_buf[DBGBUFID],
                              trc[DBGBUFID].len);
#endif

#ifdef _LOCAL_DEBUG_
				if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
				{
#ifdef	_TELNET_UDP_ENABLED_
					UdpTraceNBuf((char*) &trc_buf[DBGBUFID], trc[DBGBUFID].len);
#else
					PrintNBuf((char*) &trc_buf[DBGBUFID], trc[DBGBUFID].len);
#endif
				}
				else
				{
					PrintNBuf((char*) &trc_buf[DBGBUFID], trc[DBGBUFID].len);
				}
#endif

#ifdef	_HOST_DEBUG_
				if HOST_OK
				{
					app->SendHostMsg( DEBUG_MSG, NULL,
                                  (BYTE*) &trc_buf[DBGBUFID],
                                  trc[DBGBUFID].len);
				}
#endif
                if(!RtReleaseMutex(trc[DBGBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }

            // clear old message.
            memset(&trc_buf[DBGBUFID], 0, sizeof(trcbuf));
            sprintf( (char*) &trc_buf[DBGBUFID],"L%d:%s\n",
                     app->this_lineid,
                     &trc_buf_desc[DBGBUFID]);
            trc[DBGBUFID].send_OK = false;
        }

        Sleep (3000);
    }   // for (;;)

}

//--------------------------------------------------------
//  InitShmTbl
//--------------------------------------------------------

void overhead::InitShmTbl()
{

// If id is a #define, it is referenced elsewhere in the program. Be
// sure to update the #define is numbers shift (shared memory change).

shm_info tbl[ALL_SHM_IDS] = {
	1,			_uint,					sizeof(app->pShm->sys_set.Shackles),			1,				(void*) &app->pShm->sys_set.Shackles,			"Shackles",				SYS_IN_GROUP,	
	2,			_uint,					sizeof(app->pShm->sys_set.TareTimes),			1,				(void*) &app->pShm->sys_set.TareTimes,			"TareTimes",			SYS_IN_GROUP,	
	3,			_uint,					sizeof(app->pShm->sys_set.GibMin),				1,				(void*) &app->pShm->sys_set.GibMin,				"GibMin",				SYS_IN_GROUP,	
	4,			_uint,					sizeof(app->pShm->sys_set.MinBird),				1,				(void*) &app->pShm->sys_set.MinBird,			"MinBird",				SYS_IN_GROUP,	
	5,			_uint,					sizeof(app->pShm->sys_set.LastDrop),			1,				(void*) &app->pShm->sys_set.LastDrop,			"LastDrop",				SYS_IN_GROUP,	
	6,			_uint,					sizeof(app->pShm->sys_set.NumDrops),			1,				(void*) &app->pShm->sys_set.NumDrops,			"NumDrops",				SYS_IN_GROUP,	
	7,			_uint,					sizeof(app->pShm->sys_set.GibDrop),				1,				(void*) &app->pShm->sys_set.GibDrop,			"GibDrop",				SYS_IN_GROUP,	
	8,			_uint,					sizeof(app->pShm->sys_set.DropOn),				1,				(void*) &app->pShm->sys_set.DropOn,				"DropOn",				SYS_IN_GROUP,
	9,			_uint,					sizeof(app->pShm->sys_set.SyncOn),				1,				(void*) &app->pShm->sys_set.SyncOn,				"SyncOn",				SYS_IN_GROUP,
	10,			_uint,					sizeof(app->pShm->sys_set.SkipTrollies),		1,				(void*) &app->pShm->sys_set.SkipTrollies,		"SkipTrollies",			SYS_IN_GROUP,
	11,			_uint,					sizeof(app->pShm->sys_set.MissedBirdEnable),	MAXSCALES,		(void*) &app->pShm->sys_set.MissedBirdEnable,	"MissedBirdEnable",		SYS_IN_GROUP,
	12,			_uint,					sizeof(app->pShm->sys_set.MBSync),				MAXSCALES,		(void*) &app->pShm->sys_set.MBSync,				"MBSync",				SYS_IN_GROUP,
	13,			_int,					sizeof(app->pShm->sys_set.MBOffset),			MAXSCALES,		(void*) &app->pShm->sys_set.MBOffset,			"MBOffset",				SYS_IN_GROUP,
	14,			_TSyncSettings,			sizeof(app->pShm->sys_set.SyncSettings),		MAXSYNCS,		(void*) &app->pShm->sys_set.SyncSettings,		"SyncSettings",			SYS_IN_GROUP,
	15,			_TOrder,				sizeof(app->pShm->sys_set.Order),				MAXDROPS,		(void*) &app->pShm->sys_set.Order,				"Order",				SYS_IN_GROUP,
	DROP_SET,	_TDropSettings,			sizeof(app->pShm->sys_set.DropSettings),		MAXDROPS,		(void*) &app->pShm->sys_set.DropSettings,		"DropSettings",			SYS_IN_GROUP,
	17,			_DBOOL,					sizeof(app->pShm->sys_set.Grading),				1,				(void*) &app->pShm->sys_set.Grading,			"Grading",				SYS_IN_GROUP,
	18,			_TResetGradeSettings,	sizeof(app->pShm->sys_set.ResetGrading),		1,				(void*) &app->pShm->sys_set.ResetGrading,		"ResetGrade",			SYS_IN_GROUP,
	19,			_TGradeSettings,		sizeof(app->pShm->sys_set.GradeArea),			MAXGRADES,		(void*) &app->pShm->sys_set.GradeArea,			"GradeArea",			SYS_IN_GROUP,
	20,			_TIsysDropSettings,		sizeof(app->pShm->sys_set.IsysDropSettings),	MAXDROPS,		(void*) &app->pShm->sys_set.IsysDropSettings,	"IsysDropSettings",		SYS_IN_GROUP,
	LSID,		_TIsysLineSettings,		sizeof(app->pShm->sys_set.IsysLineSettings),	1,				(void*) &app->pShm->sys_set.IsysLineSettings,	"IsysLineSettings",		SYS_IN_GROUP,
	22,			_TMiscFeatures,			sizeof(app->pShm->sys_set.MiscFeatures),		1,				(void*) &app->pShm->sys_set.MiscFeatures,		"MiscFeatures",			SYS_IN_GROUP,
	23,			_int,					sizeof(app->pShm->sys_set.OverRideDelay),		1,				(void*) &app->pShm->sys_set.OverRideDelay,		"OverRideDelay",		SYS_IN_GROUP,
	24,			_int,					sizeof(app->pShm->sys_set.ReassignTimerMax),	1,				(void*) &app->pShm->sys_set.ReassignTimerMax,	"ReassignTimerMax",		SYS_IN_GROUP,
	
	25,			_uint,					sizeof(app->pShm->scl_set.NumScales),			1,				(void*) &app->pShm->scl_set.NumScales,			"NumScales",			SCL_IN_GROUP,
	26,			_uint,					sizeof(app->pShm->scl_set.ZeroNumber),			1,				(void*) &app->pShm->scl_set.ZeroNumber,			"ZeroNumber",			SCL_IN_GROUP,
	27,			_int,					sizeof(app->pShm->scl_set.AutoBiasLimit),		1,				(void*) &app->pShm->scl_set.AutoBiasLimit,		"AutoBiasLimit",		SCL_IN_GROUP,
	28,			_uint,					sizeof(app->pShm->scl_set.NumAdcReads),			MAXSCALES,		(void*) &app->pShm->scl_set.NumAdcReads,		"NumAdcReads",			SCL_IN_GROUP,
	29,			_uint,					sizeof(app->pShm->scl_set.AdcMode),				MAXSCALES,		(void*) &app->pShm->scl_set.AdcMode,			"AdcMode",				SCL_IN_GROUP,
	30,			_int,					sizeof(app->pShm->scl_set.OffsetAdc),			MAXSCALES,		(void*) &app->pShm->scl_set.OffsetAdc,			"OffsetAdc",			SCL_IN_GROUP,
	31,			___int64,				sizeof(app->pShm->scl_set.Spare),				MAXSCALES,		(void*) &app->pShm->scl_set.Spare,				"Spare",				SCL_IN_GROUP,
	32,			___int64,				sizeof(app->pShm->scl_set.SpanBias),			MAXSCALES,		(void*) &app->pShm->scl_set.SpanBias,			"SpanBias",				SCL_IN_GROUP,
	33,			_int,					sizeof(app->pShm->scl_set.LoadCellType),		1,				(void*) &app->pShm->scl_set.LoadCellType,		"LoadCellType",			SCL_IN_GROUP,
	34,			_DigLCSet,				sizeof(app->pShm->scl_set.DigLCSet),			MAXLOADCELLS,	(void*) &app->pShm->scl_set.DigLCSet,			"DigLCSet",				SCL_IN_GROUP,
	35,			_lc_sample_adj,			sizeof(app->pShm->scl_set.LC_Sample_Adj),		MAXSCALES,		(void*) &app->pShm->scl_set.LC_Sample_Adj,		"LC_Sample_Adj",		SCL_IN_GROUP,
	36,			_int,					sizeof(app->pShm->scl_set.Pad01ScaleIn),		1,				(void*) &app->pShm->scl_set.Pad01ScaleIn,		"Pad01ScaleIn",			SCL_IN_GROUP,
	37,			_int,					sizeof(app->pShm->scl_set.Pad02ScaleIn),		1,				(void*) &app->pShm->scl_set.Pad02ScaleIn,		"Pad02ScaleIn",			SCL_IN_GROUP,
	38,			_int,					sizeof(app->pShm->scl_set.Pad03ScaleIn),		1,				(void*) &app->pShm->scl_set.Pad03ScaleIn,		"Pad03ScaleIn",			SCL_IN_GROUP,
	
	39,			_TDebugRec,				sizeof(app->pShm->dbg_set.debug_id),			1,				(void*) &app->pShm->dbg_set.debug_id,			"debug_id",				NO_GROUP,
	40,			_uint,					sizeof(app->pShm->dbg_set.dbg_trace),			1,				(void*) &app->pShm->dbg_set.dbg_trace,			"dbg_trace",			NO_GROUP,
	41,			_uint,					sizeof(app->pShm->dbg_set.dbg_output),			1,				(void*) &app->pShm->dbg_set.dbg_output,			"dbg_output",			NO_GROUP,
	42,			_uint,					sizeof(app->pShm->dbg_set.dbg_extra),			1,				(void*) &app->pShm->dbg_set.dbg_extra,			"dbg_extra",			NO_GROUP,
	43,			_uint,					sizeof(app->pShm->dbg_set.TestFireDrop),		1,				(void*) &app->pShm->dbg_set.TestFireDrop,		"TestFireDrop",			NO_GROUP,
	44,			_int,					sizeof(app->pShm->dbg_set.UdpTrace),			1,				(void*) &app->pShm->dbg_set.UdpTrace,			"UdpTrace",				NO_GROUP,
	45,			_int,					sizeof(app->pShm->dbg_set.Pad01DebugIn),		1,				(void*) &app->pShm->dbg_set.Pad01DebugIn,		"Pad01DebugIn",			NO_GROUP,
	46,			_int,					sizeof(app->pShm->dbg_set.Pad02DebugIn),		1,				(void*) &app->pShm->dbg_set.Pad02DebugIn,		"Pad02DebugIn",			NO_GROUP,
	47,			_int,					sizeof(app->pShm->dbg_set.Pad03DebugIn),		1,				(void*) &app->pShm->dbg_set.Pad03DebugIn,		"Pad03DebugIn",			NO_GROUP,	

	FIRST_STAT,	_uint,					sizeof(app->pShm->sys_stat.PPMTimer),			1,				(void*) &app->pShm->sys_stat.PPMTimer,			"PPMTimer",				NO_GROUP,
	49,			_int,					sizeof(app->pShm->sys_stat.raw_weight),			1,				(void*) &app->pShm->sys_stat.raw_weight,		"raw_weight",			NO_GROUP,
	50,			_uint,					sizeof(app->pShm->sys_stat.ShacklesPerMinute),	1,				(void*) &app->pShm->sys_stat.ShacklesPerMinute,	"ShacklesPerMinute",	NO_GROUP,
	51,			_uint,					sizeof(app->pShm->sys_stat.DropsPerMinute),		MAXSCALES,		(void*) &app->pShm->sys_stat.DropsPerMinute,	"DropsPerMinute",		NO_GROUP,
	DISP_DATA,	_TDisplayShackle,		sizeof(app->pShm->sys_stat.DispShackle),		1,				(void*) &app->pShm->sys_stat.DispShackle,		"DispShackle",			NO_GROUP,
	53,			___int64,				sizeof(app->pShm->sys_stat.TotalCount),			1,				(void*) &app->pShm->sys_stat.TotalCount,		"TotalCount",			NO_GROUP,
	54,			___int64,				sizeof(app->pShm->sys_stat.TotalWeight),		1,				(void*) &app->pShm->sys_stat.TotalWeight,		"TotalWeight",			NO_GROUP,
	55,			_char,					sizeof(app->pShm->sys_stat.adc_part),			ADCINFOMAX,		(void*) &app->pShm->sys_stat.adc_part,			"adc_part",				NO_GROUP,
	56,			_char,					sizeof(app->pShm->sys_stat.adc_version),		ADCINFOMAX,		(void*) &app->pShm->sys_stat.adc_version,		"adc_version",			NO_GROUP,
	57,			_TDropStats,			sizeof(app->pShm->sys_stat.MissedDropInfo),		MAXDROPS+1,		(void*) &app->pShm->sys_stat.MissedDropInfo,	"MissedDropInfo",		NO_GROUP,
	58,			_TDropStats1,			sizeof(app->pShm->sys_stat.DropInfo),			MAXDROPS,		(void*) &app->pShm->sys_stat.DropInfo,			"DropInfo",				NO_GROUP,
	59,			___int64,				sizeof(app->pShm->sys_stat.GradeCount),			MAXGRADES,		(void*) &app->pShm->sys_stat.GradeCount,		"GradeCount",			NO_GROUP,
	60,			_TScheduleStatus,		sizeof(app->pShm->sys_stat.ScheduleStatus),		MAXDROPS,		(void*) &app->pShm->sys_stat.ScheduleStatus,	"ScheduleStatus",		NO_GROUP,
	61,			_TDropStatus,			sizeof(app->pShm->sys_stat.DropStatus),			MAXDROPS,		(void*) &app->pShm->sys_stat.DropStatus,		"DropStatus",			NO_GROUP,
	62,			_uint,					sizeof(app->pShm->sys_stat.dbg_sync),			MAXSYNCS+1,		(void*) &app->pShm->sys_stat.dbg_sync,			"dbg_sync",				NO_GROUP,
	63,			_char,					sizeof(app->pShm->sys_stat.dbg_zero),			MAXINPUTBYTS,	(void*) &app->pShm->sys_stat.dbg_zero,			"dbg_zero",				NO_GROUP,
	LAST_STAT,	_char,					sizeof(app->pShm->sys_stat.dbg_switch),			MAXINPUTBYTS,	(void*) &app->pShm->sys_stat.dbg_switch,		"dbg_switch",			NO_GROUP,
	65,			_uint,					sizeof(app->pShm->AppFlags),					1,				(void*) &app->pShm->AppFlags,					"AppFlags",				NO_GROUP,
	66,			_TShackleTares,			sizeof(app->pShm->ShackleTares),				MAXPENDANT,		(void*) &app->pShm->ShackleTares,				"ShackleTares",			SHKL_TARES,
	67,			_TScheduleSettings,		sizeof(app->pShm->Schedule),					MAXDROPS,		(void*) &app->pShm->Schedule,					"Schedule",				SCHEDULE,
	68,			_uint,					sizeof(app->pShm->AutoTareStep),				1,				(void*) &app->pShm->AutoTareStep,				"AutoTareStep",			NO_GROUP,
	OPMODE,		_uint,					sizeof(app->pShm->OpMode),						1,				(void*) &app->pShm->OpMode,						"OpMode",				NO_GROUP,
	70,			_uint,					sizeof(app->pShm->raw_scale),					1,				(void*) &app->pShm->raw_scale,					"raw_scale",			NO_GROUP,
	ZERO_CTR,	_uint,					sizeof(app->pShm->ZeroCounter),					MAXSCALES,		(void*) &app->pShm->ZeroCounter,				"ZeroCounter",			NO_GROUP,
	72,			_int,					sizeof(app->pShm->ZeroAverage),					MAXAVGCNT,		(void*) &app->pShm->ZeroAverage,				"ZeroAverage",			NO_GROUP,
	73,			_THostDropRec,			sizeof(app->pShm->HostDropRec),					MAXDROPRECS,	(void*) &app->pShm->HostDropRec,				"HostDropRec",			NO_GROUP,
	74,			_TIsysDropStatus,		sizeof(app->pShm->IsysDropStatus),				MAXDROPS,		(void*) &app->pShm->IsysDropStatus,				"IsysDropStatus",		NO_GROUP,
	75,			_uint,					sizeof(app->pShm->IfServerHeartbeat),			1,				(void*) &app->pShm->IfServerHeartbeat,			"IfServerHeartbeat",	NO_GROUP,
	76,			_uint,					sizeof(app->pShm->AppHeartbeat),				1,				(void*) &app->pShm->AppHeartbeat,				"AppHeartbeat",			NO_GROUP,
	77,			_TIsysLineStatus,		sizeof(app->pShm->IsysLineStatus),				1,				(void*) &app->pShm->IsysLineStatus,				"IsysLineStatus",		NO_GROUP,
	78,			_TSyncStatus,			sizeof(app->pShm->SyncStatus),					MAXSYNCS,		(void*) &app->pShm->SyncStatus,					"SyncStatus",			NO_GROUP,
	SHKSTAT,	_TShackleStatus,		sizeof(app->pShm->ShackleStatus),				MAXPENDANT,		(void*) &app->pShm->ShackleStatus,				"ShackleStatus",		NO_GROUP,
	SCLWGHZ,	_DBOOL,					sizeof(app->pShm->WeighZero),					MAXSCALES,		(void*) &app->pShm->WeighZero,					"WeighZero",			NO_GROUP,
	81,			_uint,					sizeof(app->pShm->spare_int),					1,				(void*) &app->pShm->spare_int,					"spare_int",			NO_GROUP,
	82,			_uint,					sizeof(app->pShm->WeighShackle),				MAXSCALES,		(void*) &app->pShm->WeighShackle,				"WeighShackle",			NO_GROUP,
	GRD_SHKL,	_uint,					sizeof(app->pShm->grade_shackle),				MAXGRADESYNCS,	(void*) &app->pShm->grade_shackle,				"grade_shackle",		NO_GROUP,
	SYS_SET,	_uint,					sizeof(app->pShm->sys_set),						1,				(void*) &app->pShm->sys_set,					"System Settings",		SYS_IN_GROUP,
	SCL_SET,	_uint,					sizeof(app->pShm->scl_set),						1,				(void*) &app->pShm->scl_set,					"Scale Settings",		SCL_IN_GROUP,
	TARES,		___int64,				sizeof(app->pShm->ShackleTares),				1,				(void*) &app->pShm->ShackleTares,				"Tares",				SHKL_TARES,
	87,			_uint,					sizeof(app->pShm->Schedule),					1,				(void*) &app->pShm->Schedule,					"Schedule",				SCHEDULE,
	STATID,		_uint,					sizeof(app->pShm->sys_stat),					1,				(void*) &app->pShm->sys_stat,					"SysStatus",			NO_GROUP,
	89,			_uint,					sizeof(app->pShm->dbg_set),						1,				(void*) &app->pShm->dbg_set,					"Debug Settings",		NO_GROUP,
	90,			___int64,				sizeof(app->pShm->AutoBias),					MAXSCALES,		(void*) &app->pShm->AutoBias,					"AutoBias",				NO_GROUP,
	MBX_STAT,	_mbx_state,				sizeof(app->pShm->mbx_status),					MBX_STATUSES,	(void*) &app->pShm->mbx_status,					"Mailbox Status",		NO_GROUP,
	APPVER,		_char,					sizeof(app->pShm->app_ver),						MAXVERINFO,		(void*) &app->pShm->app_ver,					"App Version",			NO_GROUP,
	COMVER,		_char,					sizeof(app->pShm->comm_ver),					MAXVERINFO,		(void*) &app->pShm->comm_ver,					"Comm Version",			NO_GROUP
};

fsave_grp grp_tbl[MAX_GROUPS] = {
	0,			false,	false, "",				NULL,						NULL,							"No Group",
	SYS_SET,	false,	false, SYS_FILE_PATH,	sizeof(pShm->sys_set),		(void*) &pShm->sys_set,			"System Settings",
	SCL_SET,	false,	false, SCL_FILE_PATH,	sizeof(pShm->scl_set),		(void*) &pShm->scl_set,			"Scale Settings",
	TARES,		false,	false, TARE_FILE_PATH,	sizeof(pShm->ShackleTares),	(void*) &pShm->ShackleTares,	"Tares",
	87,			false,	false, SCH_FILE_PATH,	sizeof(pShm->Schedule),		(void*) &pShm->Schedule,		"Schedule"
};

//int i; //GLC

    // update the table
    memcpy((void*) shm_tbl, tbl, sizeof(tbl));
    // update the group table
    memcpy((void*) fsave_grp_tbl, grp_tbl, sizeof(grp_tbl));

//----- Some miscellaneous prints to see sizes for debug, should normally be commented out.

    /*
    for ( int i = 0; i < MAXIDS; i++ )
        RtPrintf("id %d size %u\n", shm_tbl[i].id, shm_tbl[i].struct_len);

    for ( i = 1; i < MAX_GROUPS; i++ )
        RtPrintf("%s size %u\n", fsave_grp_tbl[i].fname, fsave_grp_tbl[i].struct_size);

        RtPrintf("mhdr size %u\n", sizeof(mhdr));
        RtPrintf("stat size %u\n", sizeof(pShm->sys_stat));

     RtPrintf(" THostDropRec size %u\n",sizeof(THostDropRec));
     RtPrintf("fdrec_info %u\n", sizeof(fdrec_info));
     RtPrintf("ftot_info %u\n",  sizeof(ftot_info));
    */

    shm_valid = true;


}

void overhead::InitLoadCell()
{

//------------- Initialize the LoadCell card
	Serial*  pser = NULL;


//#ifdef _1510_LOAD_CELL_
	RtPrintf("InitLoadCell::LoadCellType = %d\n",app->pShm->scl_set.LoadCellType);
	if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 )
	{
		RtPrintf("Using loadcell LOADCELL_TYPE_1510\n");


        if ((ld_cel[0] = new LoadCell_1510) == NULL)
        {
            sprintf(app_err_buf,"Can't create loadcell obj %s line %d\n", _FILE_, __LINE__);
            GenError(critical, app_err_buf);
        }

        //----- Create/Open Mutex for load cell

        load_cell_mutex[MAINBUFID] = RtCreateMutex( NULL, FALSE, "loadCell");

        if ( load_cell_mutex[MAINBUFID] == NULL )
        {
            sprintf(app_err_buf,"Can't create load cell mutex %s line %d\n", _FILE_, __LINE__);
            GenError(critical, app_err_buf);
        }

        load_cell_mutex[GPBUFID] = RtOpenMutex( NULL, FALSE, "loadCell");

        if ( load_cell_mutex[GPBUFID] == NULL )
        {
            sprintf(app_err_buf,"Can't open load cell mutex %s line %d\n", _FILE_, __LINE__);
            GenError(critical, app_err_buf);
        }

        // Request Mutex for reading/writing
        if(RtWaitForSingleObject(load_cell_mutex[MAINBUFID], WAIT50MS) != WAIT_OBJECT_0)
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        else
        {
            //ld_cel[0]->init_adc(AVG, 2);
			ld_cel[0]->init_adc(app->pShm->scl_set.AdcMode[0], app->pShm->scl_set.NumAdcReads[0]);

            if(!RtReleaseMutex(load_cell_mutex[MAINBUFID]))
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        }
	}


//------------- GLC Initialize the serial load cell

	if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_HBM )
	{
		RtPrintf("Using loadcell LOADCELL_TYPE_HBM\n");

		pser = new Serial(ucbList[LOADCELL_1]);

		// At this point we should know how many Load Cells we need to init.
		if ((HBMLc = new HBMLoadCell) == NULL)
		{
			RtPrintf("Error file %s, line %d \n",_FILE_,__LINE__);
			RtExitProcess(1);
		}
		else
		{
			pHBMLoadCellInit1 = (HBMLoadCell::HBMinit*)RtAllocateLockedMemory(sizeof(HBMLoadCell::HBMinit));
			DebugTrace(_HBMLDCELL_, "HBMLoadCell object created.\n");
			pHBMLoadCellInit1->pHBMLoadCell = HBMLc;
			pHBMLoadCellInit1->pHBMLoadCell->LoadCellNum = LOADCELL_1;
			pHBMLoadCellInit1->pSerial = pser;

			HBMLc->initialize(pHBMLoadCellInit1);
			ld_cel[LOADCELL_1] = HBMLc;

			// Initialize these in case the front-end changes
			// NumLoadCells after we have gone through the
			// Loadcell init. Prevents us from referencing
			// a NULL pointer.
			ld_cel[LOADCELL_2] = HBMLc;
			ld_cel[LOADCELL_3] = HBMLc;
			ld_cel[LOADCELL_4] = HBMLc;
		}
		//----- Create/Open Mutex for load cell

		load_cell_mutex[MAINBUFID] = RtCreateMutex( NULL, FALSE, "loadCell");

		if ( load_cell_mutex[MAINBUFID] == NULL )
		{
			sprintf(app_err_buf,"Can't create load cell mutex %s line %d\n", _FILE_, __LINE__);
			GenError(critical, app_err_buf);
		}

		load_cell_mutex[GPBUFID] = RtOpenMutex( NULL, FALSE, "loadCell");

		if ( load_cell_mutex[GPBUFID] == NULL )
		{
			sprintf(app_err_buf,"Can't open load cell mutex %s line %d\n", _FILE_, __LINE__);
			GenError(critical, app_err_buf);
		}

		Sleep (500); //GLC TEMP

		// If a dual scale system, init the second loadcell
		if (pShm->scl_set.NumScales > 1)
		//if (app->dual_scale)
        {
		    pser = new Serial(ucbList[LOADCELL_2]);

		    if ((HBMLc2 = new HBMLoadCell) == NULL)
		    {
			    RtPrintf("Error file %s, line %d \n",_FILE_,__LINE__);
			    RtExitProcess(1);
		    }
		    else
		    {
			    pHBMLoadCellInit2 = (HBMLoadCell::HBMinit*)RtAllocateLockedMemory(sizeof(HBMLoadCell::HBMinit));
			    DebugTrace(_HBMLDCELL_, "HBMLoadCell2 object created.\n");
			    pHBMLoadCellInit2->pHBMLoadCell = HBMLc2;
				pHBMLoadCellInit2->pHBMLoadCell->LoadCellNum = LOADCELL_2;
			    pHBMLoadCellInit2->pSerial = pser;

			    HBMLc2->initialize(pHBMLoadCellInit2);
			    ld_cel[LOADCELL_2] = HBMLc2;
		    }
		}

        for(int i=0;i<pShm->scl_set.NumScales;i++) //GLC TEMP NEED TO ADD BACK IN
        {
            if (RtWaitForSingleObject(hLoadCellInitialized[i], 5000) != WAIT_OBJECT_0) // GLC added 2/19/04
            {
                sprintf(app_err_buf,"Load Cell %d initialization failed. Error %d file %s, line %d \n", i+1, GetLastError(), _FILE_, __LINE__);
                GenError(critical, app_err_buf);
                RtPrintf(app_err_buf); //"Load Cell %d initialization failed. Error %d file %s, line %d \n", i+1, GetLastError(), _FILE_, __LINE__);
            }
            else
            {
                RtPrintf("Load Cell %d initialized.\n", i + 1); //GLC TEMP
            }

			// Initialize sample counter
			app->WriteLCReadsToFile = app->pShm->scl_set.NumScales;
        }
    }
}


//--------------------------------------------------------
//  AnalyzeConfig()
//--------------------------------------------------------

void overhead::AnalyzeConfig(bool Initialize)
{
	int x;
	bool	New_Required = false;
	int		New_InputRegisters = 0;
	int		New_No_of_Inputs = 0;
	int		New_No_of_Outputs = 0;
	int		New_Drops[24];
	bool	New_Input[24];
	bool	New_Output[24];

	for (x = 0; x < 24; x++)
	{
		New_Drops[x] = 0;
		New_Input[x] = false;
		New_Output[x] = false;
	}

	//	Go through all drops and establish 2nd 3724 card requirements
	for (x = 0; x < pShm->sys_set.NumDrops; x++)
	{
		//	Get SwitchLEDIndex for drop for use inside of switch statement
		int SwitchLEDIndex = pShm->sys_set.DropSettings[x].SwitchLEDIndex;
		//	Set requirements for each drop depending on the mode it is in
		switch (pShm->sys_set.DropSettings[x].DropMode)
		{
			//	Remote Reset requires an Input for switch, an Output for LED and we need to know the Drop the LED is assigned to
			case DM_REMOTERESET:
				New_No_of_Inputs = max(New_No_of_Inputs, SwitchLEDIndex);
				New_No_of_Outputs = max(New_No_of_Outputs, SwitchLEDIndex);
				New_Drops[SwitchLEDIndex - 1] = x + 1;
				break;
			//	Reassignment only requires an Input
			case DM_REASSIGN:
				New_No_of_Inputs = max(New_No_of_Inputs, SwitchLEDIndex);
				New_Drops[SwitchLEDIndex - 1] = x + 1;
				break;
			//	Normal drops require nothing
			case DM_NORMAL:
				break;
			//	Drop not setup properly from front end set drop to DM_NORMAL and LED to 0
			default:
				pShm->sys_set.DropSettings[x].DropMode = DM_NORMAL;
				pShm->sys_set.DropSettings[x].SwitchLEDIndex = 0;
				RtPrintf("Invalid configuration for Drop %d, set to mode to DM_NORMAL with index set to 0\n", x+1); 
				break;
		}	//	End of switch (pShm->sys_set.DropSettings[x].DropMode)
	}	// End of for (int x = 0; x < pShm->sys_set.NumDrops; x++)

	//	If we have any input or output requirements we need the card
	New_InputRegisters = (New_No_of_Inputs / 8) + ((New_No_of_Inputs) % 8 ? 1 : 0);
	if ((New_No_of_Inputs + New_No_of_Outputs) > 0)
	{
		New_Required = true;
	}

	//	See if initial configuration or if configuration changed
	bool Changed =	Initialize |
					(New_InputRegisters != Second_3724_InputRegisters) |
					(New_Required != Second_3724_Required) |
					(New_No_of_Inputs != Second_3724_No_of_Inputs) |
					(New_No_of_Outputs != Second_3724_No_of_Outputs);

	if (!Changed)
	{
		for (x = 0; x < 24; x++)
		{
			Changed =	Changed |
						(New_Drops[x] != Second_3724_Drops[x]);
		}
	}

	//	If it changed, notify and then overlay old configuration with new configuration
	//	If it didn't, do nothing
	if (Changed)
	{
		Second_3724_Required = New_Required;
		Second_3724_InputRegisters = New_InputRegisters;
		Second_3724_No_of_Inputs = New_No_of_Inputs;
		Second_3724_No_of_Outputs = New_No_of_Outputs;
		RtPrintf("Drops[x] =");
		for (x = 0; x < 24; x++)
		{
			Second_3724_Drops[x] = New_Drops[x];
			Second_3724_Input[x] = New_Input[x];
			Second_3724_Output[x] = New_Output[x];
			Second_3724_DropDisabled[x] = false;
		}
		RtPrintf("\n");
		Second_3724_WriteOutput = true;
	}
}

void overhead::SetIsysFastestIndices(int DropIdx, bool Default)
{
	int StartIdx = (DropIdx < 0) ? 0 : DropIdx;
	int EndIdx = (DropIdx < 0) ? MAXDROPS : (DropIdx + 1);

	//	For all the possible drops on a line
	for (int Drop = StartIdx; Drop < EndIdx; Drop++)
	{

		//	Get the DropMaster for this drop index
		int drop_master;
		GET_DROP_MASTER(Drop, pShm);

		//	If this is an InterSystem drop
		if (drop_master != NOT_AN_ISYS_DROP)
		{
			int DropCount = 0;
			int Batched = 0;
			int NotBatchedIndex = 0;

			// Get the total number of batched drops for this ISYS grouping
			for (int LineIndex = 0; LineIndex < MAXIISYSLINES; LineIndex++)
			{
				if ( pShm->sys_set.IsysLineSettings.active[LineIndex] &&
					pShm->sys_set.IsysDropSettings[Drop].active[LineIndex] )
				{
					// Get the number of lines dropping here
					DropCount++;

					// Now check if the line has batched out for this drop
					if ( (app->pShm->IsysDropStatus[Drop].flags[LineIndex] & ISYS_DROP_BATCHED) != 0)
					{
						Batched++;
					}
					else
					{
						// Only interested in the case were one line is all that's open
						NotBatchedIndex = LineIndex;
					}
				}
			}

			//	If only one line open
			if (Batched == (DropCount - 1))
			{
				isys_fastest_idx[Drop] = NotBatchedIndex;
			}
			else
			{
				//	If we are just setting default values
				if (Default)
				{
					//	Set default values for DropMaster being the fastest
					isys_fastest_idx[Drop] = drop_master;
				}
				else
				{
					//	Set temporary fastest Index & BPM to zero for each Intersystems drop
					int isys_fst_idx = -1;
					int isys_fst_bpm = -1;

					//	Loop through all the lines
					for (int Line = 0; Line < MAXIISYSLINES; Line++)
					{
						//	if this line and the drop on this line are active, the drop is not batched and the drops last PPMCount > 0
						if (pShm->sys_set.IsysLineSettings.active[Line] && 
							pShm->sys_set.IsysDropSettings[Drop].active[Line] &&
							!(pShm->IsysDropStatus[Drop].flags[Line] & ISYS_DROP_BATCHED) &&
							(pShm->IsysDropStatus[Drop].PPMCount[Line] > 0))
						{
							int PPM = pShm->sys_stat.PPMTimer < 15 ? pShm->IsysDropStatus[Drop].PPMPrevious[Line] : pShm->IsysDropStatus[Drop].PPMCount[Line];
							//	If the PPMCount for the Drop on this Line
							if (PPM > isys_fst_bpm)
							{
								//	Set this Line as the fastest
								isys_fst_idx = Line;
								//	Set the fastest BPM to the PPMCount for this Line
								isys_fst_bpm = PPM;
							}	//	End If the PPMCount for the Drop on this Line
						}	//	End if this line and the drop on this line are active
					}	//	End Loop through all the lines

					//	If we found a valid fastest index, otherwise leave it where it was
					if (isys_fst_idx != -1)
						isys_fastest_idx[Drop] = isys_fst_idx;

				}	//	End If we are not just setting default values
			}	// End If only one line open
		}	//	End If this is an InterSystem drop
	}	//	End For all the possible drops on a line
}