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
#define LOADCELL_TYPE_ISPD	2

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
#define APP_VER2         7  // Minor
#define APP_VER3         16	// Local -- 15.7.16 = Pitman hotfix line: 15.7.15 + single-sensor cross-sync count check (NOT 9da7827)

#define CREATE_VER_STRING(string) \
    sprintf((char *)string, "GS-1000 RTOS Version %d.%d.%d %s ", \
            APP_VER1, \
            APP_VER2, \
            APP_VER3, \
            __DATE__);

// STEP 8: Build for no interface/debug/simulation mode 

//#define _SIMULATION_MODE_ //GLC  -- enables built-in sync/grade simulator (bypasses I/O)
//#define _WEIGHT_SIMULATION_MODE_ //GLC  -- enables simulated weights (skips real load cell) -- CONFLICTS with _HBM_SIM_
//#define _HBM_SIM_      // OBSOLETE - replaced by _LC_SIM_ (runtime selection of HBM or ISPD sim)
//#define _LC_SIM_         // Load cell simulator — routes serial I/O to HBM or ISPD sim based on LoadCellType
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
// 15.7.16: controller -> host message queue (GenError -> SendError). Until 15.7.16 GenError kept
// ONE slot pointing at the caller's buffer, so a second GenError before GpSendThread's next pass
// (<= 50 ms) overwrote the first - two "Count corrected" in one scan reached the host as one, and a
// burst of 8 as 2. GenError now copies sev + text into a ring of ERRQ_LEN entries (power of 2);
// producers never wait (any thread, the 5 ms scan included); on overflow the OLDEST are dropped and
// one "N controller messages dropped (queue full)" follows the survivors to the host.
#define ERRQ_LEN            64
#define MAXTRCIDHDRSIZE     16      // the length of buffer origin string
#define MAXDATETIMESTR      48      // date/time strings
#define MAXPENDANT          2000    // max number of pendants
#define MAXDROPRECS         200     // buffer of birds dropped
#define DROPRECHISP         100     // high water mark, if exceeded save to files
#define MAXSAVERECS         400     // max records per drop record file
#define MINDSAVREC          10      // buffer of birds dropped
#define MAXDSAVFILES        500
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
// Single-sensor zero flag: minimum trolleys between two accepted zero tabs. The real flag
// comes once per chain revolution (hundreds of trolleys); anything closer means the learned
// trolley interval T has gone stale and ordinary trolleys are being misread as tabs.
#define SS_MIN_TROLLEYS_BETWEEN_TABS  10
// Single-sensor zero flag: consecutive UNDERSIZED gaps required before the learned
// trolley interval T is re-seeded DOWNWARD. This is the exact mirror of the existing
// oversized-gap stall re-seed (4 consecutive gaps > 3*T). Without it T can only ever
// GROW: the EMA folds a gap only when it is >= 0.7*T, the stall path only fires above
// 3*T, so once T is latched stale-large NOTHING can bring it back down and the sync
// stops zeroing permanently until the controller is restarted (Pitman 2026-07-26,
// both drop syncs dead for 4+ hours after a line stop/start ramp). A run of 4 is far
// above any isolated short gap (the tab itself is ONE short gap per revolution, and
// noise blips are isolated) and far below any real speed change, which produces
// hundreds of consecutive shorter gaps.
#define SS_SHRINK_RUN                 4
// Saturation for the per-sync "trolleys since the last accepted tab" counter (tabRun) and
// the per-sync zero-flag alarm counter. Also the boot value of tabRun: "no tab seen yet",
// so the first real tab after boot (or before the host has pushed Shackles) is accepted.
#define SS_TAB_RUN_MAX                1000000
// --- 15.7.15 single-sensor TAB RULE (SingleSensorTabRule). Everything is measured on the
// line itself, so no per-plant tuning: G = the most recent trolley-to-trolley gap, G2 = the
// gap before it (accepted tabs never update either). A count edge is the zero TAB only if
//   (a) SS_TAB_BAND_LO_PERMIL*G <= 1000*delta <= SS_TAB_BAND_HI_PERMIL*G
//       (the flag tab sits 0.20..0.65 of the measured trolley gap: covers every tab
//       geometry 0.25..0.50 incl. 0.27 flags with jitter; Pitman ~0.42),
//   (b) SS_GAP_AGREE_LO_PCT*G2 <= 100*G <= SS_GAP_AGREE_HI_PCT*G2
//       (the two previous trolley gaps agree: the line is not accelerating), and
//   (c) tabRun >= Rg, the revolution gate: R = Shackles*(SkipTrollies+1) trolleys per chain
//       revolution, Rg = R - SS_GATE_MARGIN, never below SS_MIN_TROLLEYS_BETWEEN_TABS.
//       Pitman: R=608 -> Rg=600. A false zero can only land in the last few trolleys before
//       the real flag (and the exact-count check in ProcessSyncs/GradeSyncs alarms it).
// BOOT CONFIRMATION (SS_BOOT_CONFIRM): until a sync's detector has accepted its first tab
// since boot, (c) is replaced by "a SECOND tab-shaped edge arrives R+1 +/- SS_GATE_MARGIN
// trolleys after an earlier one" (the first is counted as a trolley, hence +1). Every
// tab-shaped edge is remembered by its trolley position (a bit ring of SS_BOOT_RING
// positions per sync, no capacity limit), so noise before or between the real tabs cannot
// block the confirmation. With noise present the first in-band edge after boot is otherwise
// often a false one. SS_BOOT_RING must exceed R+1+SS_GATE_MARGIN (Shackles <= MAXPENDANT
// 2000 at SkipTrollies <= 3); a longer chain is simply never boot-confirmed. TRADE-OFF: after every controller restart the first zero comes up to
// ONE EXTRA REVOLUTION later (~14 min at turkey speed, ~5 min at chicken speed) and no drops
// fire before the first zero. Set SS_BOOT_CONFIRM 0 to accept the first tab-shaped edge
// after boot instead (the 15.7.14 start-up behaviour); nothing else changes.
// The host-pushed ZeroTabWindowMinMs/MaxMs are no longer consulted (shm + wire unchanged).
#define SS_TAB_BAND_LO_PERMIL         200
#define SS_TAB_BAND_HI_PERMIL         650
#define SS_GAP_AGREE_LO_PCT           70
#define SS_GAP_AGREE_HI_PCT           143
#define SS_GATE_MARGIN                8
#define SS_BOOT_RING                  8192  // power of 2
#ifndef SS_BOOT_CONFIRM
#define SS_BOOT_CONFIRM               0     // 0 = OFF (default: first flag pass zeroes, as before), 1 = wait for a 2nd matching pass
#endif

