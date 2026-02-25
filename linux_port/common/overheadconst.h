#pragma once

//--------------------------------------------------------------------------------
// Build/Setup (3 Parts):
//--------------------------------------------------------------------------------

// ---------------- Part1: I/O Board/CPU specific defines. ------------------- 

// Follow the configuration STEPS !!!

// STEP 1: pick a board type

#define VSBC6

// STEP 2: for each board type, pick the I/O defines, based on board type

#ifdef VSBC6
//#define _1510_LOAD_CELL_

#define LOADCELL_TYPE_1510	0
#define LOADCELL_TYPE_HBM	1

#define _3724_IO_   // 6 primary i/o ports
#define _VSBC6_IO_  // 2 secondary i/o ports
#endif

// The following are mainly used to provide a picture on 
// what should be used where. There is some checking in the defines
// Make sure what's defined matches the i/o routines.

// STEP 3: configure the _3724_IO_ (primary i/o)

#ifdef _3724_IO_
    // inputs
    #define ZERO_0_LO         0  // PCM_3724_PORT_A0 (odd)
    #define ZERO_0_HI         0  // PCM_3724_PORT_B0 (odd)
    #define SYNC_0_LO         0  // PCM_3724_PORT_A0 (even)
    #define SYNC_0_HI         0  // PCM_3724_PORT_B0 (even)
    #define SWITCH_0          0  // PCM_3724_PORT_C0
    // outputs
    #define OUTPUT_0          0  // PCM_3724_PORT_A1 
    #define OUTPUT_1          1  // PCM_3724_PORT_B1 
    #define OUTPUT_2          2  // PCM_3724_PORT_C1 
#endif

// STEP 4: configure the _VSBC6_IO_ (secondary i/o)

#ifdef _VSBC6_IO_

// STEP 5: on the secondary i/o, DEFINE ONE OF THE TWO BELOW

//  #define VSBC6_LOWIN_HIOUT   // 1 in & 1 out (wkr, has not been debugged)
    #define VSBC6_LOWOUT_HIOUT  // 0 in & 2 out 

    // baised on the above
    #ifdef VSBC6_LOWIN_HIOUT
        #define SWITCH_1          1  // VSBC6_INPUT_PORT_LO
        #define OUTPUT_3          3  // VSBC6_OUTPUT_PORT_HI
    #endif

    #ifdef VSBC6_LOWOUT_HIOUT
        #define OUTPUT_3          3  // VSBC6_OUTPUT_PORT_LO
        #define OUTPUT_4          4  // VSBC6_OUTPUT_PORT_HI
    #endif

    // both defined is in invalid, undef all
    #ifdef VSBC6_LOWIN_HIOUT
    #ifdef VSBC6_LOWOUT_HIOUT
        #undef SWITCH_1
        #undef OUTPUT_3
        #undef OUTPUT_4
    #endif
    #endif

// STEP 6: recheck STEP(s) 1 - 5, then continue to STEP 7.

#endif

// STEP 7: Change version numbers. 

// Change before a label is applied in SS

#define APP_VER1         15 // Major
#define APP_VER2         6  // Minor
#define APP_VER3         27	// Local

#define CREATE_VER_STRING(string) \
    sprintf((char *)string, "GS-1000 RTOS Version %d.%d.%d %s ", \
            APP_VER1, \
            APP_VER2, \
            APP_VER3, \
            __DATE__);

// STEP 8: Build for no interface/debug/simulation mode 

//#define _SIMULATION_MODE_ //GLC  -- enables built-in sync/grade simulator (bypasses I/O)
//#define _WEIGHT_SIMULATION_MODE_ //GLC  -- enables simulated weights (skips real load cell) -- CONFLICTS with _HBM_SIM_
//#define _HBM_SIM_      // Virtual HBM FIT7 load cell simulator (exercises full serial path)
                         // Disabled: now using real RS-485 serial to signal generator's HBM responder
//#define _ZERO_SIM_WEIGHT_
//#define _NO_INTERFACE_
//#define _SIM_LAPTOP_	// Simulation build for laptop

