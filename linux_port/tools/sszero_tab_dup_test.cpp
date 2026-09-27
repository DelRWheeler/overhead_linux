//--------------------------------------------------------------------------
//  sszero_tab_dup_test.cpp - single-sensor zero tab duplicate-pass test (15.7.14)
//
//  Build + run:   linux_port/tools/sszero_tab_dup_test.sh
//  (the script cuts the REAL ProcessSyncs / GradeSyncs / SingleSensorIsZeroTab /
//   SingleSensorWarnOk / RingSub out of overhead.cpp - 15.7.13 as *_OLD, the
//   working tree as *_NEW - and compiles this file against the controller headers)
//
//  THE DEFECT (rig, 2026-09-26). In single-sensor zero mode (ZeroFlagMode==1)
//  the flag trolley gives the count sensor a double pulse. The BODY pulse is
//  counted (Shackles -> Shackles+1, an overshoot ProcessSyncs tolerates) and
//  runs the sync's per-shackle pass; the TAB pulse ~0.4 trolley later resets
//  the counter to 1 and runs the pass AGAIN. RingSub(1,x)==RingSub(Shackles+1,x)
//  so the second pass hits the same shackles: second drop kick, second
//  MissedBirdCheck + AddDropRecord (shackle 1172 on the rig), second
//  GradeProcess on the grade syncs, an extra isys shackles/sec count.
//
//  WHAT THIS PROVES. Both versions are fed identical 5 ms scan streams of
//  body / tab / zero-bit pulses (the detector is the real one). Per scenario:
//    A. counters are realigned IDENTICALLY: per-scan hash of every counter,
//       zeroed flag, true count, trolley counter, detector timebase, weigh
//       relabel (WeighShackle), capture re-arm state and every GenError text;
//    B. NEW's per-shackle work is OLD's minus duplicates only: every event
//       OLD has and NEW lacks sits on an accepted single-sensor tab edge and
//       repeats an event OLD already produced for the same shackle since that
//       sync's counter last moved; nothing else differs;
//    C. in steady single-sensor running NEW processes every shackle exactly
//       once per revolution per pass (drop fire, missed-bird check, drop
//       record, GradeProcess) and counts Shackles trolleys/rev for isys, where
//       OLD processes Shackles+1-offset twice (the rig's 1172);
//    D. standard two-sensor mode: OLD and NEW are event-for-event identical.
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>

// Give the class two extra member declarations (the renamed OLD / NEW copies)
// and open its private section to the harness. Test-only preprocessor tricks;
// the controller headers are used unmodified.
#define ProcessSyncs ProcessSyncs(); void ProcessSyncs_OLD(); void ProcessSyncs_NEW
#define GradeSyncs   GradeSyncs();   void GradeSyncs_OLD();   void GradeSyncs_NEW
#define private public
#include "types.h"
#undef private
#undef ProcessSyncs
#undef GradeSyncs

//----- Globals the extracted functions use (defined in overhead.cpp normally)
overhead*   app = 0;
UINT        TraceMask = _ZEROS_;
trcbuf      trc_buf    [MAXTRCBUFFERS];
tmptrcbuf   tmp_trc_buf[MAXTRCBUFFERS];
trace_ctrl  trc        [MAXTRCBUFFERS];
char        app_err_buf[MAXERRMBUFSIZE];
char        sync_desc[MAXSYNCS][MAX_DBG_DESC] = {
    "Scale 1", "Scale 2", "Drop Sync 1", "Drop Sync 2", "Drop Sync 3",
    "Drop Sync 4", "Drop Sync 5", "Drop Sync 6" };
volatile sig_atomic_t g_outputs_disabled = 0;
const BYTE  Mask[8] = {0x01,0x2,0x4,0x8,0x10,0x20,0x40,0x80};    // overhead.cpp:418 (BITSET)
int  RtPrintf(const char*, ...) { return 0; }
void DebugTrace(UINT, char*, ...) {}

#define _FILE_ "overhead.cpp"
typedef app_type::SHARE_MEMORY SHARE_MEMORY;

// The missed-bird eye is read straight from the 3724 port; MissedBirdCheck is
// stubbed below, so any value will do - just never touch real I/O.
#define RtReadPortUchar(a) ((UCHAR) 0)

//----- Event capture ------------------------------------------------------
// type: F drop fire (SetOutput on), M MissedBirdCheck, R AddDropRecord,
//       G GradeProcess, I isys shackles/sec count
struct Ev
{
    long tick; char type; int idx; int sub; int shk;
    bool operator<(const Ev& o) const
    { return std::tie(tick, type, idx, sub, shk) < std::tie(o.tick, o.type, o.idx, o.sub, o.shk); }
    bool operator==(const Ev& o) const
    { return tick == o.tick && type == o.type && idx == o.idx && sub == o.sub && shk == o.shk; }
};
static std::vector<Ev>*          g_ev   = 0;
static std::vector<std::string>* g_msgs = 0;
static long                      g_tick = 0;