// --- 15.7.16 single-sensor CROSS-SYNC COUNT CHECK (SingleSensorXC*, ZeroFlagMode == 1 only).
// All count syncs (ProcessSyncs syncs, not grade syncs) sit on ONE chain, so once each has zeroed
// the trolley offset between two of them, D = P_a - P_b, is fixed by the sensor spacing. P is a
// sync's phase-accurate position since its last zero: P = T + frac, T = (shackleno-1)*(SkipTrollies+1)
// + trolly_counters, frac = scan ticks since its last counted trolley edge / G (its measured gap),
// clamped below 1. A miscount on one sync moves its offset to every other sync by a whole trolley.
//
// MEASURED AT PITMAN (scope captures 2026-10-05, offline, exact interpolation): D is NOT a scalar
// constant - it varies with chain position by up to 0.42 trolley peak-to-peak around the
// revolution (uneven pitch between sensors 145..249 trolleys apart), but repeats revolution to
// revolution within sd 0.02 and across stops. So D is SEEDED at clean zeros (the spec) and then
// TRACKED: at each steady edge its FRACTIONAL residual is folded in (gain 1/SS_XC_TRACK_GAIN); the
// integer part is never touched, so a miscount can never be absorbed. Residual in steady running:
// ~0.05 trolley, against a 1.0 trolley miscount.
// Every miscount seen in those captures was +1 (a rollback at a stop re-passes a body: an extra
// edge). Two of the six were TWO sensors at the same stop, where a symmetric 2-of-3 vote picks the
// wrong sync. Hence: only a sync that is AHEAD is ever corrected (never a "lost" count), and a sync
// whose count was anchored by its own zero since its last disturbance is never corrected.
//   learn   : pair state 0 none -> 1 candidate (tracked) -> 2 armed. A sample is taken when a sync's
//             accepted tab passes the exact-count check and the other sync's last zero did too (and
//             it was not corrected since), both steady. The candidate is armed by the NEXT clean
//             sample of either sync agreeing within SS_XC_CONFIRM_TOL_PERMIL. A non-exact zero,
//             an overrun or a correction of either sync discards a candidate; a clean sample whose
//             fractional residual vs an armed D exceeds SS_XC_RELEARN_TOL_PERMIL disarms the pair.
//   steady  : a sync is steady after SS_XC_STEADY_GAPS consecutive trolley gaps each within
//             SS_XC_GAP_LO..HI_PCT of the previous one, while its open gap stays <= SS_XC_OPEN_GAP_PCT
//             % of G. Nothing is checked, learned or tracked unless every sync in the vote is steady.
//             TIGHTER THAN THE IDEA (70..143%, stopped at an open gap > 2 x G), from the data: a
//             sync's phase estimate is stale by up to |1 - speed ratio| x frac, so within 70..143%
//             two syncs can read up to ~0.5 trolley off (synthetic 7<->71 SPM steps), and at
//             Pitman the chain decelerating into a stop left two syncs at 1.5 / 1.9 x G - a pair
//             read 0 where the truth was +1. 99.4% of Pitman's in-band gap ratios are within 15%
//             (all the others are stops / restarts), so +/-15% costs only a trolley after a restart.
//   vote    : the largest set of zeroed syncs whose pairs are all armed. e = P_a - P_b - D, wrapped
//             to +/-R/2; a pair is "integer" when |e - round(e)| < SS_XC_K_TOL_PERMIL.
//   one-step: a disagreement is acted on only if it arose in ONE step from every pair reading 0,
//             and its level pattern has since changed only by our own corrections or by a sync's
//             own zero (its new level is the physical flag's). Anything else - e.g. two lost counts
//             on two syncs at different times, which leaves the one good sync looking "ahead" - is
//             alarm only until every pair reads 0 again. The line-state bookkeeping (all at 0, the
//             pattern) only takes a reading the next steady check confirms: a sync rolled back onto
//             the previous body at the start of a stop counts it at a normal-looking moment, and
//             that one reading is garbage.
//   correct : sync i by -1 trolley when every pair in the vote is integer, the vote has >=
//             SS_XC_MIN_SYNCS syncs, i is not anchored, the disagreement is one-step, the same
//             verdict holds on SS_XC_HYST_EDGES consecutive counted edges of i, at an edge where the
//             corrected count lands on a shackle trolley (tc 0), and EITHER
//             (P1) i is +1 against every other sync and they agree with each other (0) - any time;
//             (P2) after a LINE STOP - every vote sync saw a gap over SS_XC_LINESTOP_PCT % of its last
//                  steady gap - that began with every pair at 0 and saw no zero or overrun: the vote
//                  splits into two levels a whole trolley apart, i is in the upper level and no
//                  upper-level sync is anchored (a stopped chain rolling back can only ADD counts, so
//                  the lower level is right - two sensors re-counting at one stop, as at Pitman).
//             The pass for the count it lands on already ran (one trolley early): ProcessSyncs skips
//             this edge's drop / missed-bird pass; the scale re-weighs (SkipTrollies >= 1) or skips
//             its pass too (SkipTrollies 0, where the early weighment has completed).
//   never   : a sync BEHIND (a lost count, or the overrun reset's -1/-2 after a rejected tab) and a
//             common-mode error (every sync re-counted at one stop) are not corrected; the zeros
//             realign them as before. A 2-sync line alarms only.
//   alarm   : "Count disagreement: <a> vs <b> by <k> trolley" when an integer offset k != 0
//             (|k| <= SS_XC_MAX_K) persists SS_XC_ALARM_EDGES counted edges of <a> without a
//             correction - at most once per revolution per pair. Every correction raises
//             "Count corrected: <sync> -1 trolley (cross-check with <others>)" (never rate-limited).
//   arming  : from boot about 2.4 clean revolutions after the first zero (each pair needs two
//             clean zeros), and only revolutions whose zeros come out exact count.
// Standard mode (ZeroFlagMode != 1) never runs any of this. SS_CROSSCHECK 0 removes it entirely.
// Proof: linux_port/tools/sszero_crosscheck_test.sh (real Pitman captures + synthetic chains).
#ifndef SS_CROSSCHECK
#define SS_CROSSCHECK                 1
#endif
#define SS_XC_K_TOL_PERMIL            300   // |e - k| < 0.30 trolley: e is the integer k
#define SS_XC_AGREE_TOL_PERMIL        350   // |e| < 0.35: two syncs agree (only used through K_TOL < AGREE_TOL)
#define SS_XC_STEADY_GAPS             3     // normal gaps after a disturbance before a sync is steady
#define SS_XC_GAP_LO_PCT              87    // a normal trolley gap: 87..115% of the previous one
#define SS_XC_GAP_HI_PCT              115
#define SS_XC_OPEN_GAP_PCT            115   // open gap > 115% of G: the sync is slowing / stopped
#define SS_XC_HYST_EDGES              2     // same verdict on 2 consecutive counted edges of the sync
#define SS_XC_MIN_SYNCS               3     // syncs in the vote needed to CORRECT (2 = alarm only)
#define SS_XC_CONFIRM_TOL_PERMIL      150   // candidate armed when the next clean sample agrees within 0.15
#define SS_XC_RELEARN_TOL_PERMIL      300   // armed pair disarmed if a clean sample is off by >= 0.30 (fractional)
#define SS_XC_TRACK_TOL_PERMIL        200   // only fractional residuals < 0.20 are tracked
#define SS_XC_TRACK_GAP_PCT           10    // ...and only while both syncs' last two gaps agree within 10%
#define SS_XC_TRACK_GAIN              16    // D += residual / 16 per tracked edge
#define SS_XC_ALARM_EDGES             8     // a disagreement must persist 8 counted edges before the alarm
#define SS_XC_DIST_EVALS              48    // a line-stop window left unresolved for 48 steady checks is closed
#define SS_XC_LINESTOP_PCT            350   // a gap over 3.5 x the sync's last steady gap = the chain stopped
                                            // (a missing body is 2 x, two 3 x; real Pitman stops 4.3..8 x and more)