#define _TELNET_UDP_ENABLED_ // Define to include telnet server and UDP tracing
//#define _DEBUGING_           // should only be used if no interface is defined
//#define _REDIRECT_DEBUG_       // should only be used if no interface is defined
#define _LOCAL_DEBUG_		   // RtPrintf/UDP print are used locally only
//#define _SHOW_HANDLES_       // show handles on initialization
//#define _SHOW_STARTUP_       // show misc startup messages on initialization
//#define _TEST_RIG_           // wkr, for testing gib feature on test system with 2 different range loadcell
//#define _HOST_TIMING_TEST_     // for timing pipe blocked time on messages (cmd 302 ID 88 21ms-23ms)
//#define _DREC_TIMING_TEST_   // time drop record messages to host
//#define _LREC_TIMING_TEST_   // time last drop record messages to host 
//#define _SHMW_TIMING_TEST_   // time shared memory write to host  (2ms - 150ms 9/10/04)
//#define _ISYS_TIMING_TEST_	   // ISYS_PORTCHK (5ms - 66ms)
//#define _MEASUREQ_ALLOW_MULTI_READ_	// Allow MeasureQ function to read values
										// out of the measure queue that have
										// aready been read. Otherwise the function
										// returns only the latest reading(s) that
										// have not been previously read.

//#define _PC104_BBSRAM_         // If going to run on regular machine, comment this out.
#ifdef  _PC104_BBSRAM_
/* SRAM physical location and size (Dallas 1245AB)*/
#define BBSRAMBASEADDR      0xE0000
#define BBSRAMSIZE          ((ULONG)  0x0000FFFFUL)
#define MPCRTLREG           ((PUCHAR) 0xE3)
#define MPCRTLBYTES         ((UCHAR)  0x1)
#define BBSRAMENABLE        ((UCHAR)  0x20)
#define BBSRAMDISABLE       ((UCHAR)  0x00)
#endif
//--------------------------------------------------------------------------------
// Normal application defines
//--------------------------------------------------------------------------------

//----- Error processing
#define DEAD_COUNT          8 
#define NO_ERRORS           0
#define ERROR_OCCURED       -1
#define WAIT100MS           100
#define WAIT200MS           200
#define WAIT50MS            50
#define WAIT25MS            25

//----- Defines for InterSystems

#define HOST_ID             5
#define MAXIISYSLINES       4
#define HOST_INDEX          MAXIISYSLINES
#define MAXIISYSNAME        64
#define MAXSLDWNPERLINE     4 // 4 bird max slowdown/per line
// Flags/per drop basis
#define NOT_AN_ISYS_DROP    -1
#define BATCH_MASK          0xFFFFF
#define TEMP_BATCH_NUM      1
#define LINE_MASK           0x300000
#define LINE_SHIFT          20 // bits to shift to get/put line number
// Message queue and commands
#define ISYS_MAXRQENTRYS   50
#define ISYS_MAXWQENTRYS   100 // RCB - Debug for PPM late reset issue
#define ISYS_UNUSEDQ       0x00000000
#define ISYS_USEDQ         0x00000001
#define ISYS_ACTIVEQ       0x00000002
#define ISYS_WROTEQ        0x00000004
#define ISYS_ACK_REQ       0x00000008
#define ISYS_ADDQ          1
#define ISYS_DELQ          2
#define ISYS_DELQ1         3
#define ISYS_MSGTMO        300
#define ISYS_BAD           0
#define ISYS_GOOD          1
#define FATAL_ERR_CNT       60

#define W32_TX_SVR_RDY        0x00000001
#define W32_HST_TX_SVR_RDY    0x00000002
#define W32_PIPE_SVR_RDY      0x00000004
#define RTSS_APP_MBX_SVR_RDY  0x00000008 
#define RTSS_DM_MBX_SVR_RDY   0x00000010 
#define RTSS_IS_MBX_SVR_RDY   0x00000020
#define RTSS_IS_RX_SVR_RDY    0x00000040
#define ALL_SYSTEMS_READY     0x0000007f