// SetOutput only gets the drop number. Every drop belongs to exactly one sync,
// so recompute the shackle exactly as the drop loop did (the counter has not
// moved since).
void overhead::SetOutput(int num, DBOOL active)
{
    if (!active) return;
    for (int s = 0; s < 8; s++)
        if (pShm->sys_set.SyncSettings[s].first > 0 &&
            num >= pShm->sys_set.SyncSettings[s].first && num <= pShm->sys_set.SyncSettings[s].last)
        {
            int shk = RingSub(pShm->SyncStatus[s].shackleno,
                              pShm->sys_set.DropSettings[num-1].Offset + pShm->ScaleSyncOffset,
                              pShm->sys_set.Shackles);
            g_ev->push_back(Ev{g_tick, 'F', s, num, shk});
            return;
        }
    g_ev->push_back(Ev{g_tick, 'F', -1, num, -1});
}
bool overhead::MissedBirdCheck(int curr_mb, int shackle, int)
{
    int s = pShm->sys_set.MBSync[curr_mb] + 1;
    g_ev->push_back(Ev{g_tick, 'M', s, curr_mb, shackle});
    return true;                                   // -> AddDropRecord(shackle)
}
void overhead::AddDropRecord(int shackle)
{
    // attribute to the sync whose MB pass is running (last M event)
    int s = g_ev->empty() ? -1 : g_ev->back().idx;
    g_ev->push_back(Ev{g_tick, 'R', s, 0, shackle});
}
void overhead::GenError(int, char* txt)             { g_msgs->push_back(txt); }
void overhead::GradeProcess(int g)
{
    // One representative grade area (offset 1) - RingSub(Shackles+1,1)==RingSub(1,1)
    g_ev->push_back(Ev{g_tick, 'G', g, 0,
                       RingSub(pShm->grade_shackle[g], 1, pShm->sys_set.Shackles)});
}
int overhead::SendLineMsg(int, int, int, BYTE*, int) { return 0; }

#include "sszero_shared.inc"     // RingSub, SingleSensorIsZeroTab, SingleSensorWarnOk (real)
#include "sszero_old.inc"        // 15.7.13 ProcessSyncs_OLD / GradeSyncs_OLD (real)
#include "sszero_new.inc"        // working tree ProcessSyncs_NEW / GradeSyncs_NEW (real)

//----- Checks -------------------------------------------------------------
static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

//----- Scenario -----------------------------------------------------------
struct Scen
{
    const char* name;
    int  mode;          // ZeroFlagMode: 0 standard two-sensor, 1 single-sensor
    int  N;             // sys_set.Shackles
    int  skip;          // SkipTrollies
    int  extra;         // chain = N*(skip+1)+extra trolleys (extra!=0 = odd-parity turkey chain)
    bool grading, gs2, dual;
    int  T, W, G, W2;   // trolley interval, body width, tab lead, tab width (5 ms ticks)
    int  revs;
    bool clean;         // every tab accepted, no perturbation -> exactly-once checks
    int  missTabRev;    // omit the tab in this revolution (-1 none)
    int  lateTabRev, lateG;       // this revolution's tab arrives at lateG (out of window)
    int  noiseRev, noiseTrolley;  // extra tab-like pulse behind trolley noiseTrolley (early zero)
    int  rawBodyRev;    // OpMode=Raw across the flag body pulse of drop sync 1, Run again for its tab
};

// Sensor layout (phase in ticks). Grade sync first so it zeroes before the scale.
enum { S_SC1 = 0, S_SC2 = 1, S_DS1 = 2, S_DS2 = 3, S_G1 = 8, S_G2 = 9, NSENS = 10 };

struct Result
{
    std::vector<Ev>          ev;
    std::vector<std::string> msgs;
    std::vector<unsigned long long> hash;   // per-scan state hash
    std::map<int, std::vector<long> > zeroTicks;   // accepted zero edges per sync (grade: 8+g)
    std::map<int, std::vector<long> > edgeTicks;   // confirmed edges per sync (grade: 8+g)
    std::map<int, std::vector<int> >  edgeCtr;     // counter value after each of those edges
    std::map<int, std::vector<bool> > edgeRan;     // did that edge's per-shackle pass run (by state)
    std::vector<long>                 rawTicks;    // scans run with OpMode Raw
    std::vector<std::pair<long,int> > weigh;        // (tick, WeighShackle[0]) at each weigh trigger
    int dupTrace;                                   // "SS tab dup ... skipped" trace lines
};

static unsigned long long fnv(unsigned long long h, long long v)
{
    for (int b = 0; b < 8; b++) { h ^= (unsigned char)(v >> (8*b)); h *= 1099511628211ULL; }
    return h;
}

static int assignDrop(const Scen& sc, int s, const int* dupShk, int nd)
{
    for (int d = 0; d < nd; d++) if (dupShk[d] == s) return d + 1;   // every drop's critical shackle
    return 1 + (s % nd);
}

