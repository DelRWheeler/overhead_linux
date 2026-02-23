//////////////////////////////////////////////////////////////////////
// overhead.h: interface for the overhead class.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_OVERHEAD_H__76E8ADD9_1B02_11D6_B441_020000000001__INCLUDED_)
#define AFX_OVERHEAD_H__76E8ADD9_1B02_11D6_B441_020000000001__INCLUDED_

// TG - VL-EPM-19 (FOX) Codes
#define FPGA_PRODUCT_CODE_OFFSET 0x00
#define VL_EPM19_PRODUCT_CODE 0x12
#define FPGA_BASE_ADDR 0xC800

// TG - Define the I/O port addresses for GPIO registers
#define GPIO_P2_DIRECTION_REGISTER  0x9A
#define GPIO_P2_DATA_REGISTER       0x7A

#pragma once

#include "types.h"

class overhead:app_type
{

public:
	int previousShackle1;
	int previousShackle2;

	int syncOffset; //offset from the scale sync to the scale, normally two

	Serial*				pSerial1;
	Serial*				pSerial2;
	HANDLE *			SerQHandle1;
	HANDLE *			SerQHandle2;
    LoadCell*           ld_cel[MAXLOADCELLS];
    HBMLoadCell*	    HBMLc; //GLC
	HBMLoadCell*	    HBMLc2;
	HBMLoadCell*	    HBMLc3;
	HBMLoadCell*	    HBMLc4;
    dig_lc_settings     LastDigLCSet[MAXLOADCELLS];    // last setting for the digital load cells
    dig_lc_settings     DefaultDigLCSet[MAXLOADCELLS]; // default settings for the digital load cells

    // Inter systems, mostly shortcuts

	bool				bVersionMismatch;
    bool                simulation_mode;
    bool                weight_simulation_mode;
	bool				StuckLoadCellWarning[MAXSCALES];
    int                 this_lineid;
    int                 isys_fastest_idx[MAXDROPS]; // Index for fastest drop for intersystems.
                                                    // If equal to MAXIISYSLINES, then local is
                                                    // fastest
    bool                isys_comm_fail[MAXIISYSLINES+1]; // An extra status for host
    bool                isys_reset_bpm;             // trigger to reset birds / minute
    int                 isys_shksec_sec_cnt;        // current cnt/sec
    int                 isys_shksec_max_cnt;        // peak shk/sec cnt, used for slwdn vars below
    int                 isys_cnt_slwdn[MAXDROPS];   // to prevent dropping too many birds
    __int64             isys_wt_slwdn [MAXDROPS];   // to prevent dropping too many birds

    // Normal variables

    bool                grade_zeroed[MAXGRADESYNCS];
    bool                grade_armed[MAXGRADESYNCS]; // FLAG TO PREVENT GRADING TWICE
    bool                sync_armed[MAXSYNCS];
    bool                dbg_sync_zero_triggered[MAXSYNCS]; // mirror of static local in ProcessSyncs for debug
    bool                dual_scale;
    char                sync_in     [MAXINPUTBYTS];
    char                sync_zero   [MAXINPUTBYTS];
    char                switch_in   [MAXINPUTBYTS];
    char                output_byte [MAXOUTPUTBYTS];
    int                 output_timer[MAXOUTPUTBYTS*8];/*counters count down using time
                                                        base set by clock_resolution and
                                                        clock_ticks.*/
    int                 sync_debounce[MAXSYNCS];
    int                 trolly_counters[MAXSYNCS+1];
    __int64             true_shackle_count[MAXSYNCS+1]; //GLC added 2/14/05 for detailed late zero message
    __int64				true_grade_shackle_count[MAXGRADESYNCS+1]; //GLC added 2/14/05 for detailed late zero message
	int                 weigh_state[MAXSCALES];
    int                 tare_start_shkl;                 // starting shackle for tares
    bool                get_weight_init[MAXSCALES];
    int                 zero_bias_mode;                  // used to change the behavior of auto zero
    int                 shackle_count;
    int                 bpm_qrtr;
    UINT                slave_batch[MAXBCHLABELS];
    int                 bpm_act_count[MAXSCALES];
    bool                trickle_active;
    bool                trickle_flag;
    int                 target_trickle_count[MAXDROPS];
    int                 trickle_short_cnt   [MAXDROPS];  // running total of birds not dropped, max is 10% of target
	int					trickle_shorts_added[MAXDROPS];
    int                 trickle_remainder   [MAXDROPS];  // leftovers
    bool                shm_valid;                       // lookup table for shmem r/w ids has been initialized
    bool                config_Ok;                       // all settings files for shmem were good
    bool                someone_is_connected;            // if can't talk to anyone, do not run

//----- flags to set so the gpSendThread will send messages/save files