#define MBX_STATUSES          4
#define MBX_HUNG_CNT          25
#define HOST_MBX              0
#define DMGR_MBX              1
#define ISYS_MBX              2
#define LINE_MBX              3

// Intersystems multipurpose defines

// The flags bits in the isys drop struct is being used
// as a combination of local and remote bits.
// The local is being used to easily clear
// old flags and or in the new. 
// !!! If any local bits are added, shift the remote bits left !!!
#define ISYS_LOCAL_MASK     0x3 
// Remote bits
#define ISYS_DROP_SUSPENDED 0x00000004
#define ISYS_DROP_BATCHED   0x00000008
#define ISYS_DROP_OVERRIDE	0x00000010

//----- Normal Application defines, most are for shared memory

#define RUN_APP             1
#define ADCINFOMAX          8
#define ADCOFFSET           -2
//#define ADCREADS            3
#define ADCREADSDFLT        5
#define ADCREADSMAX         600
#define MAXSCALES           2
#define MAXGRADESYNCS       2
#define MAXLOADCELLS        2
#define READ_INDEXES 2			// Number of read indexes for product capture mode
#define MAXLCSTUCKCOUNTRAW	20 //For digital loadcell - max consecutive unchanged raw mode readings before alarm
#define MAXLCSTUCKCOUNTRUN	50 //For digital loadcell - max consecutive unchanged weights before alarm

#define BOAZSYNCS           6  //Boas mode jdc

#define MAXSYNCS            16
#define MAXDROPS            32
#define MAXERRORS           39
#define MAXERRMBUFSIZE      256
#define MAXTRCIDHDRSIZE     16      // the length of buffer origin string
#define MAXDATETIMESTR      48      // date/time strings
#define MAXPENDANT          2000    // max number of pendants
#define MAXDROPRECS         200     // buffer of birds dropped
#define DROPRECHISP         100     // high water mark, if exceeded save to files
#define MAXSAVERECS         500     // file i/o buffer for saving drop records
#define MINDSAVREC          10      // buffer of birds dropped
#define MAXDSAVFILES        100
#define MAXBCHLABELS        64
#define LEN_UNKNOWN         99      // length of file unknown
#define MAXAVGCNT           10      // # readings for auto bias average
#define MAXLCMUTEX          2       // # threads to use loadcell
#define MAXGRADES           5
#define NOTDROPPED          0
#define DROPPED             1
#define PCRECUSED           1
#define PCRECSENT           2
#define MAXINPUTBYTS        4
#define MAXOUTPUTBYTS       5
#define MAXFILEBUFFER       50000
#define TRICKLE_SAMPLES     12
#define MAINBUFID           0       // buffer origin id (oh timer)
#define GPBUFID             1       // buffer origin id (gp timer)
#define EVTBUFID            2       // buffer origin id (event handler)
#define DBGBUFID            3       // buffer origin id (event handler)
#define DMMBXBUFID          4       // buffer origin id (drop manager mailbox)
#define ISYSMAINBUFID       5       // buffer origin id (isys main)
#define ISYSMBXBUFID        6       // buffer origin id (isys mailbox)
#define FASTBUFID           7       // buffer origin id (fast update)
#define MAXTRCBUFFERS       8       // how many trace buffers (BUFID's above)
#define MAXTRCMBUFSIZE      4096    // trace buffer size
#define TRCMBUFFULL         3500    // stop trace, to protect against overrun 
#define DRECRECOVERY        1       // sending a large chunk of drop records
#define MAXBCHLBLNUM        1000    // max label number (999999)
#define MAXCAPTUREBUFFS     2
#define CAPTURE_SPEED       3
#define SAMPLE_WEIGHTS      1000
#define AVG_WEIGHTS         1000
#define MAXVERINFO          64
#define MAX2BPROCESSED      MAXTRCMBUFSIZE  // whatever is the largest message (no tares)