static Result run(const Scen& sc, bool fixed)
{
    Result r; r.dupTrace = 0;
    g_ev = &r.ev; g_msgs = &r.msgs;

    overhead*     o   = (overhead*)     calloc(1, sizeof(overhead));      // never constructed: plain data
    SHARE_MEMORY* shm = (SHARE_MEMORY*) calloc(1, sizeof(SHARE_MEMORY));
    app = o; o->pShm = shm;
    memset(trc, 0, sizeof(trc)); memset(trc_buf, 0, sizeof(trc_buf));

    //--- configuration (what the host pushes)
    shm->sys_set.Shackles     = sc.N;
    shm->sys_set.SkipTrollies = sc.skip;
    shm->sys_set.SyncOn       = 4;                     // rig sync_debounce
    shm->sys_set.Grading      = sc.grading;
    shm->sys_set.MiscFeatures.EnableGradeSync2 = sc.gs2;
    shm->scl_set.NumScales    = sc.dual ? 2 : 1;
    shm->ZeroFlagMode         = sc.mode;
    shm->ZeroTabWindowMinMs   = 30;                    // rig 30 / 250 ms
    shm->ZeroTabWindowMaxMs   = 250;
    shm->ScaleSyncOffset      = -2;                    // rig
    shm->OpMode               = ModeRun;
    // drops: scale sync 1 -> 1-2, drop sync 1 -> 3-6, drop sync 2 -> 7-8
    shm->sys_set.SyncSettings[S_SC1].first = 1; shm->sys_set.SyncSettings[S_SC1].last = 2;
    shm->sys_set.SyncSettings[S_DS1].first = 3; shm->sys_set.SyncSettings[S_DS1].last = 6;
    shm->sys_set.SyncSettings[S_DS2].first = 7; shm->sys_set.SyncSettings[S_DS2].last = 8;
    // offsets incl. both ends of the RingSub-equivalence range [3-N, N+2]
    const int off[8] = { 0, -8, 20, sc.N + 2, 3 - sc.N, -33, 46, 5 };
    const int nd = 8;
    for (int d = 0; d < nd; d++)
    {
        shm->sys_set.DropSettings[d].Offset   = off[d];
        shm->sys_set.DropSettings[d].DropMode = DM_NORMAL;
        shm->sys_set.DropSettings[d].Active   = 1;
    }
    shm->sys_set.MBSync[0] = S_DS1 - 1;  shm->sys_set.MBOffset[0] = 20;   // rig: 1189+1-18 = 1172
    shm->sys_set.MBSync[1] = S_DS2 - 1;  shm->sys_set.MBOffset[1] = 30;
    shm->dbg_set.TestFireDrop = 0;

    // drop assignment table (FindDrops stand-in): each drop's "tab" shackle is its own
    int dupShk[nd];
    for (int d = 0; d < nd; d++) dupShk[d] = o->RingSub(1, off[d] + shm->ScaleSyncOffset, sc.N);
    std::vector<int> assign(sc.N + 2, 0);
    for (int s = 1; s <= sc.N + 1; s++) assign[s] = assignDrop(sc, s, dupShk, nd);
    for (int s = 0; s <= sc.N + 1 && s < MAXPENDANT; s++) shm->ShackleStatus[s].drop[0] = assign[s];

    //--- InitLocals (the parts these functions use)
    o->this_lineid = 1; o->dual_scale = sc.dual; o->syncOffset = 0;
    for (int i = 0; i < MAXSYNCS; i++)
    {
        o->sync_armed[i] = true; o->sync_debounce[i] = shm->sys_set.SyncOn;
        o->ss_tab_run[i] = SS_MIN_TROLLEYS_BETWEEN_TABS;
    }
    for (int g = 0; g < MAXGRADESYNCS; g++)
    {
        o->grade_armed[g] = false; o->grade_zeroed[g] = false;
        o->grade_debounce[g] = shm->sys_set.SyncOn;
        o->ss_grade_tab_run[g] = SS_MIN_TROLLEYS_BETWEEN_TABS;
    }
    o->ss_scan_tick = 0; o->ss_last_warn_tick = -100000;
    o->shk2shk_ticks = sc.T;

    //--- chain / pulse model
    const int L = sc.N * (sc.skip + 1) + sc.extra;         // trolleys on the chain
    const int k0 = L - 25;                                  // start 25 trolleys before the flag
    int phase[NSENS] = {0};
    phase[S_G1]  = 0;            phase[S_G2]  = 2*sc.T + 3;
    phase[S_SC1] = 7*sc.T + 5;   phase[S_SC2] = 9*sc.T + 9;
    phase[S_DS1] = 30*sc.T + 11; phase[S_DS2] = 55*sc.T + 17;
    const long total = (long)(sc.revs + 1) * L * sc.T + 80L * sc.T;

    // high? for one sensor at tick t.  rev = revolution index of the trolley.
    auto level = [&](int sens, long t, bool& flagBody) -> bool {
        flagBody = false;
        long x = t - phase[sens];
        if (x < 0) return false;
        long k = x / sc.T; int p = (int)(x % sc.T);
        long pos = (k + k0) % L;                 // chain position, 0 = flag trolley
        long rev = (k + k0) / L;
        if (p < sc.W) { flagBody = (pos == 0); return true; }
        if (sc.mode == 1 && pos == 0)
        {
            int g = sc.G;
            if (rev == sc.missTabRev) return false;
            if (rev == sc.lateTabRev) g = sc.lateG;
            if (p >= g && p < g + sc.W2) return true;
        }
        if (sc.mode == 1 && pos == sc.noiseTrolley && rev == sc.noiseRev &&
            p >= sc.G && p < sc.G + sc.W2) return true;
        return false;
    };
    auto isRawWindow = [&](long t) -> bool {        // OpMode Raw around DS1's flag body pulse
        if (sc.rawBodyRev < 0) return false;
        long x = t - phase[S_DS1]; if (x < 0) return false;
        long k = x / sc.T; int p = (int)(x % sc.T);
        return ((k + k0) % L == 0) && ((k + k0) / L == sc.rawBodyRev) && p < sc.G;
    };

    const int NumSyncs = 4;   // SC1, SC2, DS1, DS2 (NumSyncs extends to the last drop sync)
    unsigned int isysPrev = 0;
    for (long t = 1; t <= total; t++)
    {
        g_tick = t;
        o->ss_scan_tick++;                                   // App_Timer_Main does this first
        shm->OpMode = isRawWindow(t) ? ModeRaw : ModeRun;
        if (shm->OpMode == ModeRaw) r.rawTicks.push_back(t);

        //--- inputs
        memset(o->sync_in, 0, sizeof(o->sync_in));
        memset(o->sync_zero, 0, sizeof(o->sync_zero));
        o->switch_in[0] = 0;
        bool fb;
        for (int s = 0; s < NumSyncs; s++)
            if (level(s, t, fb))
            {
                o->sync_in[0] |= (1 << s);
                if (sc.mode == 0 && fb) o->sync_zero[0] |= (1 << s);   // standard: zero bit held over the flag trolley
            }
        if (level(S_G1, t, fb)) { o->switch_in[0] |= (1 << GRADESYNCBIT);  if (sc.mode == 0 && fb) o->switch_in[0] |= (1 << GRADEZEROBIT); }
        if (sc.gs2 && level(S_G2, t, fb)) { o->switch_in[0] |= (1 << GRADESYNC2BIT); if (sc.mode == 0 && fb) o->switch_in[0] |= (1 << GRADEZERO2BIT); }

        //--- pre-state for edge / zero detection
        bool armedPre[MAXSYNCS]; long long truePre[MAXSYNCS];
        bool gArmedPre[MAXGRADESYNCS]; long long gTruePre[MAXGRADESYNCS];
        for (int i = 0; i < MAXSYNCS; i++) { armedPre[i] = o->sync_armed[i]; truePre[i] = o->true_shackle_count[i]; }
        for (int g = 0; g < MAXGRADESYNCS; g++) { gArmedPre[g] = o->grade_armed[g]; gTruePre[g] = o->true_grade_shackle_count[g]; }

        //--- the scan (App_Timer_Main order: GradeSyncs, then ProcessSyncs)
        if (shm->sys_set.Grading) { if (fixed) o->GradeSyncs_NEW(); else o->GradeSyncs_OLD(); }
        if (fixed) o->ProcessSyncs_NEW(); else o->ProcessSyncs_OLD();

        //--- isys shackles/sec: one event per count
        while (isysPrev < (unsigned int) o->isys_shksec_sec_cnt)
        {
            isysPrev++;
            g_ev->push_back(Ev{t, 'I', 0, 0, o->RingSub(shm->SyncStatus[0].shackleno, 0, sc.N)});
        }

        //--- edges and accepted zeros
        for (int i = 0; i < NumSyncs; i++)
            if (armedPre[i] && !o->sync_armed[i])
            {
                r.edgeTicks[i].push_back(t);
                r.edgeCtr[i].push_back(shm->SyncStatus[i].shackleno);
                // ProcessSyncs' drop / missed-bird pass condition (SYNC_OK && scale-2 rule && ModeRun)
                r.edgeRan[i].push_back(shm->SyncStatus[i].zeroed && o->trolly_counters[i] == o->syncOffset &&
                                       (sc.dual || i != SCALE2SYNCBIT) && shm->OpMode == ModeRun);
                if (o->true_shackle_count[i] == 1 && truePre[i] > 1 && shm->SyncStatus[i].shackleno == 1)
                    r.zeroTicks[i].push_back(t);
            }
        for (int g = 0; g < MAXGRADESYNCS; g++)
            if (gArmedPre[g] && !o->grade_armed[g])
            {
                r.edgeTicks[8+g].push_back(t);
                r.edgeCtr[8+g].push_back(shm->grade_shackle[g]);
                r.edgeRan[8+g].push_back(shm->OpMode == ModeRun &&
                                         (sc.gs2 ? (o->grade_zeroed[0] && o->grade_zeroed[1]) : o->grade_zeroed[g]));
                if (o->true_grade_shackle_count[g] == 1 && gTruePre[g] > 1 && shm->grade_shackle[g] == 1)
                    r.zeroTicks[8+g].push_back(t);
            }

        //--- weigh trigger: record the label, then stand in for ProcessWeight + FindDrops
        for (int sc2 = 0; sc2 < 2; sc2++)
            if (o->weigh_state[sc2] == WeighActive)
            {
                if (sc2 == 0) r.weigh.push_back(std::make_pair(t, shm->WeighShackle[0]));
                o->weigh_state[sc2] = WeighIdle;
                int ws = shm->WeighShackle[sc2];
                if (ws >= 0 && ws <= sc.N + 1) shm->ShackleStatus[ws].drop[0] = assign[ws];
            }

        //--- trace buffer (normally drained by the trace thread)
        for (char* p = trc_buf[MAINBUFID]; (p = strstr(p, "SS tab")) != 0; p++) r.dupTrace++;
        trc_buf[MAINBUFID][0] = 0;

        //--- state hash: everything the counter / zero / realign path owns
        unsigned long long h = 1469598103934665603ULL;
        for (int i = 0; i < MAXSYNCS; i++)
        {
            h = fnv(h, shm->SyncStatus[i].shackleno); h = fnv(h, shm->SyncStatus[i].zeroed);
            h = fnv(h, o->true_shackle_count[i]);     h = fnv(h, o->trolly_counters[i]);
            h = fnv(h, o->sync_armed[i]);             h = fnv(h, o->sync_debounce[i]);
            h = fnv(h, o->ss_last_trolley_tick[i]);   h = fnv(h, o->ss_trolley_interval[i]);
            h = fnv(h, o->ss_trolley_stall[i]);       h = fnv(h, o->ss_tab_run[i]);
            h = fnv(h, o->ss_trolley_shrink[i]);
        }
        h = fnv(h, o->trolly_counters[MAXSYNCS]);
        for (int g = 0; g < MAXGRADESYNCS; g++)
        {
            h = fnv(h, shm->grade_shackle[g]);          h = fnv(h, o->true_grade_shackle_count[g]);
            h = fnv(h, o->grade_zeroed[g]);             h = fnv(h, o->grade_armed[g]);
            h = fnv(h, o->ss_grade_last_trolley_tick[g]); h = fnv(h, o->ss_grade_trolley_interval[g]);
            h = fnv(h, o->ss_grade_tab_run[g]);         h = fnv(h, o->ss_grade_trolley_shrink[g]);
        }
        h = fnv(h, shm->WeighShackle[0]); h = fnv(h, shm->WeighShackle[1]);
        h = fnv(h, shm->WeighZero[0]);    h = fnv(h, o->previousShackle1); h = fnv(h, o->previousShackle2);
        h = fnv(h, o->w_avg[0].avg_trigger_cntr); h = fnv(h, o->capt_wt.capture);
        h = fnv(h, (long long) r.msgs.size());
        r.hash.push_back(h);
    }
    free(shm); free(o); app = 0;
    return r;
}