#define SS_XC_MAX_K                   3     // disagreements larger than this are left to the zero alarms

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
// Spare host-push slots above MAXIDS (do NOT add save groups; > MAXIDS so not
// iterated/saved): the 5 from MAX_GROUPS = ids 94-98 (power-loss 94/95,
// single-sensor 96/97/98), then +1 = id 99 (ScaleSyncOffset), then +4 more =
// ids 100/101/102/103 for Auto Calculate Span (enable / known-weight / clamp / ref-shackle).
#define ALL_SHM_IDS         (MAXIDS + MAX_GROUPS + 5)

// Auto Calculate Span host-pushed settings (spare shmIDs, SandCat only)
#define AUTOSPAN_ENABLE      100   // int  AutoSpanEnable (0/1, per line)
#define AUTOSPAN_KNOWN_WT    101   // __int64 AutoSpanKnownWeight (internal weight units)
#define AUTOSPAN_CLAMP       102   // int  AutoSpanClampPpt (ppt, default 20 = 2%)
#define AUTOSPAN_REFSHK_ID   103   // int  AutoSpanRefShackle (reference shackle no, host-pushed)
#define AUTOSPAN_REF_SHACKLE 2     // default reference shackle = trolley 1 = shackleno 2 (pinned by observation)

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
// 15.7.13 - optional logical-drop -> physical-output-pin map. Absent = identity.
#define OUTMAP_FILE_PATH DCHSERVICES_BASE "/data/settings/output_map.cfg"