// Grading
#define GRADEZERO2BIT		7	// For a 2 Grade sync system (Alt MB_BIT2)
#define	GRADESYNC2BIT		5	// For a 2 Grade sync system (Alt GRADE4_BIT)

#define GRADE5_BIT          6   // far from scale
#define GRADE4_BIT          5   // (Alt GRADESYNC2BIT)
#define AUTOSHUTDOWN        5
#define GRADE3_BIT          4
#define GRADE2_BIT          3
#define GRADE1_BIT          2
#define GRADE0_BIT          0   // (no bit connected) near scales
#define GRADEZEROBIT        1
#define GRADESYNCBIT        0
//Missed Bird 
#define MB_BIT1             6
#define MB_BIT2             7
#define MB_NSET             0
#define MB_SET              1
// Scales/Weight
#define SCALE1SYNCBIT       0
#define SCALE2SYNCBIT       1
#define WeighIdle           0
#define WeighActive         1
#define WeighAverage        2
#define WeighDone           3
#define FUNC_RAW            0   // Function for INDIV_READS macro
#define FUNC_AVG            1   // it us used to determine where
#define FUNC_CAPT           2   // to put weight
// Application run modes, wkr need to redefine modes
#define ModeNone            0
#define ModeStart           1
#define ModeATare1          3
#define ModeATare2          4
#define ModeATare3          5
#define ModeRun             6
#define ModeRaw             12
#define ModeASpan1          13
#define ModeASpan2          14

#define SYNC_ON             0
#define ZERO_ON             1

#define USE_ZERO_BIAS_AVG   1
#define USE_ZERO_BIAS_LIM   2

// These are weight capture modes
enum 
{
    no_capt = 0,  
    cont_capt,
    prod_capt
};

// These are cases for the weight averaging function
enum 
{
    read_window,
    compute_average,
    reaverage_using_limits,
    wt_average_finish
};

// error severity
enum 
{
    critical = 1,  
    warning,
    informational,
	logonly
};

// These are the error code catagories
enum 
{
    early_zero = 1,  
    late_zero,
    system_error
};

enum
{
// Critical errors
/*  1*/    ovh_timer_fail = 1,
/*  2*/    gen_purpose_timer_fail,
/*  3*/    io_3724_fail,
/*  4*/    com1_fail,
/*  5*/    com2_fail,
/*  6*/    intersystems_init_error,
/*  7*/    drop_manager_init_error,
/*  8*/    drop_manager_mbx_error,
/*  9*/    interface_heartbeat,
/* 10*/    io_1510lc_fail,
/* 11*/    shutdown_hndlr_init_fail,
/* 12*/    serial_init_fail,
/* 13*/    io_vsbc_fail,
/* 14*/    rx_mbx_error,
/* 15*/    intersystems_mbx_error,
/* 16*/    com3_fail,
/* 17*/    app_mbx_error,
/* 18*/    isys_thread_error,
/* 19*/    file_rw_alloc_error,
/* 20*/    com4_fail,


// Less critical errors
/* 100*/   critical_errors = 100,
/* 101*/   debug_task_fail,
/* 102*/   simulator_init_error,
/* 103*/   drop_sync_unassigned,
/* 104*/   drop_rec_buf_full,
/* 105*/   com1_normal,
/* 106*/   com2_normal,
/* 107*/   com3_normal,
/* 108*/   com4_normal

};

enum 
{
    search_locals,
    search_remotes,
    clear_drops,
	search_short_batch
};

// !!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!!!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!

// If commands are added, update the string 
// table (dist_mode_desc) at the top of overhead.cpp.
// It translates the distribution mode to a description.

enum 
{
    mode_1  = 1,			// weight & grade are implied for all modes
    mode_2,					// just turn on global grading flag
    mode_3_batch,			// Batch count mode
    mode_4_rate,			// BPM
    mode_5_batch_rate,		// BPM w/Batch
    mode_6_batch_alt_rate,	// Loop w/Batch
    mode_7_batch_alt_rate,	// Loop w/Batch & BPM
};