//----- Analysis helpers ---------------------------------------------------
static int syncOfEvent(const Ev& e) { return e.type == 'G' ? 8 + e.idx : e.idx; }

static bool isTick(const std::vector<long>& v, long t) { return std::binary_search(v.begin(), v.end(), t); }

// Revolution windows [zero_r, zero_{r+1}) of one sync; count events of one kind per shackle.
static std::map<int,int> perShackle(const Result& r, char type, int sync, int sub, long lo, long hi)
{
    std::map<int,int> m;
    for (size_t i = 0; i < r.ev.size(); i++)
    {
        const Ev& e = r.ev[i];
        if (e.type == type && syncOfEvent(e) == sync && e.sub == sub && e.tick >= lo && e.tick < hi) m[e.shk]++;
    }
    return m;
}

static void checkScenario(const Scen& sc)
{
    printf("\n== %s\n", sc.name);
    Result O = run(sc, false);
    Result Nw = run(sc, true);

    //--- A. identical counter realignment
    size_t firstDiff = O.hash.size();
    for (size_t i = 0; i < O.hash.size() && i < Nw.hash.size(); i++) if (O.hash[i] != Nw.hash[i]) { firstDiff = i; break; }
    CHECK(O.hash.size() == Nw.hash.size() && firstDiff == O.hash.size(),
          "counter/zero/detector/weigh state diverges at scan %zu", firstDiff);
    CHECK(O.msgs == Nw.msgs, "GenError stream differs (%zu vs %zu msgs)", O.msgs.size(), Nw.msgs.size());
    CHECK(O.weigh == Nw.weigh, "weigh relabel sequence differs");
    CHECK(O.zeroTicks == Nw.zeroTicks && O.edgeTicks == Nw.edgeTicks, "edge / zero ticks differ");

    //--- B. NEW = OLD minus duplicates on accepted single-sensor tab edges
    std::vector<Ev> a = O.ev, b = Nw.ev;
    std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
    std::vector<Ev> onlyOld, onlyNew;
    std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(onlyOld));
    std::set_difference(b.begin(), b.end(), a.begin(), a.end(), std::back_inserter(onlyNew));
    CHECK(onlyNew.empty(), "NEW has %zu events OLD does not", onlyNew.size());
    int notOnTab = 0, notDup = 0;
    for (size_t i = 0; i < onlyOld.size(); i++)
    {
        const Ev& e = onlyOld[i];
        int s = syncOfEvent(e);
        if (sc.mode != 1 || !isTick(O.zeroTicks[s], e.tick))
        {
            notOnTab++;
            printf("   ! removed %c sync %d sub %d shk %d @%ld is not on an accepted tab\n", e.type, s, e.sub, e.shk, e.tick);
            continue;
        }
        if (e.type == 'I') continue;           // isys: the tab is not a trolley (its body pulse was counted)
        // must repeat an OLD event for the same (type, sync, sub, shackle) since the
        // counter last moved: i.e. within the preceding edges on this sync, back to
        // (and including) the last edge that ran with the counter at Shackles+1.
        const std::vector<long>& ed = O.edgeTicks[s];
        size_t k = std::lower_bound(ed.begin(), ed.end(), e.tick) - ed.begin();   // this edge
        long from = (k >= 3) ? ed[k-3] : 0;                                      // at most 2 skipped trolleys back
        bool found = false;
        for (size_t j = 0; j < O.ev.size() && !found; j++)
        {
            const Ev& p = O.ev[j];
            if (p.tick >= from && p.tick < e.tick && p.type == e.type && syncOfEvent(p) == s &&
                p.sub == e.sub && p.shk == e.shk) found = true;
        }
        if (!found)
        {
            notDup++;
            printf("   ! removed %c sync %d sub %d shk %d @%ld repeats nothing\n", e.type, s, e.sub, e.shk, e.tick);
        }
    }
    CHECK(notOnTab == 0, "%d removed events are NOT on an accepted tab edge", notOnTab);
    CHECK(notDup == 0,   "%d removed events are NOT duplicates", notDup);

    std::map<char,int> removed;
    for (size_t i = 0; i < onlyOld.size(); i++) removed[onlyOld[i].type]++;
    printf("   events OLD %zu  NEW %zu  removed:", O.ev.size(), Nw.ev.size());
    for (std::map<char,int>::iterator it = removed.begin(); it != removed.end(); ++it) printf(" %c=%d", it->first, it->second);
    printf("   NEW dup-skip trace lines: %d\n", Nw.dupTrace);
    if (sc.mode == 0) CHECK(onlyOld.empty(), "standard mode must be event-for-event identical (%zu differ)", onlyOld.size());

    //--- E. NEW: an accepted tab whose sync last ran its pass at Shackles+1 (counter not
    //    moved since - SkipTrollies edges may sit in between) runs NO per-shackle work.
    //    Also report duplicates NEW still has on fault edges (as 15.7.13, by design).
    if (sc.mode == 1)
    {
        int viol = 0, guarded = 0, maxBack = 0, residual = 0;
        for (std::map<int, std::vector<long> >::iterator zi = Nw.zeroTicks.begin(); zi != Nw.zeroTicks.end(); ++zi)
        {
            int sy = zi->first;
            const std::vector<long>& ed = Nw.edgeTicks[sy];
            const std::vector<int>&  ec = Nw.edgeCtr[sy];
            for (size_t zk = 0; zk < zi->second.size(); zk++)
            {
                size_t k = std::lower_bound(ed.begin(), ed.end(), zi->second[zk]) - ed.begin();
                // walk back over edges that left the counter at Shackles+1; did any run the pass?
                const std::vector<bool>& er = Nw.edgeRan[sy];
                int back = 0; bool ranAtOvershoot = false;
                for (size_t j = k; j > 0 && ec[j-1] == sc.N + 1; j--) { back++; if (er[j-1]) ranAtOvershoot = true; }
                if (!ranAtOvershoot) continue;
                guarded++;
                if (back > maxBack) maxBack = back;
                for (size_t e = 0; e < Nw.ev.size(); e++)
                    if (Nw.ev[e].tick == ed[k] && syncOfEvent(Nw.ev[e]) == sy && Nw.ev[e].type != 'I') viol++;
            }
        }
        CHECK(viol == 0, "NEW ran %d per-shackle events on tabs that follow a Shackles+1 pass", viol);
        // residual: NEW events that repeat an event of the same pass/shackle within the previous 2 edges
        for (std::map<int, std::vector<long> >::iterator ei = Nw.edgeTicks.begin(); ei != Nw.edgeTicks.end(); ++ei)
        {
            int sy = ei->first; const std::vector<long>& ed = ei->second;
            std::map<std::tuple<char,int,int>, long> last;   // (type, sub, shk) -> edge index of last event
            std::map<long, size_t> idx; for (size_t k = 0; k < ed.size(); k++) idx[ed[k]] = k;
            for (size_t e = 0; e < Nw.ev.size(); e++)
            {
                const Ev& v = Nw.ev[e];
                if (syncOfEvent(v) != sy || v.type == 'I' || v.type == 'G') continue;   // G repeats on skip edges by design
                std::map<long,size_t>::iterator it = idx.find(v.tick); if (it == idx.end()) continue;
                std::tuple<char,int,int> key(v.type, v.sub, v.shk);
                std::map<std::tuple<char,int,int>, long>::iterator L = last.find(key);
                if (L != last.end() && L->second != (long) it->second && (long) it->second - L->second <= 2) residual++;
                last[key] = it->second;
            }
        }
        printf("   E: %d tab edges followed a Shackles+1 pass (guard reached back up to %d edge%s), %d re-ran work;"
               " fault-path repeats left as in 15.7.13: %d\n", guarded, maxBack, maxBack == 1 ? "" : "s", viol, residual);
        if (sc.clean) CHECK(residual == 0, "clean run still has %d repeats", residual);
    }

    //--- C. exactly once per revolution per pass (clean single-sensor runs)
    if (sc.clean && sc.mode == 1)
    {
        // Expected passes per shackle per revolution. GradeProcess runs on EVERY grade
        // edge (legacy, incl. SkipTrollies trolleys that do not count), so skip+1 there.
        struct P { char type; int sync; int sub; int per; };
        std::vector<P> passes;
        passes.push_back(P{'M', S_DS1, 0, 1}); passes.push_back(P{'R', S_DS1, 0, 1});
        passes.push_back(P{'M', S_DS2, 1, 1}); passes.push_back(P{'R', S_DS2, 0, 1});
        if (sc.grading) passes.push_back(P{'G', 8, 0, sc.skip + 1});
        if (sc.grading && sc.gs2) passes.push_back(P{'G', 9, 0, sc.skip + 1});
        for (size_t pi = 0; pi < passes.size(); pi++)
        {
            const P& p = passes[pi];
            const std::vector<long>& z = O.zeroTicks[p.sync];
            CHECK(z.size() >= 3, "%c sync %d: only %zu zeros", p.type, p.sync, z.size());
            int badNew = 0, badOld = 0, dupOld = 0;
            for (size_t zr = 1; zr + 1 < z.size(); zr++)            // full revolutions only
            {
                std::map<int,int> mn = perShackle(Nw, p.type, p.sync, p.sub, z[zr], z[zr+1]);
                std::map<int,int> mo = perShackle(O,  p.type, p.sync, p.sub, z[zr], z[zr+1]);
                if ((int) mn.size() != sc.N) badNew++;
                for (std::map<int,int>::iterator it = mn.begin(); it != mn.end(); ++it) if (it->second != p.per) badNew++;
                for (std::map<int,int>::iterator it = mo.begin(); it != mo.end(); ++it) if (it->second == p.per + 1) dupOld++; else if (it->second != p.per) badOld++;
            }
            int revs = (int) z.size() - 2;
            CHECK(badNew == 0, "NEW %c sync %d: not exactly %d per shackle per revolution (%d bad)", p.type, p.sync, p.per, badNew);
            CHECK(dupOld == revs && badOld == 0, "OLD %c sync %d: expected exactly 1 dup/rev (%d dups in %d revs, %d bad)",
                  p.type, p.sync, dupOld, revs, badOld);
            printf("   %c sync %d: %d full revs, NEW every shackle x%d/rev, OLD one shackle x%d/rev\n", p.type, p.sync, revs, p.per, p.per + 1);
        }
        // drop fires: every drop's shackles exactly once per revolution of its sync
        for (int s = 0; s < 4; s++)
        {
            if (s == S_SC2) continue;
            const std::vector<long>& z = O.zeroTicks[s];
            int badNew = 0, dupOld = 0, fires = 0;
            for (size_t zr = 1; zr + 1 < z.size(); zr++)
                for (int d = 1; d <= 8; d++)
                {
                    std::map<int,int> mn = perShackle(Nw, 'F', s, d, z[zr], z[zr+1]);
                    std::map<int,int> mo = perShackle(O,  'F', s, d, z[zr], z[zr+1]);
                    for (std::map<int,int>::iterator it = mn.begin(); it != mn.end(); ++it) { fires++; if (it->second != 1) badNew++; }
                    for (std::map<int,int>::iterator it = mo.begin(); it != mo.end(); ++it) if (it->second == 2) dupOld++;
                }
            if (fires == 0) continue;
            CHECK(badNew == 0, "NEW drop fire on sync %d repeated a shackle (%d)", s, badNew);
            CHECK(dupOld > 0,  "OLD drop fire on sync %d shows no double kick (harness blind?)", s);
            printf("   F sync %d: NEW %d fires, no shackle kicked twice; OLD %d double kicks\n", s, fires, dupOld);
        }
        // isys: trolleys per revolution on scale sync 1
        const std::vector<long>& z = O.zeroTicks[S_SC1];
        if (z.size() >= 3)
        {
            long lo = z[1], hi = z[2];
            int in = 0, io = 0;
            for (size_t i = 0; i < Nw.ev.size(); i++) if (Nw.ev[i].type == 'I' && Nw.ev[i].tick >= lo && Nw.ev[i].tick < hi) in++;
            for (size_t i = 0; i < O.ev.size();  i++) if (O.ev[i].type == 'I'  && O.ev[i].tick  >= lo && O.ev[i].tick  < hi) io++;
            int trolleys = sc.N * (sc.skip + 1) + sc.extra;
            CHECK(in == trolleys && io == trolleys + 1, "isys per rev NEW %d OLD %d (chain %d)", in, io, trolleys);
            printf("   isys counts/rev: NEW %d (= trolleys)  OLD %d\n", in, io);
        }
    }
}