    bool                susp_drop_rec_send;              // temporarily stop sending drop records to host
    bool                sendDrpRecs;                     // send drop records
    bool                sendSavedDrpRecs;                // send saved drop records
    bool                sendBlockRequested;              // request to send block sent
    bool                sendBlockGranted;                // request to send block granted by host
    bool                sendGpDebug;                     // send debug info
    bool                sendBpmReset;                    // send reset to remotes
    bool                masterCheck;                     // send comchecks and determine master status
    bool                configGroupCheck;                // request settings file(s) from host
    err_info            send_error;                      // send error message
    bool                sendBatchLabel;                  // send label info for batches
    bool                saveTotals;                      // save totals
    bool                saveDrpRecs;                     // save drop records
    bool                updateDrecInfo;                  // update drop record info

//----- weigh variables

    int                 LastAdcMode    [MAXSCALES];      // If current value != last, set it
    int                 LastNumAdcReads[MAXSCALES];      // If current value != last, set it
    int                 LastOffsetAdc  [MAXSCALES];      // If current value != last, set it
    __int64             individual_wts [ADCREADSMAX];	 //MAX_ADC_READS];
    __int64             individual_lim;
    int                 capture_period;
    t_capture_info      capt_wt;                         // weight capture for fine tuning reads
    bool                calc_shk2shk_ticks;              // flag to start shk2shk timing
    int                 cur_shk2shk_ticks;               // # of ticks between shackles for w_avg
    int                 shk2shk_ticks;                   // # of ticks between shackles for w_avg
    t_wt_avg            w_avg          [MAXSCALES];      // weight averaging variables

//----- section for managing drop records

    bool                last_drop_rec_susp;              // susp_drop_rec_send may be set/cleared several places, this rembers last state
    int                 drp_rec_count;                   // to be compaired to a high water count
    drp_rec_sendQ       drp_rec_resp_q  [MAXDROPRECS];   // keeps track of what drop records were sent
    UINT                sav_beg_seq_num;                 // for these, just remember the first seq_num for later
    fdrec_info          sav_drp_rec_file_info;           // keeps track of drop records while not communicating with host
    ftot_info           totals_info;                     // keeps track of drop records while not communicating with host

    BYTE                to_be_processed[MAX2BPROCESSED]; // place to store data from host to be processed
    bool                shm_updates[ALL_SHM_IDS];        // a flag for any id can be set and an update sent to the host
    TSyncSettings       *pSyncSet;
    TSyncStatus         *pSyncStat;
    TShackleStatus      *pShk_Data;
    const shm_info      shm_tbl[ALL_SHM_IDS];            // Lookup table for shared memory stuff.
                                                         // Include space for groups.
    fsave_grp     fsave_grp_tbl[MAX_GROUPS];       // groups of structs to save in files
    lblQ                batch_label;                     // send queue of batch labels
    SHARE_MEMORY        *pShm;
    TRACE_MEMORY        *pTraceMemory;
    int					WriteLCReadsToFile;

//----- section for 2nd 3724 card
	bool	Second_3724_Required;			//	Do we need a card at all
	int		Second_3724_No_of_Inputs;		//	Highest number input required
	int		Second_3724_No_of_Outputs;		//	Highest number Output
	int		Second_3724_Drops[24];			//	Array of LED outputs vs. Drop they are assigned to
	int		Second_3724_InputRegisters;
	int		Second_3724_DropCounter[24];
	int		Second_3724_Shackle[24];
	int		Second_3724_Scale[24];
	bool	Second_3724_WriteOutput;
	bool	Second_3724_Input[24];
	bool	Second_3724_Output[24];
	bool	Second_3724_DropDisabled[24];

	bool	BoardIsEPM15;

	// TG - Added VL-EPM-19 (FOX) Support
	bool	BoardIsEPM19;

	bool	last_in_batch_dropped[MAXDROPS];
private:

    void    AddBird(int grade_idx, int drop_no, __int64 wt);
    void    AddDropRecord(int shackle);
    void    AddMissedBird(int grade_idx, int drop_no, __int64 wt);
	void	AnalyzeConfig(bool Initialize);
    bool    AssignDrop(int scale, int drop, TShackleStatus* pShk );
    void    AutoTare(int s);
    void    AutoZero(int s);
    void    AverageWeight(int scale);
    void    BpmStats();
    void    CaptureLcData();
    void    Check_Adc_Settings(int scale);
    void    CheckConfig();
    int     CheckRange(int drop, int scale, TShackleStatus* pShk);
    int     CreateAppMailbox();
    int     CreateAppMbxThread();
    int     CreateMbxEventHandler();
    int     CreateDMgrMailbox();
    HANDLE  CreateGpSendThread();
    int     CreateIsysMailbox();
    HANDLE  Create_Gp_Timer();
    HANDLE  Create_App_Timer();
    HANDLE  Create_Fast_Update();
    HANDLE  CreateDebugThread();
    void    DecCntrlCtrs();
    void    FindDrops(int Scale, int StartAt = 0, int Shackle = 0);
    void    GpDebug();
    void    GradeProcess(int GradeSyncIndex);
    void    GradeSyncs();
    void    InitCommunications();
    void    InitDebug();
    void    InitIO();
    void    InitLocals();
    HANDLE  InitMem();
	HANDLE	InitTraceMem();
    void    InitMiscThreads();
    void    InitShmTbl();
    int     InitTrace();
    int     MasterCtrl();
    bool    MissedBirdCheck(int curr_mb, int shackle, int mb_bit_state);
    void    ProcessMbxMsg();
    void    ProcessSyncs();
    void    ProcessWeight();
    void    BatchResetStation();
	void    RawMode();
    void    ReadConfiguration();
    void    RemoveDropRecords(int cnt, UINT* seqnums);
    void    RemoveSavedDropRecords();
    void    RequestToSendDropRecordBlock();
    int     RingSub(int shakle_no, int offset, int total_shakles);
    int     SearchLoop(int scale, int start_drop);
    int     SearchLoopUtility(int scale, int cmd, int index);
    void    SendDropRecords();
    //int     SendDrpManager(int cmd, int drop, int var1, int var2, int var3, int var4);
    void    SendError();
    int     SendHostMsg( int cmd, int var, BYTE *data, int len);
    int     SendIsysMsg( int cmd, int lineId, BYTE *data, int len);
    void    SendLabelInfo();
    int     SendLineMsg( int cmd, int line, int var, BYTE *data, int len);
    void    SendPreLabel();
    void    SendSlavePreLabel();
    void    SetDefaults();
    void    SetOutput(int num, DBOOL active);
    void    SubtractBird(int grade_idx, int drop_no, __int64 wt);
    void    TrickleCounts();
    void    UpdateCapture(int buf);
    void    UpdateHost();
    void    Validate();
    void    WriteDropRecords(int cnt);
    __int64 ZeroAverage(int scale);

public:
	// TG - VL-EPM-19 Checking Functions
	unsigned char ReadFPGARegister(unsigned short base_addr, unsigned char offset);
	void CheckIfEPM19();

	void InitLoadCell();

            overhead();
    virtual ~overhead();

    void    ClearOutputs();
    void    GenError(int sev, char* txt);
    void    initialize();
    void    SaveDropRecords(bool save_all);
    void    SendSavedDropRecords();
    void	SetIsysFastestIndices(int DropIdx, bool Default);
    void    SaveTotals();
    void    TimePPM();
	HANDLE	InitDebugShell();
	int     SendDrpManager(int cmd, int drop, int var1, int var2, int var3, int var4);


    static void __stdcall DebugThread(PVOID unused);
    static void __stdcall Gp_Timer_Main(PVOID addr);
    static void __stdcall GpSendThread(PVOID addr);
    static void __stdcall App_Timer_Main(PVOID addr);
    static void __stdcall Fast_Update(PVOID addr);
    static int  __stdcall Mbx_Server(PVOID unused);
    static int  __stdcall MbxEventHandler(PVOID unused);
    static LONG __stdcall appMbxUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    static LONG __stdcall appMbxEvUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    static LONG __stdcall appUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    static LONG __stdcall appGpUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    static LONG __stdcall appDebugUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);

	//  Implicit declarations of copy constructor
	//   and assignment operator.
	overhead( const overhead& );
	overhead& operator=( const overhead& );

};

#endif // !defined(AFX_OVERHEAD_H__76E8ADD9_1B02_11D6_B441_020000000001__INCLUDED_)