// If commands are added, update the string 
// table (bpm_mode_desc) at the top of overhead.cpp.
// It translates the bpm mode to a description.

enum 
{
    bpm_normal = 0,
    bpm_trickle
};

enum {

// used with isys_actual_state[]
// If commands are added, update the string 
// table (drp_state_desc) at the top of overhead.cpp.
// It translates the action to description.

 /* 0*/   isys_drop_set_flags,
 /* 1*/   isys_drop_bpm_reset,
 /* 2*/   isys_drop_batch_loop_reset,
 /* 3*/   isys_drop_batch_req,
 // for add Q routine
 /* 4*/   isys_send_single,
 /* 5*/   isys_send_common,
 /* 6*/   isys_send_all,
 // for control isys drop routine
 /* 7*/   isys_chk_batch_cnt,
 /* 8*/   isys_chk_batch_wt,
 /* 9*/   isys_chk_bpm_cnt,
 // miscellaneous, setting them to 99 makes the easy to see in debug
 /*98*/   no_drop_avaiable        = 98,
 /*99*/   wait_on_drop            = 99,
 /*99*/   isys_send_all_drops     = 99,
 /*99*/   bad_sync                = 99
};

// Some return codes for Checkrange
// If commands are added, update the string 
// table (drp_state_desc) at the top of overhead.cpp.
// It translates the action to description.

enum 
{
    drop_it = 0,
    weight_out_of_range,
    grade_out_of_range,
    trickle_limit,
    hole_in_logic
};

//	DropMode values
enum
{
	DM_NORMAL = 1,
	DM_REMOTERESET,
	DM_REASSIGN
};


// !!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!!!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!

// These defines are for settings group structures in shared memory
enum {
    NO_GROUP = 0,
    SYS_IN_GROUP,
    SCL_IN_GROUP,
    SHKL_TARES,
    SCHEDULE,
    MAX_GROUPS = 5
};
// Hard coded shared memory Ids. Any Id which will be used in code should
// be defined here and the define put in the table. If any Ids are added/
// removed, the define will be seen and can be changed globaly by changing
// it here.
#define DROP_SET			16	//	drop settings
#define LSID                21  // line settings id.
#define FIRST_STAT          48 //38 //35
#define DISP_DATA           52 //42 //39
#define LAST_STAT           64 //54 //51
#define OPMODE              69 //59 //56
#define ZERO_CTR            71 //61 //58
#define SHKSTAT             79 //69 //66
#define SCLWGHZ             80 //70 //67
#define GRD_SHKL            83 //73 //70
#define SYS_SET             84 //74 //71
#define SCL_SET             85 //75 //72
#define TARES               86 //76 //73
#define STATID              88 //78 //75
#define MBX_STAT            91 //81 //78
#define APPVER              92 //82 //79
#define COMVER              93 //83 //80

// These are for the DebugThread task
// They were made out of the actual data type 
// with a "_" for convienence and are used to 
// know how to format the data.

#define MAXDBGIDS           5
#define MAXIDS              93 //83 //GLC 79 //76
#define ALL_SHM_IDS         MAXIDS + MAX_GROUPS

enum {
    _int,
    _uint,
    ___int64,
    _DBOOL,
    _char,
    _TShackleTares,
    _TSyncSettings,
    _TScheduleSettings,
    _TOrder,
    _TDropSettings,
    _TGradeSettings,
    _TIsysDropSettings,
    _TDebugRec,
    _TIsysLineSettings,
    _TDropStats,
    _TDropStats1,
    _THostDropRec,
    _TIsysDropStatus,
    _TScheduleStatus,
    _TSyncStatus,
    _TDropStatus,
    _TDisplayShackle,
    _TIsysLineStatus,
    _TShackleStatus,
    _mbx_state,
    _DigLCSet,
    _lc_sample_adj,
	_TMiscFeatures,
	_TResetGradeSettings
};