static int RingSubPlain(int s, int off, int n) { int d = s - off; return d > 0 ? (d > n ? d - n : d) : d + n; }

static int countMsg(const Result& r, const char* sub)
{
    int n = 0;
    for (size_t i = 0; i < r.msgs.size(); i++) if (strstr(r.msgs[i].c_str(), sub)) n++;
    return n;
}

static Scen base(const char* name)
{
    Scen s; memset(&s, 0, sizeof(s));
    s.name = name; s.mode = 1; s.N = 60; s.skip = 0; s.extra = 0;
    s.grading = true; s.gs2 = false; s.dual = false;
    s.T = 66; s.W = 12; s.G = 28; s.W2 = 10;              // 180 SPM, tab 140 ms (rig)
    s.revs = 6; s.clean = true;
    s.missTabRev = s.lateTabRev = s.noiseRev = s.rawBodyRev = -1;
    s.noiseTrolley = -1; s.lateG = 0;
    return s;
}

int main()
{
    // 1. The rig: 1189 shackles, MB offset 20 + scale offset -2 -> shackle 1172.
    {
        Scen s = base("rig replica: N=1189, 180 SPM, tab 140 ms, grading, single-sensor");
        s.N = 1189; s.revs = 3;
        checkScenario(s);
        Result O = run(s, false), Nw = run(s, true);
        int o1172 = 0, n1172 = 0;
        long lo = O.zeroTicks[S_DS1][1], hi = O.zeroTicks[S_DS1][2];
        for (size_t i = 0; i < O.ev.size(); i++)  if (O.ev[i].type == 'R'  && O.ev[i].shk == 1172 && O.ev[i].idx == S_DS1 && O.ev[i].tick >= lo && O.ev[i].tick < hi) o1172++;
        for (size_t i = 0; i < Nw.ev.size(); i++) if (Nw.ev[i].type == 'R' && Nw.ev[i].shk == 1172 && Nw.ev[i].idx == S_DS1 && Nw.ev[i].tick >= lo && Nw.ev[i].tick < hi) n1172++;
        CHECK(o1172 == 2 && n1172 == 1, "shackle 1172 drop records per rev: OLD %d NEW %d", o1172, n1172);
        printf("   shackle 1172 DROP_RECORDs per revolution: OLD %d  NEW %d\n", o1172, n1172);
        // weigh relabel still lands the flag trolley on shackle 1 (auto-zero) every revolution
        int relabel = 0;
        for (size_t i = 1; i < Nw.weigh.size(); i++)
            if (Nw.weigh[i-1].second == s.N + 1 && Nw.weigh[i].second == 1) relabel++;
        CHECK(relabel >= 3, "weigh relabel N+1 -> 1 seen %d times", relabel);
    }

    // 2. Single-sensor variants
    { Scen s = base("single-sensor, grading off");                       s.grading = false; checkScenario(s); }
    { Scen s = base("single-sensor, grade sync 2 enabled");              s.gs2 = true;      checkScenario(s); }
    { Scen s = base("single-sensor, dual scale");                        s.dual = true;     checkScenario(s); }
    { Scen s = base("Pitman-like: 58 SPM, SkipTrollies=1, no grading, tab 0.42T"); s.T = 103; s.W = 11; s.G = 43; s.W2 = 11; s.skip = 1; s.grading = false; s.revs = 4; checkScenario(s); }
    { Scen s = base("single-sensor, 20 revolutions (counter wrap x20)");  s.revs = 20; checkScenario(s); }

    // 3. Perturbations: counter must realign exactly as 15.7.13; only true duplicates go
    const int zClean = (int) run(base("ref"), true).zeroTicks[S_DS1].size();
    {
        Scen s = base("missed tab in revolution 3 (Zero Flag NOT Detected path)");  s.clean = false; s.missTabRev = 3; checkScenario(s);
        Result r = run(s, true);
        CHECK((int) r.zeroTicks[S_DS1].size() == zClean - 1, "missed tab: zeros %zu vs clean %d", r.zeroTicks[S_DS1].size(), zClean);
        CHECK(countMsg(r, "Zero Flag NOT Detected") >= 1, "missed tab: no 'Zero Flag NOT Detected'");
    }
    {
        Scen s = base("late tab (0.60T, out of window) in revolution 3");          s.clean = false; s.lateTabRev = 3; s.lateG = 40; checkScenario(s);
        Result r = run(s, true);
        CHECK(countMsg(r, "Zero Flag NOT Detected") >= 1, "late tab: tab was not rejected");
    }
    {
        Scen s = base("early tab (0.15T, below window) in revolution 3");          s.clean = false; s.W = 6; s.W2 = 6; s.lateTabRev = 3; s.lateG = 10; checkScenario(s);
        Result r = run(s, true);
        CHECK(countMsg(r, "Zero Flag NOT Detected") >= 1, "early tab: tab was not rejected");
    }
    {
        Scen s = base("spurious double pulse mid-revolution (Early Zero)");        s.clean = false; s.noiseRev = 3; s.noiseTrolley = 30; checkScenario(s);
        Result r = run(s, true);
        CHECK((int) r.zeroTicks[S_DS1].size() == zClean + 1, "spurious: zeros %zu vs clean %d", r.zeroTicks[S_DS1].size(), zClean);
        CHECK(countMsg(r, "Early Zero Flag Detected") >= 1, "spurious: no 'Early Zero Flag Detected'");
    }
    {
        Scen s = base("OpMode Raw over the body pulse, Run for the tab");          s.clean = false; s.rawBodyRev = 3; checkScenario(s);
        // The body pass did not run (Raw), so the tab must run it - exactly once, in both versions.
        for (int v = 0; v < 2; v++)
        {
            Result r = run(s, v == 1);
            CHECK(!r.rawTicks.empty(), "raw window never applied");
            long tab = 0;
            for (size_t i = 0; i < r.zeroTicks[S_DS1].size(); i++) if (r.zeroTicks[S_DS1][i] > r.rawTicks.back()) { tab = r.zeroTicks[S_DS1][i]; break; }
            int dupShk = RingSubPlain(1, 20 - 2, s.N), atTab = 0, inBody = 0;
            for (size_t i = 0; i < r.ev.size(); i++)
                if (r.ev[i].type == 'M' && r.ev[i].idx == S_DS1 && r.ev[i].shk == dupShk)
                {
                    if (r.ev[i].tick == tab) atTab++;
                    if (r.ev[i].tick >= r.rawTicks.front() && r.ev[i].tick < tab) inBody++;
                }
            CHECK(tab != 0 && atTab == 1 && inBody == 0, "%s: tab after a Raw body pass must process shk %d once at the tab (tab %d, body %d)",
                  v ? "NEW" : "OLD", dupShk, atTab, inBody);
        }
        printf("   Raw body / Run tab: both versions process the shackle once, at the tab\n");
    }

    // 4. Turkey lines
    { Scen s = base("SkipTrollies=1, even chain (flag body counts N+1)");        s.skip = 1; s.revs = 4; checkScenario(s); }
    { Scen s = base("SkipTrollies=1, odd chain (flag body does not count)");     s.skip = 1; s.extra = 1; s.revs = 4; s.clean = false; checkScenario(s); }  // E shows the guard reaching 2 edges back

    // 5. Standard two-sensor zero: must be event-for-event identical
    { Scen s = base("standard two-sensor, grading");                             s.mode = 0; s.clean = false; checkScenario(s); }
    { Scen s = base("standard two-sensor, SkipTrollies=1, gs2, dual");           s.mode = 0; s.skip = 1; s.gs2 = true; s.dual = true; s.clean = false; checkScenario(s); }

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
