#pragma once

//---------------------------------------------------
// local data types
//---------------------------------------------------
#include "types.h"

class app_type  
{
public:

//----- Not part of shared memory

typedef struct
{
    int     measure_mode;
    int     ASF;
    int     FMD;
    int     ICR;
    int     CWT;
    int     LDW;
    int     LWT;
    int     NOV;
    int     RSN;
    int     MTD;
    int     LIC0;
    int     LIC1;
    int     LIC2;
    int     LIC3;
    int     ZTR;
    int     ZSE;
    int     TRC1;
    int     TRC2;
    int     TRC3;
    int     TRC4;
    int     TRC5;
} dig_lc_settings; // settings for digital load cells

typedef struct
{
    int start;
    int end;
} lc_sample_adj; // used to adjust load cell sampling

typedef struct
{
    //bool   hung;
    byte   hung; //GLC
    int    hung_cnt;
} mbx_state;               // application mailbox state for interface to restart app

typedef struct
{
    bool   send;
    int    sev;
    char*  err_addr;
} err_info;                // request settings file(s) from host

typedef struct
{
    int     label_step;
    int     pre_label_step;
    UINT    seq_num;
    tm      time;
    int     drop;
    int     cnt;
    __int64 wt;
} print_info;               // data to send host for batch labels

typedef struct
{
    print_info info[MAXBCHLABELS];
} lblQ;

typedef struct 
{
    int   id; 
    int   data_type; 
    int   struct_len;
    int   num_members;
    void* data;
    char  desc[25];
    int   group;
} shm_info;

typedef struct 
{
    int   id; 
    bool  changed;      // the structure has changed
    bool  loaded;       // the settings were loaded/set
    char  fname[64];    // file/path
    int   struct_size;
    void* struct_ptr;
    char  desc[25];
} fsave_grp;

// If no communications to the host, the line keeps running and 
// saves the drop records in files until the host recovers. This
// structure will be stored in a file called drecinfo.dat. When the 
// record count reaches MAXDRECSAVCNT, cur_file will be incremented 
// and a new file created for the next MAXDRECSAVCNT records. If  
// cur_file number will wrap to 0 and if the file alredy existed, 
// the data will be lost.
typedef struct 
{
    UINT  cur_file;       // a number (0-MAXDSAVFILES) to append to filename (drprec?.dat)
    UINT  nxt2_send;      // if recovering, use this as a starting point for sending to host
    UINT  cur_cnt;        // the number of records in the current file MAXDRECSAVCNT max.
    UINT  nxt_seqnum;     // save the next seq num, to resume where we left off.
    UINT  nxt_lbl_seqnum; // save the next label seq num, to resume where we left off.
} fdrec_info;

// For capturing weights and the index to where the data was read.
typedef struct 
{
    int     scale;        // how to capture or not
    int     mode;         // how to capture or not
    int     curr_buf;
    int     curr_index;
    int     capture;      // for product capture
    int     send_capture   [MAXCAPTUREBUFFS];
    int     send_length    [MAXCAPTUREBUFFS];
    __int64 capture_weights[MAXCAPTUREBUFFS][SAMPLE_WEIGHTS];
} t_capture_info;

// For averaging captured weights and the index to where the data was read.
typedef struct 
{
    bool    capt_hold;          // while averaging, no need to capture, capture data updated by avg routine
    bool    valid_wt;           // indicates ok to use
	char	for_alignment[2];	
    __int64 weight;             // computed average
    int     step;               // current average step
    int     avg_trigger_cntr;   // triggers start/end to average
    __int64 last_wt;            // previous weight read
    int     mrk1_idx;           // start marker
    __int64 wt1;                // weight at start marker
    int     cap1_idx;           // if product capture is on, where average started.
    int     mrk2_idx;           // end marker
    __int64 wt2;                // weight at end marker
    int     cap2_idx;           // if product capture is on, where average ended.
    __int64 avg;                // the final average
    int     curr_index;         // curret buffer index
    __int64 weights[AVG_WEIGHTS];
} t_wt_avg;

//----- Shared memory

typedef struct 
{
   __int64 tare[MAXSCALES];
}  TShackleTares;

typedef struct 
{
     byte   GradeIndex    [MAXSCALES];
	 byte   spare         [2];
     int    dropped       [MAXSCALES];
     int    drop          [MAXSCALES];
     __int64 weight       [MAXSCALES];
     bool   last_in_batch [MAXSCALES];
     UINT   batch_number  [MAXSCALES];
}  TShackleStatus;

typedef struct 
{
     int GradeIndex [MAXSCALES];
     int shackle    [MAXSCALES];
     int drop       [MAXSCALES];
     __int64 weight [MAXSCALES];
}  TDisplayShackle;

typedef struct 
{
    int          first;     // first drop assigned to sync
    int          last;      // last drop assigned to sync
} TSyncSettings;

typedef struct 
{
    int          shackleno;
    DBOOL        zeroed;
} TSyncStatus;

typedef struct 
{
    __int64     BWeight;
    __int64     BCount;
    __int64     GrdCount [MAXGRADES];
    __int64     GrdWeight[MAXGRADES];
} TDropStats;

// This struct makes saving the totals to a file easier. Make sure the structure 
// is kept the same as the sys_out members in the comments to the right.
typedef struct 
{
    __int64     TCnt;                 // TotalCount all drops
    __int64     TWt;                  // TotalWeight all drops
    TDropStats  MdrpInfo[MAXDROPS+1]; // MissedDropInfo[MAXDROPS+1] grand total (all grades & all drops) and individual grade totals (all drops)
    TDropStats  DrpInfo[MAXDROPS];    // DropInfo[MAXDROPS] total per drop and individual grade totals per drop
} ftot_info;

typedef struct 
{
    DBOOL  Active;            // manually controlled disable
    DBOOL  Extgrade;          // flag to activate ext grade 
    char   spare[2];          // for 4 byte alignment
    int    IsysBchSlowdown;   // a number to be subtracted from the BchCount
    int    IsysBchWtSlowdown; // a number to be subtracted from the BchWtSp where the
    int    IsysBPMSlowdown;   // slower intersystems drops should stop
    int    Sync;
    int    Offset;
	int    DropMode;          // Operational mode of the drop
	int    SwitchLEDIndex;    // Switch or Switch/LED index for Reassignment or Remote Batch Reset Modes
} TDropSettings;

typedef struct 
{
    __int64 BWeight;
    __int64 BCount;
    UINT    batch_number;
    bool    Batched;
    bool    Suspended;
    bool    last_in_batch;
} TDropStatus;

typedef struct 
{
    DBOOL  active      [MAXIISYSLINES];       // manually controlled disable (drop)
    int    drop        [MAXIISYSLINES];       // drop number on the other line

} TIsysDropSettings;

typedef struct 
{
    int     flags       [MAXIISYSLINES];       // miscellaneous flags
    int     PPMCount    [MAXIISYSLINES];       // counter to determine bpm
    int     PPMPrevious [MAXIISYSLINES];       // previous bpm count
    __int64 BCount      [MAXIISYSLINES];
    __int64 BWeight     [MAXIISYSLINES];
    UINT    batch_number[MAXIISYSLINES];
} TIsysDropStatus;

typedef struct 
{
    char    Extgrade [MAXGRADES+3]; // the +3 is for 4 byte alignment
    char    Grade    [MAXGRADES+3]; // the +3 is for 4 byte alignment
    int     DistMode;
    int     BpmMode;
    int     LoopOrder;
    int     NextEntry;      // alternate drop
    __int64 MinWeight;
    __int64 MaxWeight;
    __int64 ExtMinWeight;
    __int64 ExtMaxWeight;
    __int64 Ext2MinWeight;
    __int64 Ext2MaxWeight;
    int     Shrinkage;
    int     BchCntSp;       // batch count setpoint
    __int64 BchWtSp;
    int     BPMSetpoint;    // birds per minute setpoint
} TScheduleSettings;

typedef struct
{ 
    int     BchActCnt;      // current vat count
    __int64 BchActWt;
    int     PPMCount;       // counter to determine bpm
    int     PPMPrevious;    // previous counter
    int     spare;
    int     BPMTrickleCount;// actual birds 1/20 per minute
    DBOOL   BPM_1stQ_ok;    // flags to increase/decrease
    DBOOL   BPM_2ndQ_ok;    // birds, depending on the
    DBOOL   BPM_3rdQ_ok;    // count and minute window
    DBOOL   BPM_4thQ_ok;
} TScheduleStatus;

typedef struct 
{
	// Add gradesync number int rcb - 8-12
    char  grade;
    char  spare[3];			// for 4 byte alignment
    int   offset;			// grade sync to sensor 1,2,3..
	int	  GradeSyncUsed;	// Can use two grade syncs for grade zones 
							// more than 80 shackles from sync
} TGradeSettings;

typedef struct 
{
    int Unload;   // wkr not necessary to be struct
} TOrder;

typedef struct 
{
    tm      time;
    int     flags;        // bit0 - data available
    int     dropped [MAXSCALES];
    int     shackle;
    __int64 weight  [MAXSCALES];
    int     drop    [MAXSCALES];
    char    grade   [MAXSCALES];
    char    last_in_batch [MAXSCALES];
//    char    spare;        // 4 byte alignment   // remove spare from both rtss and exe files
    UINT    batch_number  [MAXSCALES];
} THostDropRec;

typedef struct
{
    int      send_tries;  // The number of send attempts
    UINT     seq_num;     // Host will ack with this number
    THostDropRec d_rec;
} drp_rec_sendQ;

typedef struct  // used in the DebugThread
{
    int  id;
    int  index;
} TDebugId;

typedef struct 
{
    int       id_count;
    TDebugId  ids[MAXDBGIDS];
} TDebugRec;

typedef struct 
{
    DBOOL  isys_enable;                       // enable = 1
    DBOOL  isys_master;                       // enable = 1
    char   spare[2];                          // for 4 byte alignment
    int    this_line_id;                      // 1 to Max Isys lines 
    DBOOL  active      [MAXIISYSLINES];       // manually controlled disable (line)
    DBOOL  reset_totals[MAXIISYSLINES];       // indication to reset totals on startup
    int    line_ids    [MAXIISYSLINES];       // 1 to Max Isys lines
    // add an extra place for host name
    char   pipe_path   [MAXIISYSLINES+1][MAXIISYSNAME]; // node name for each line
} TIsysLineSettings;

typedef struct 
{
    int    pkts_sent;
    int    pkts_recvd;
    int    pps_sent;
    int    pps_recvd;
    UINT   app_threads;                       // bits to show ready
    int    flags       [MAXIISYSLINES];       // miscellaneous flags
    // add an extra place for host status
    int    connected   [MAXIISYSLINES+1];     // connection status from interface server

} TIsysLineStatus;


typedef struct 
{
    DBOOL	EnableGradeSync2;
    DBOOL	EnableBoazMode;
    DBOOL   RequireZeroBeforeSync;
	char    spare;
}TMiscFeatures;

//DRW Modifiec from spare to ResetS1Empty for resetting grade on empty shackles only 06/03/2015
typedef	struct
{
	DBOOL	ResetGrade;
	char	ResetScale1Grade;
	char	ResetS1Empty;
}TResetGradeSettings;


// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// The commented numbers are used for debugging, allways keep
// them numbered correctly. IF ANYTHING CHANGES IN SHARED 
// MEMORY, CHANGE THE TABLE IN DebugThread ROUTINE!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
typedef struct
{
/*001*/        int                  Shackles;                      // total number of shackles
/*002*/        int                  TareTimes;                     // current count (tare loop), adding shackle weight to get an average tare for shackle
/*003*/        int                  GibMin;                        // must be a bird with gib
/*004*/        __int64              MinBird;                       // must be a real bird
/*005*/        int                  LastDrop;                      // last drop on scale 1
/*006*/        int                  NumDrops;                      // total number of drops
/*007*/        int                  GibDrop;                       // drop for birds with gibs
/*008*/        int                  DropOn;                        // output on time (depends on main loop timer)
/*009*/        int                  SyncOn;                        // sync debounce
/*010*/        int                  SkipTrollies;                  // number of trollies to skip
/*011*/        int                  MissedBirdEnable[MAXSCALES];   // missed bird option
/*012*/        int                  MBSync[MAXSCALES];             // missed bird sync
/*013*/        int                  MBOffset[MAXSCALES];           // missed bird offset
/*014*/        TSyncSettings        SyncSettings[MAXSYNCS];        // which drops assigned to sync, current shackle etc.
/*015*/        TOrder               Order[MAXDROPS];               // desired sequence for drops (unless processing a loop)
/*016*/        TDropSettings        DropSettings[MAXDROPS];        // more useful info for drops
/*017*/        DBOOL                Grading;                       // switch to turn grading on/off
/*018*/		   TResetGradeSettings  ResetGrading;				   // Reset grading to the default grading set by frontend // Adeed
/*018          DBOOL                ResetGrade;                    // lets birds circle around grade zones without resetting grade, if false.
               char                 spare[2];                      // for 4 byte alignment  */
/*019*/        TGradeSettings       GradeArea[MAXGRADES];          // the grade assigned to each area
/*020*/        TIsysDropSettings    IsysDropSettings[MAXDROPS];    // settings for intersystem drops
/*021*/        TIsysLineSettings    IsysLineSettings;              // node name, id, etc. for intersystems
/*022*/        TMiscFeatures		MiscFeatures;				   // Struct used for misc features
/*022 continued*/ //DBOOL			EnableGradeSync2;              // Enable a 2 grade sync system
/*022 continued*/ //DBOOL			EnableBoazMode;                // Enable Boaz mode
/*022 continued*/ //DBOOL			RequireZeroBeforeSync;         // Added for Carolina Turkeys to help with line backing up after stop
																   // A zero pulse is required before each sync pulse in this mode
/*022 continued*/ //char			spare;					       // Use as bit-map for future
																   // features that are on/off
																   // type settings
/*023*/        int                  OverRideDelay;                 // Delay in seconds for override
																   // of stalled line with ISYS loop
/*024*/        int                  ReassignTimerMax;              // # of 5 ms cycles to wait for input from Cryovac
} sys_in;

typedef struct
{
/*025*/        int                  NumScales;                     // self explanatory
/*026*/        int                  ZeroNumber;                    // on interface as zero revolutions
/*027*/        int                  AutoBiasLimit;                 // like a deadband for auto bias 
/*028*/        int                  NumAdcReads[MAXLOADCELLS];     // filter count
/*029*/        int                  AdcMode[MAXLOADCELLS];         // individual/average (individual bombs 1510, forced to auto)
/*030*/        int                  OffsetAdc[MAXLOADCELLS];       // offset applied to the loadcell from interface
/*031*/        __int64              Spare[MAXLOADCELLS];           // Limit before changing the auto_bias
/*032*/        __int64              SpanBias[MAXLOADCELLS];        // span bias calculated, from interface
/*033*/        int                  LoadCellType;                  // type of load cell (analog, digital xxxx, digital yyyy)
/*034*/        dig_lc_settings      DigLCSet[MAXLOADCELLS];        // Setting for the digital load cells
/*035*/        lc_sample_adj        LC_Sample_Adj[MAXSCALES];      // Used for adjustment of load cell sampling time
/*036*/        int                  Pad01ScaleIn;                  // spare
/*037*/        int                  Pad02ScaleIn;                  // spare
/*038*/        int                  Pad03ScaleIn;                  // spare
} scl_in;

typedef struct
{
/*039*/        TDebugRec            debug_id;                      // for debugging shared memory, see id (left in comment)
/*040*/        int                  dbg_trace;                     // turns on debug tracing (bitmask)
/*041*/        int                  dbg_output;                    // for test firing outputs
/*042*/        int                  dbg_extra;                     // extra info for dbg_trace, enter drop
/*043*/        int                  TestFireDrop;                  // used to help adjust drop timing
/*044*/        int                  UdpTrace;                      // Enable UDP tracing
/*045*/        int                  Pad01DebugIn;                  // spare
/*046*/        int                  Pad02DebugIn;                  // spare
/*047*/        int                  Pad03DebugIn;                  // spare
} dbg_in;

typedef struct
{
/*048*/        int                  PPMTimer;                      // timer for anything per minute
/*049*/        __int64              raw_weight;                    // weight for interface span bias screen
/*050*/        int                  ShacklesPerMinute;             // self explanatory
/*051*/        int                  DropsPerMinute[MAXSCALES];     // self explanatory
/*052*/        TDisplayShackle      DispShackle;                   // info to display for scales
/*053*/        __int64              TotalCount;                    // total count all drops
/*054*/        __int64              TotalWeight;                   // total weight all drops
/*055*/        char                 adc_part[ADCINFOMAX];          // scale part number info
/*056*/        char                 adc_version[ADCINFOMAX];       // scale version info
/*057*/        TDropStats           MissedDropInfo[MAXDROPS+1];    // grand total (all grades & all drops) and individual grade totals (all drops)
/*058*/        TDropStats           DropInfo[MAXDROPS];            // total per drop and individual grade totals per drop
/*059*/        __int64              GradeCount[MAXGRADES];         // total per grade area
/*060*/        TScheduleStatus      ScheduleStatus[MAXDROPS];      // operating counts/weights for schedule
/*061*/        TDropStatus          DropStatus[MAXDROPS];          // more useful info for drops
/*062*/        UINT                 dbg_sync[2][MAXSYNCS+1];       // counters 0=sync_on check 1=zero check
/*063*/        char                 dbg_zero[MAXINPUTBYTS];        // for viewing zero input signals
/*064*/        char                 dbg_switch[MAXINPUTBYTS];      // for viewing switch input signals
} sys_out;

// Structure for saving to battery backed ram

typedef struct
{
  ULONG             drec_cs;
  fdrec_info        drec;
  ULONG             tots_cs;
  ftot_info         tots;
  ULONG             sset_cs;
  sys_in            sset;
  ULONG             scl_set_cs;
  scl_in            scl_set;
  ULONG             tare_cs;
  TShackleTares     tares[MAXPENDANT];
  ULONG             sch_cs;
  TScheduleSettings sched[MAXDROPS];
} bb_sram;

typedef struct
{

// Settings from host

/*065*/        int                  AppFlags;                      // misc flags
/*066*/        TShackleTares        ShackleTares[MAXPENDANT];      // self explanatory
/*067*/        TScheduleSettings    Schedule[MAXDROPS];            // drop setup
/*068*/        int                  AutoTareStep;                  // basically, how many loops in the tare process
/*069*/        int                  OpMode;                        // mode for backend, start, tare, run, etc.
/*070*/        int                  raw_scale;                     // scale for interface span bias screen
/*084*/        sys_in               sys_set;                       // system settings
/*085*/        scl_in               scl_set;                       // scale settings
               dbg_in               dbg_set;                       // debug settings

// Updated by application and sent to host

               sys_out              sys_stat;                      // system status 

// Internal, no concern to host

/*071*/        int                  ZeroCounter[MAXSCALES];        // wkr, don't need here, make private overhead class
/*072*/        __int64              ZeroAverage[MAXAVGCNT][MAXSCALES]; // zero weight sample 
/*073*/        THostDropRec         HostDropRec[MAXDROPRECS];      // drop records for Delphi
/*074*/        TIsysDropStatus      IsysDropStatus[MAXDROPS];      // settings for intersystem drops
/*075*/        int                  IfServerHeartbeat;             // counter, indicates interface is alive
/*076*/        int                  AppHeartbeat;                  // counter,indicates backend is alive
/*077*/        TIsysLineStatus      IsysLineStatus;                // current status of intersystems lines
/*078*/        TSyncStatus          SyncStatus[MAXSYNCS];          // which drops assigned to sync, current shackle etc.
/*079*/        TShackleStatus       ShackleStatus[MAXPENDANT];     // self explanatory
/*080*/        DBOOL                WeighZero[MAXSCALES];          // zeroed status of scale, set when zero flag passes
               char                 spare[2];                      // for 4 byte alignment
/*081*/        int                  spare_int;                     // nothing
/*082*/        int                  WeighShackle[MAXSCALES];       // shackle number just weighed
/*083*/        int                  grade_shackle[MAXGRADESYNCS];	// shackle number at grade detect
// Some group Ids (84-89) already defined. The stuff below was added later.
/*090*/        __int64              AutoBias[MAXSCALES];           // Limit before changing the auto_bias
// Some shared memory is not in the lookup table and does not have an id (0XX)
/*091*/        mbx_state            mbx_status[MBX_STATUSES];      // mailbox status
/*092*/        char                 app_ver[MAXVERINFO];        // version information for this application
/*093*/        char                 comm_ver[MAXVERINFO];       // version information for communications
} SHARE_MEMORY;

typedef struct
{
	int TraceId;
	char TraceString[MAXERRMBUFSIZE];
} UdpTrace;

typedef struct
{
	int LatestEntry;
	UdpTrace UdpTraceData[MAXTRCMBUFSIZE];
}TRACE_MEMORY;

};