// Some conversions for testing with simulator. Just uncomment the desired CNTS define.
// The values are rounded up a little so the calculated integer setpoint doesn't come 
// a few counts shy of a batch. 
//
// Counts/lb = 1580
//    "  /oz = 98.75
//    "  /gm = 3.48
// grams
//#define     CNTS    3.5
// ounces
//#define     CNTS    98.8
// pounds (analog 1510)
//#define     CNTS    1580
// pounds (digital HBM - matches loadcell_config.counts_per_pound)
#define     CNTS    116500

// Debug trace definitions
#define _CHKRNG_      0x00000001    // 1
#define _FDROPS_      0x00000002    // 2
#define _DROPS_       0x00000004    // 4
#define _HOST_COMS_   0x00000008    // 8
#define _LABELS_      0x00000010    // 16
#define _WEIGHT_      0x00000020    // 32
#define _GRADE_       0x00000040    // 64
#define _SYNCS_       0x00000080    // 128
#define _ZEROS_       0x00000100    // 256
#define _TRICKLE_     0x00000200    // 512
#define _IPC_         0x00000400    // 1024
#define _COMBASIC_    0x00000800    // 2048
#define _COMQUEUE_    0x00001000    // 4096
#define _DRPMGRQ_     0x00002000    // 8192
#define _ISCOMS_      0x00004000    // 16384
#define _WTAVG_       0x00008000    // 32768
#define _AUTOZ_       0x00010000    //
#define _HBMLDCELL_   0x00100000    // 1048576
#define	_DEBUG_		  0x00800000
#define _ALWAYS_ON_   0x80000000    // 

#define UDP_ENABLE		0x80000000	// Enable UDP tracing to port (800(line#))

#ifdef __linux__

#ifndef DCHSERVICES_BASE
#define DCHSERVICES_BASE "/home/del/dchservices"
#endif
#define TOTALS_PATH    DCHSERVICES_BASE "/data/totals.dat"
#define LCREADS_PATH   DCHSERVICES_BASE "/data/lcreads"
#define DREC_INFO_PATH DCHSERVICES_BASE "/data/droprecs/drecinfo.dat"
#define DREC_FILE_PATH DCHSERVICES_BASE "/data/droprecs/drprec%d.dat"
#define SYS_FILE_PATH  DCHSERVICES_BASE "/data/settings/system.bin"
#define SCL_FILE_PATH  DCHSERVICES_BASE "/data/settings/scale.bin"
#define TARE_FILE_PATH DCHSERVICES_BASE "/data/settings/tares.bin"
#define SCH_FILE_PATH  DCHSERVICES_BASE "/data/settings/schedule.bin"

#elif !defined(_SIM_LAPTOP_)

#define TOTALS_PATH    "d:\\dchservices\\totals.dat"
#define LCREADS_PATH   "d:\\dchservices\\lcreads"
#define DREC_INFO_PATH "d:\\dchservices\\droprecs\\drecinfo.dat"
#define DREC_FILE_PATH "d:\\dchservices\\droprecs\\drprec%d.dat"
#define SYS_FILE_PATH  "d:\\dchservices\\settings\\system.bin"
#define SCL_FILE_PATH  "d:\\dchservices\\settings\\scale.bin"
#define TARE_FILE_PATH "d:\\dchservices\\settings\\tares.bin"
#define SCH_FILE_PATH  "d:\\dchservices\\settings\\schedule.bin"

#else

#define TOTALS_PATH    "c:\\dchservices\\totals.dat"
#define LCREADS_PATH   "c:\\dchservices\\lcreads"
#define DREC_INFO_PATH "c:\\dchservices\\droprecs\\drecinfo.dat"
#define DREC_FILE_PATH "c:\\dchservices\\droprecs\\drprec%d.dat"
#define SYS_FILE_PATH  "c:\\dchservices\\settings\\system.bin"
#define SCL_FILE_PATH  "c:\\dchservices\\settings\\scale.bin"
#define TARE_FILE_PATH "c:\\dchservices\\settings\\tares.bin"
#define SCH_FILE_PATH  "c:\\dchservices\\settings\\schedule.bin"

#endif


// Macros