#elif !defined(_SIM_LAPTOP_)

#define TOTALS_PATH    "d:\\dchservices\\totals.dat"
#define LCREADS_PATH   "d:\\dchservices\\lcreads"
#define DREC_INFO_PATH "d:\\dchservices\\droprecs\\drecinfo.dat"
#define DREC_FILE_PATH "d:\\dchservices\\droprecs\\drprec%d.dat"
#define SYS_FILE_PATH  "d:\\dchservices\\settings\\system.bin"
#define SCL_FILE_PATH  "d:\\dchservices\\settings\\scale.bin"
#define TARE_FILE_PATH "d:\\dchservices\\settings\\tares.bin"
#define SCH_FILE_PATH  "d:\\dchservices\\settings\\schedule.bin"
#define OUTMAP_FILE_PATH "d:\\dchservices\\settings\\output_map.cfg"

#else

#define TOTALS_PATH    "c:\\dchservices\\totals.dat"
#define LCREADS_PATH   "c:\\dchservices\\lcreads"
#define DREC_INFO_PATH "c:\\dchservices\\droprecs\\drecinfo.dat"
#define DREC_FILE_PATH "c:\\dchservices\\droprecs\\drprec%d.dat"
#define SYS_FILE_PATH  "c:\\dchservices\\settings\\system.bin"
#define SCL_FILE_PATH  "c:\\dchservices\\settings\\scale.bin"
#define TARE_FILE_PATH "c:\\dchservices\\settings\\tares.bin"
#define SCH_FILE_PATH  "c:\\dchservices\\settings\\schedule.bin"
#define OUTMAP_FILE_PATH "c:\\dchservices\\settings\\output_map.cfg"

#endif


// Macros
