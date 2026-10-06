//--------------------------------------------------------------------------
//  sszero_window_test.cpp - single-sensor zero tab detector vs real line speeds (15.7.15)
//
//  Build + run:   linux_port/tools/sszero_window_test.sh [group ...]
//  (the script cuts the REAL ProcessSyncs / GradeSyncs / SingleSensorIsZeroTab /
//   SingleSensorTabRule / ... / SingleSensorAlarmOk out of the working tree as *_NEW,
//   and 15.7.13's (git f9f2968, the build Pitman runs) as *_OLD, and compiles this
//   file against the controller headers)
//
//  THE FAULT (Pitman, Oct 2026). 15.7.13 accepted the zero tab only inside the host's
//  ms rails; Pitman's ZeroTabWindowMaxMs = 375 rejects the genuine tab (0.42 x 30000/SPM
//  ms) whenever the line runs below ~33.6 SPM, i.e. every revolution at turkey speed.
//
//  MODEL. A chain of L = Shackles*(SkipTrollies+1) trolleys moves past each sensor at a
//  speed profile SPM(t); the controller samples every 5 ms scan exactly as App_Timer_Main
//  does (GradeSyncs, then ProcessSyncs, with the real SyncOn debounce). Trolley k's body
//  covers [k+d_k, k+d_k+wb) of chain position; the flag trolley (k % L == 0) also has a
//  tab at [k+d_k+f_k, +wt). Jitter j: d_k ~ U(-j/2, j/2) per trolley (gap jitter up to
//  +/-j, fresh every revolution), tab lead f_k = f*(1 + U(-j/2, j/2)). Optional spurious
//  tab-like "bounce" pulses, removed tabs (sensor fault) and stops.
//
//  PER SCENARIO, PER SYNC: tab pulses that passed, tabs the detector missed after its
//  first zero, false zeros (accepted edges that were not the tab), the flag that gave the
//  first zero, zero-flag alarms by kind, alarms per physical revolution, the smallest
//  count spacing between alarms, and "silent" faults - a miss or false zero with no alarm
//  for that sync within one revolution of counts either side.
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

#define ProcessSyncs ProcessSyncs(); void ProcessSyncs_OLD(); \
    bool SingleSensorIsZeroTab_OLD(__int64 &lastTrolleyTick, __int64 &interval, int &stall, int &tabRun, int &shrink); \
    bool SingleSensorWarnOk_OLD(); __int64 ss_last_warn_tick; void ProcessSyncs_NEW
#define GradeSyncs   GradeSyncs();   void GradeSyncs_OLD();   void GradeSyncs_NEW
#define private public
#include "types.h"
#undef private
#undef ProcessSyncs
#undef GradeSyncs

//----- Globals the extracted functions use (defined in overhead.cpp normally)
overhead*   app = 0;
UINT        TraceMask = 0;
trcbuf      trc_buf    [MAXTRCBUFFERS];
tmptrcbuf   tmp_trc_buf[MAXTRCBUFFERS];
trace_ctrl  trc        [MAXTRCBUFFERS];
char        app_err_buf[MAXERRMBUFSIZE];
char        sync_desc[MAXSYNCS][MAX_DBG_DESC] = {
    "Scale 1", "Scale 2", "Drop Sync 1", "Drop Sync 2", "Drop Sync 3",
    "Drop Sync 4", "Drop Sync 5", "Drop Sync 6" };
volatile sig_atomic_t g_outputs_disabled = 0;
const BYTE  Mask[8] = {0x01,0x2,0x4,0x8,0x10,0x20,0x40,0x80};
#ifndef XC_REAL_ERRQ
int  RtPrintf(const char*, ...) { return 0; }
#endif
void DebugTrace(UINT, char*, ...) {}
#define _FILE_ "overhead.cpp"
typedef app_type::SHARE_MEMORY SHARE_MEMORY;
#define RtReadPortUchar(a) ((UCHAR) 0)

struct Msg { long tick; std::string txt; };
static std::vector<Msg>* g_msgs = 0;
static long              g_tick = 0;
void overhead::SetOutput(int, DBOOL)                 {}
bool overhead::MissedBirdCheck(int, int, int)        { return false; }
void overhead::AddDropRecord(int)                    {}
#ifdef XC_REAL_ERRQ          // 15.7.16: the real GenError -> queue -> SendError; the host's view is analysed
#include "errq_harness.h"
static void deliverMsg(const char* t)                 { g_msgs->push_back(Msg{g_tick, t}); }
#else
void overhead::GenError(int, char* txt)              { g_msgs->push_back(Msg{g_tick, txt}); }
#endif
void overhead::GradeProcess(int)                     {}
int  overhead::SendLineMsg(int, int, int, BYTE*, int) { return 0; }

#include "sszero_shared.inc"
#include "sszero_old.inc"
#include "sszero_new.inc"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

//----- deterministic per-trolley randomness ---------------------------------
static inline double urand(unsigned long long k, unsigned long long salt)
{
    unsigned long long z = k * 0x9E3779B97F4A7C15ULL + salt * 0xBF58476D1CE4E5B9ULL + 0x94D049BB133111EBULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return (double)(z >> 11) / 9007199254740992.0;
}

//----- scenario --------------------------------------------------------------
struct Knot { double t, spm; };                 // piecewise-linear SPM(t), t in s
enum { SY_SC1 = 0, SY_DS1 = 1, SY_G1 = 2, SY_G2 = 3, NSY = 4 };
static const char* syName[NSY] = { "Scale 1", "Drop Sync 1", "Grade Sync 1", "Grade Sync 2" };

struct Cfg
{
    std::string name;
    int    N, skip;
    double f, wb, wt, jit;
    unsigned seed;
    std::vector<Knot> prof;
    double dur;                 // s
    bool   useDS1, grading, gs2;
    double offDS1;              // sensor offsets (trolleys downstream of scale sync 1)
    int    syncOn;
    int    railMin, railMax;    // host ms rails (read only by the 15.7.13 detector)
    double startFrac;           // boot position (fraction of a revolution before the first flag)
    double bounce;              // fraction of trolleys carrying a spurious tab-like pulse
    int    missFrom, missTo;    // flags (revolution index) whose tab is removed [from, to)
    // position-triggered speed event (adversarial): when scale-1's sensor reaches trigPos,
    // ramp from the current speed to trigSpm over trigRamp s; hold trigHold s; then (if
    // trigBack >= 0) ramp to trigBack over trigRamp2 s.
    double trigPos, trigSpm, trigRamp, trigHold, trigBack, trigRamp2;
    bool   runOld;
};

static Cfg mk(const std::string& name, int N, int skip, double f, double spm, double revs)
{
    Cfg c;
    c.name = name; c.N = N; c.skip = skip; c.f = f; c.wb = 0.105; c.wt = 0.105; c.jit = 0;
    c.seed = 1; c.prof.push_back(Knot{0, spm});
    double revSec = (double) N / spm * 60.0;
    c.dur = revs * revSec;
    c.useDS1 = true; c.grading = false; c.gs2 = false; c.offDS1 = 41.37;
    c.syncOn = 4; c.railMin = 30; c.railMax = 375;
    c.startFrac = 0.37; c.bounce = 0; c.missFrom = c.missTo = -1;
    c.trigPos = -1; c.trigSpm = 0; c.trigRamp = 0; c.trigHold = 0; c.trigBack = -1; c.trigRamp2 = 0;
    c.runOld = false;
    return c;
}

struct SyncStats
{
    int  edges = 0, tabEdges = 0, tabsPassed = 0, accTab = 0, accFalse = 0, missed = 0, bootSkipped = 0;
    int  firstZeroFlag = -1;            // 1 = the first flag after boot
    bool firstZeroFalse = false;        // the sync's first accepted zero was not the tab
    int  nd = 0, early = 0, late = 0, mism = 0, initial = 0, reacq = 0;
    int  maxPerRev = 0;
    long minSpacing = -1, firstAlarmCnt = -1;
    int  silent = 0, faults = 0, unassessed = 0;
    int  spacingViol = 0;
};
struct RunRes { SyncStats s[NSY]; bool active[NSY]; std::vector<Msg> msgs; };

static double spmAt(const Cfg& c, double t)
{
    const std::vector<Knot>& k = c.prof;
    if (t <= k[0].t) return k[0].spm;
    for (size_t i = 1; i < k.size(); i++)
        if (t < k[i].t)
        {
            double a = k[i-1].t, b = k[i].t;
            if (b <= a) return k[i].spm;
            return k[i-1].spm + (k[i].spm - k[i-1].spm) * (t - a) / (b - a);
        }
    return k.back().spm;
}

// feature under a sensor at chain position pos: -1 low, else 4*k + type (0 body, 1 tab, 2 bounce)
struct Chain
{
    const Cfg* c; long long L;
    double d(long long k)  const { return c->jit * (urand(k, c->seed * 3 + 1) - 0.5); }
    double fk(long long k) const { return c->f * (1.0 + c->jit * (urand(k, c->seed * 3 + 2) - 0.5)); }
    bool   isFlag(long long k) const { return k % L == 0; }
    bool   tabOn(long long k) const
    {
        long long r = k / L;
        return isFlag(k) && !(c->missFrom >= 0 && r >= c->missFrom && r < c->missTo);
    }
    bool   bounceOn(long long k) const { return c->bounce > 0 && !isFlag(k) && urand(k, c->seed * 3 + 3) < c->bounce; }
    long long feature(double pos) const
    {
        long long k0 = (long long) floor(pos);
        for (long long kk = k0 + 1; kk >= k0; kk--)
        {
            if (kk < 0) continue;
            double e = kk + d(kk);
            if (pos >= e && pos < e + c->wb) return 4 * kk;
            if (tabOn(kk)) { double te = e + fk(kk); if (pos >= te && pos < te + c->wt) return 4 * kk + 1; }
            if (bounceOn(kk)) { double te = e + c->f; if (pos >= te && pos < te + c->wt) return 4 * kk + 2; }
        }
        return -1;
    }
};

static const char* g_debug = getenv("SSDEBUG");   // scenario name: trace its zero edges and alarms
static RunRes run(const Cfg& c, bool useOld)
{
    const bool dbg = g_debug && c.name == g_debug && !useOld;
    RunRes R; g_msgs = &R.msgs;
    overhead*     o   = (overhead*)     calloc(1, sizeof(overhead));
    SHARE_MEMORY* shm = (SHARE_MEMORY*) calloc(1, sizeof(SHARE_MEMORY));
    app = o; o->pShm = shm;
    memset(trc, 0, sizeof(trc)); memset(trc_buf, 0, sizeof(trc_buf));
#ifdef XC_REAL_ERRQ
    errq_begin(o); errq_deliver = deliverMsg;
#endif

    shm->sys_set.Shackles     = c.N;
    shm->sys_set.SkipTrollies = c.skip;
    shm->sys_set.SyncOn       = c.syncOn;
    shm->sys_set.Grading      = c.grading;
    shm->sys_set.MiscFeatures.EnableGradeSync2 = c.gs2;
    shm->scl_set.NumScales    = 1;
    shm->ZeroFlagMode         = 1;
    shm->ZeroTabWindowMinMs   = c.railMin;
    shm->ZeroTabWindowMaxMs   = c.railMax;
    shm->OpMode               = ModeRun;
    if (c.useDS1) { shm->sys_set.SyncSettings[2].first = 1; shm->sys_set.SyncSettings[2].last = 1; }
    shm->sys_set.MBSync[0] = 7; shm->sys_set.MBSync[1] = 7;          // no missed-bird pass on our syncs
    o->this_lineid = 1; o->dual_scale = false; o->syncOffset = 0;
    for (int i = 0; i < MAXSYNCS; i++)
    {
        o->sync_armed[i] = true; o->sync_debounce[i] = c.syncOn;
        o->ss_tab_run[i] = useOld ? SS_MIN_TROLLEYS_BETWEEN_TABS : 0;     // each version's InitLocals
    }
    for (int g = 0; g < MAXGRADESYNCS; g++)
    {
        o->grade_armed[g] = false; o->grade_zeroed[g] = false; o->grade_debounce[g] = c.syncOn;
        o->ss_grade_tab_run[g] = useOld ? SS_MIN_TROLLEYS_BETWEEN_TABS : 0;
    }
    o->ss_scan_tick = 0; o->ss_last_warn_tick = -100000; o->shk2shk_ticks = 50;

    Chain ch; ch.c = &c; ch.L = (long long) c.N * (c.skip + 1);
    const double tps = c.skip + 1;
    // sensor offsets: grade sensors upstream of the scale (see the flag first), drop sync downstream
    const double off[NSY] = { 0.0, c.offDS1, -5.21, -3.13 };
    R.active[SY_SC1] = true; R.active[SY_DS1] = c.useDS1; R.active[SY_G1] = c.grading; R.active[SY_G2] = c.grading && c.gs2;
    const int pIdx[NSY] = { 0, 2, -1, -1 };

    // boot: scale 1's sensor sits startFrac of a revolution before flag 3 (chain position 3L);
    // with boot confirmation the first zero is flag 4 (4L), steady state from flag 5 (5L).
    double x = (3.0 - c.startFrac) * ch.L;
    double x0 = x;
    long nScans = (long) (c.dur * 200.0);

    // per-sync bookkeeping
    long  incr[NSY] = {0};                       // count advances since boot (independent of the code under test)
    long  lastAlarmCnt[NSY]; for (int s = 0; s < NSY; s++) lastAlarmCnt[s] = -1;
    bool  zeroedOnce[NSY] = {false};
    std::map<long long,int> flagAcc[NSY];        // flag revolution index -> accepted
    std::map<long long,long> flagCnt[NSY];       // flag -> count at its tab edge (rejected tabs)
    std::vector<long> alarmCnt[NSY], faultCnt[NSY];
    std::map<long long,int> perRev[NSY];
    size_t msgSeen = 0;

    // trigger state
    bool trig = false; double trigT = 0, trigFrom = 0;
    double curSpm = spmAt(c, 0);

    for (long n = 1; n <= nScans; n++)
    {
        g_tick = n;
        double t = (n - 0.5) * 0.005;
        double spm;
        if (c.trigPos >= 0 && !trig && (x - off[SY_SC1]) >= c.trigPos) { trig = true; trigT = t; trigFrom = curSpm; }
        if (trig)
        {
            double u = t - trigT;
            if (u < c.trigRamp) spm = trigFrom + (c.trigSpm - trigFrom) * (c.trigRamp > 0 ? u / c.trigRamp : 1);
            else if (c.trigBack < 0 || u < c.trigRamp + c.trigHold) spm = c.trigSpm;
            else
            {
                double v = u - c.trigRamp - c.trigHold;
                spm = (v < c.trigRamp2 && c.trigRamp2 > 0) ? c.trigSpm + (c.trigBack - c.trigSpm) * v / c.trigRamp2 : c.trigBack;
            }
        }
        else spm = spmAt(c, t);
        curSpm = spm;
        x += spm * tps / 60000.0 * 5.0;

        o->ss_scan_tick++;
        memset(o->sync_in, 0, sizeof(o->sync_in)); memset(o->sync_zero, 0, sizeof(o->sync_zero));
        o->switch_in[0] = 0;
        long long feat[NSY];
        for (int s = 0; s < NSY; s++)
        {
            feat[s] = R.active[s] ? ch.feature(x - off[s]) : -1;
            if (feat[s] < 0) continue;
            if (s == SY_SC1) o->sync_in[0] |= 1;
            if (s == SY_DS1) o->sync_in[0] |= 4;
            if (s == SY_G1)  o->switch_in[0] |= (1 << GRADESYNCBIT);
            if (s == SY_G2)  o->switch_in[0] |= (1 << GRADESYNC2BIT);
        }

        bool armedPre[NSY]; long long truePre[NSY]; bool gzPre = o->grade_zeroed[0];
        int shkPre0 = shm->SyncStatus[0].shackleno, tcPre0 = o->trolly_counters[0], runPre0 = o->ss_tab_run[0];
        __int64 gPre0 = o->ss_rule[0].gap, g2Pre0 = o->ss_rule[0].gapPrev, lastPre0 = o->ss_last_trolley_tick[0];
        for (int s = 0; s < NSY; s++)
        {
            if (s < 2) { armedPre[s] = o->sync_armed[pIdx[s]]; truePre[s] = o->true_shackle_count[pIdx[s]]; }
            else       { armedPre[s] = o->grade_armed[s - 2];  truePre[s] = o->true_grade_shackle_count[s - 2]; }
        }

        if (c.grading) { if (useOld) o->GradeSyncs_OLD(); else o->GradeSyncs_NEW(); }
        if (useOld) o->ProcessSyncs_OLD(); else o->ProcessSyncs_NEW();
#ifdef XC_REAL_ERRQ
        errq_drain(o);                                  // GpSendThread's pass
#endif

        bool gradeInitial = c.grading && !gzPre && o->grade_zeroed[0];
        for (int s = 0; s < NSY; s++)
        {
            if (!R.active[s]) continue;
            bool edge; bool acc; long long tp;
            if (s < 2)
            {
                int i = pIdx[s];
                edge = armedPre[s] && !o->sync_armed[i];
                acc  = edge && o->ss_last_trolley_tick[i] != o->ss_scan_tick;
                tp   = o->true_shackle_count[i];
            }
            else
            {
                int g = s - 2;
                edge = armedPre[s] && !o->grade_armed[g];
                acc  = edge && o->ss_grade_last_trolley_tick[g] != o->ss_scan_tick;
                tp   = o->true_grade_shackle_count[g];
            }
            if (!acc)
            {
                if (s < 2 && gradeInitial) incr[s] += (tp > 0 ? tp : 0);
                else if (tp > truePre[s]) incr[s] += tp - truePre[s];
            }
            if (!edge) continue;
            if (dbg && s == SY_SC1)
            {
                int ty = feat[s] < 0 ? -1 : (int) (feat[s] & 3);
                long long kk2 = feat[s] >> 2;
                if (acc || ty != 0 || (kk2 % ch.L) > ch.L - 12 || (kk2 % ch.L) < 3)
                    printf("    dbg t=%ld %s k=%lld (pos %lld) delta=%lld G=%lld G2=%lld tabRun=%d shk %d->%d tc %d->%d %s\n", n,
                           ty == 0 ? "body" : ty == 1 ? "TAB " : ty == 2 ? "bnc " : "?", kk2, kk2 % ch.L, (long long) (o->ss_scan_tick - lastPre0),
                           (long long) gPre0, (long long) g2Pre0, runPre0, shkPre0, shm->SyncStatus[0].shackleno, tcPre0, o->trolly_counters[0],
                           acc ? "ACCEPTED" : "");
            }
            SyncStats& st = R.s[s];
            st.edges++;
            int type = (int) (feat[s] & 3); long long kk = feat[s] >> 2;
            long long flag = kk / ch.L;
            if (feat[s] >= 0 && type == 1)
            {
                st.tabEdges++;
                flagAcc[s][flag] |= acc ? 1 : 0;
                flagCnt[s][flag] = incr[s];
            }
            if (acc)
            {
                if (feat[s] >= 0 && type == 1)
                {
                    st.accTab++;
                    if (!zeroedOnce[s]) { zeroedOnce[s] = true; st.firstZeroFlag = (int) (flag - (long long) floor((x0 - off[s]) / ch.L)); }
                }
                else { st.accFalse++; faultCnt[s].push_back(incr[s]); if (!zeroedOnce[s]) { zeroedOnce[s] = true; st.firstZeroFalse = true; } }
            }
        }

        // alarms raised this scan
        for (; msgSeen < R.msgs.size(); msgSeen++)
        {
            const std::string& m = R.msgs[msgSeen].txt;
            if (dbg) printf("    dbg t=%ld MSG %s", n, m.c_str());
            int s = -1;
            for (int k = NSY - 1; k >= 0; k--) if (m.find(syName[k]) != std::string::npos) { s = k; break; }
            if (s < 0) continue;
            SyncStats& st = R.s[s];
            if (m.find("Initial") != std::string::npos) { st.initial++; continue; }
            if (m.find("re-acquired") != std::string::npos) { st.reacq++; continue; }
            if      (m.find("NOT Detected") != std::string::npos) st.nd++;
            else if (m.find("Early") != std::string::npos)        st.early++;
            else if (m.find("Late") != std::string::npos)         st.late++;
            else if (m.find("mismatch") != std::string::npos)     st.mism++;
            else continue;
            long cnt = incr[s];
            if (lastAlarmCnt[s] < 0) st.firstAlarmCnt = cnt;
            else { long sp = cnt - lastAlarmCnt[s]; if (st.minSpacing < 0 || sp < st.minSpacing) st.minSpacing = sp; }
            lastAlarmCnt[s] = cnt;
            alarmCnt[s].push_back(cnt);
            long long rev = (long long) floor((x - off[s]) / ch.L);
            int pr = ++perRev[s][rev]; if (pr > st.maxPerRev) st.maxPerRev = pr;
        }
    }

    // tabs that passed each sensor; misses after the first zero; silent faults
    for (int s = 0; s < NSY; s++)
    {
        if (!R.active[s]) continue;
        SyncStats& st = R.s[s];
        long long fFirst = (long long) ceil((x0 - off[s]) / ch.L), fLast = (long long) floor((x - off[s] - 1.0) / ch.L);
        long long firstAccFlag = -1;
        for (std::map<long long,int>::iterator it = flagAcc[s].begin(); it != flagAcc[s].end(); ++it)
            if (it->second) { firstAccFlag = it->first; break; }
        for (long long fl = fFirst; fl <= fLast; fl++)
        {
            long long kk = fl * ch.L;
            if (!ch.tabOn(kk)) continue;
            st.tabsPassed++;
            bool a = flagAcc[s].count(fl) && flagAcc[s][fl];
            if (firstAccFlag < 0 || fl < firstAccFlag) { if (!a) st.bootSkipped++; continue; }
            if (!a)
            {
                st.missed++;
                faultCnt[s].push_back(flagCnt[s].count(fl) ? flagCnt[s][fl] : -1);
            }
        }
        // a fault is alarmed if this sync raised a zero-flag alarm within one revolution of counts
        // either side (an alarm just before it is what suppresses a second one - the rule).
        long win = c.N + 1 + 3;
        for (size_t k = 0; k < faultCnt[s].size(); k++)
        {
            long fc = faultCnt[s][k];
            if (fc >= 0 && fc > incr[s] - win) { st.unassessed++; continue; }   // run ended before its alarm could come
            st.faults++;
            if (fc < 0) { st.silent++; continue; }
            bool al = false;
            for (size_t a = 0; a < alarmCnt[s].size() && !al; a++)
                if (alarmCnt[s][a] >= fc - win && alarmCnt[s][a] <= fc + win) al = true;
            if (!al) st.silent++;
        }
        // the owner's rule, counted independently of the code under test: no zero-flag alarm
        // before Shackles+1 counts since boot, then >= Shackles+1 counts between two alarms
        for (size_t a = 0; a < alarmCnt[s].size(); a++)
        {
            long prev = a ? alarmCnt[s][a-1] : 0;
            if (alarmCnt[s][a] - prev < c.N + 1) st.spacingViol++;
        }
    }
#ifdef XC_REAL_ERRQ
    CHECK(errq_end(o), "%s: the host did not get every controller message in order", c.name.c_str());
#endif
    free(shm); free(o); app = 0;
    return R;
}

//----- reporting -------------------------------------------------------------
struct Tot { int runs = 0, tabs = 0, miss = 0, fz = 0, silent = 0, alarms = 0, maxPerRev = 0, viol = 0, lost = 0; };
static std::map<std::string, Tot> g_tot;

static void addTot(const std::string& grp, const RunRes& r)
{
    Tot& t = g_tot[grp]; t.runs++;
    for (int s = 0; s < NSY; s++) if (r.active[s])
    {
        const SyncStats& st = r.s[s];
        t.tabs += st.tabsPassed; t.miss += st.missed; t.fz += st.accFalse; t.silent += st.silent;
        t.alarms += st.nd + st.early + st.late + st.mism;
        t.maxPerRev = (std::max)(t.maxPerRev, st.maxPerRev); t.viol += st.spacingViol;
        t.lost += st.tabsPassed - st.tabEdges;
    }
}

static void row(const Cfg& c, const RunRes& r, const RunRes* old, bool verbose)
{
    int tabs = 0, miss = 0, fz = 0, sil = 0, nd = 0, e = 0, l = 0, m = 0, mpr = 0, viol = 0, fzf = 99, lost = 0, boot = 0;
    for (int s = 0; s < NSY; s++) if (r.active[s])
    {
        const SyncStats& st = r.s[s];
        tabs += st.tabsPassed; miss += st.missed; fz += st.accFalse; sil += st.silent;
        nd += st.nd; e += st.early; l += st.late; m += st.mism; mpr = (std::max)(mpr, st.maxPerRev); viol += st.spacingViol;
        lost += st.tabsPassed - st.tabEdges; boot += st.bootSkipped;
        if (st.firstZeroFlag >= 0) fzf = (std::min)(fzf, st.firstZeroFlag);
    }
    printf("  %-58s tabs %4d miss %3d false %3d silent %2d | alarms ND %3d E %3d L %3d M %3d max/rev %d | 1st zero flag %d",
           c.name.c_str(), tabs, miss, fz, sil, nd, e, l, m, mpr, fzf == 99 ? -1 : fzf);
    if (lost) printf(" LOST %d", lost);
    if (viol) printf(" RULE-VIOL %d", viol);
    if (old)
    {
        int om = 0, of = 0, oa = 0;
        for (int s = 0; s < NSY; s++) if (old->active[s])
        { om += old->s[s].missed + old->s[s].bootSkipped; of += old->s[s].accFalse; oa += old->s[s].nd + old->s[s].early + old->s[s].late; }
        printf(" || 15.7.13: miss %d false %d alarms %d", om, of, oa);
    }
    printf("\n");
    if (verbose)
        for (int s = 0; s < NSY; s++) if (r.active[s])
        {
            const SyncStats& st = r.s[s];
            printf("      %-12s tabs %d miss %d false %d silent %d ND %d E %d L %d M %d init %d reacq %d max/rev %d minSpacing %ld firstAlarmAt %ld 1stZeroFlag %d",
                   syName[s], st.tabsPassed, st.missed, st.accFalse, st.silent, st.nd, st.early, st.late, st.mism, st.initial, st.reacq,
                   st.maxPerRev, st.minSpacing, st.firstAlarmCnt, st.firstZeroFlag);
            if (old) printf("  || 15.7.13 ND %d E %d L %d miss %d false %d", old->s[s].nd, old->s[s].early, old->s[s].late,
                            old->s[s].missed + old->s[s].bootSkipped, old->s[s].accFalse);
            printf("\n");
        }
}

static RunRes go(const std::string& grp, const Cfg& c, bool verbose = false)
{
    RunRes r = run(c, false);
    RunRes o; bool haveOld = c.runOld;
    if (haveOld) o = run(c, true);
    row(c, r, haveOld ? &o : 0, verbose);
    addTot(grp, r);
    // invariants that must hold in every scenario
    for (int s = 0; s < NSY; s++) if (r.active[s])
    {
        CHECK(r.s[s].spacingViol == 0, "%s %s: zero-flag alarms closer than one revolution of counts", c.name.c_str(), syName[s]);
        CHECK(r.s[s].maxPerRev <= 1, "%s %s: %d zero-flag alarms in one revolution", c.name.c_str(), syName[s], r.s[s].maxPerRev);
    }
    return r;
}
static void cleanCheck(const Cfg& c, const RunRes& r)
{
    for (int s = 0; s < NSY; s++) if (r.active[s])
    {
        const SyncStats& st = r.s[s];
        CHECK(st.missed == 0 && st.accFalse == 0, "%s %s: miss %d false %d", c.name.c_str(), syName[s], st.missed, st.accFalse);
        CHECK(st.nd + st.early + st.late + st.mism == 0, "%s %s: %d alarms on a clean line", c.name.c_str(), syName[s], st.nd + st.early + st.late + st.mism);
        CHECK(st.tabsPassed == st.tabEdges, "%s %s: %d tab pulses not seen by the debounce", c.name.c_str(), syName[s], st.tabsPassed - st.tabEdges);
        CHECK(st.firstZeroFlag == (SS_BOOT_CONFIRM ? 2 : 1), "%s %s: first zero at flag %d", c.name.c_str(), syName[s], st.firstZeroFlag);
    }
}

static std::vector<Knot> toggle(double a, double b, double period, double ramp, double dur, double t0 = 0)
{
    std::vector<Knot> k; k.push_back(Knot{0, a});
    double t = t0 + period; bool hi = false;
    while (t < dur) { double from = hi ? b : a, to = hi ? a : b; k.push_back(Knot{t, from}); k.push_back(Knot{t + ramp, to}); hi = !hi; t += period; }
    return k;
}

static bool want(int argc, char** argv, const char* g)
{
    if (argc < 2) return true;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], g)) return true;
    return false;
}

int main(int argc, char** argv)
{
    printf("SS_BOOT_CONFIRM=%d  band %d..%d permil  gap agree %d..%d%%  gate R-%d  (15.7.13 rails 30/375 ms Pitman, 30/250 chicken)\n",
           SS_BOOT_CONFIRM, SS_TAB_BAND_LO_PERMIL, SS_TAB_BAND_HI_PERMIL, SS_GAP_AGREE_LO_PCT, SS_GAP_AGREE_HI_PCT, SS_GATE_MARGIN);
    const double jits[4] = { 0, 0.05, 0.10, 0.15 };

    //=== Pitman steady: skip 1, 304 shackles, tab 0.42 T
    if (want(argc, argv, "pitman_steady"))
    {
        printf("\n== Pitman steady (skip 1, 304 shk, tab 0.42T, scale + drop sync)\n");
        const double sp[] = { 18, 20, 21, 25, 29, 33, 34, 40, 50, 55, 59, 65, 70, 72, 80 };
        for (double v : sp) for (int j = 0; j < 4; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM jitter %2.0f%%", v, jits[j] * 100);
            Cfg c = mk(nm, 304, 1, 0.42, v, 4.2); c.jit = jits[j]; c.seed = 11 + j; c.runOld = true;
            RunRes r = go("pitman_steady", c); cleanCheck(c, r);
        }
    }
    //=== Pitman speed steps / ramps
    if (want(argc, argv, "pitman_steps"))
    {
        printf("\n== Pitman speed changes: toggle every 97 s (steps land at all phases of the revolution)\n");
        const double pr[5][2] = { {21,55}, {21,72}, {29,59}, {33,70}, {54,21} };
        const double ramps[4] = { 0, 2, 5, 15 };
        for (int p = 0; p < 5; p++) for (int rr = 0; rr < 4; rr++) for (int j = 0; j < 4; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Pitman %2.0f<->%2.0f ramp %2.0fs jit %2.0f%%", pr[p][0], pr[p][1], ramps[rr], jits[j] * 100);
            Cfg c = mk(nm, 304, 1, 0.42, pr[p][0], 1);
            double slow = (std::min)(pr[p][0], pr[p][1]);
            c.dur = 7.0 * 304 / slow * 60.0 * 0.6;
            double tz = (c.startFrac + 1.0) * 304 / pr[p][0] * 60.0 + 30;   // after the first zero
            c.dur += tz;
            c.prof = toggle(pr[p][0], pr[p][1], 97, ramps[rr], c.dur, tz);
            c.jit = jits[j]; c.seed = 100 + p * 16 + rr * 4 + j; c.runOld = true;
            RunRes r = go(ramps[rr] >= 5 ? "pitman_ramp5_15" : "pitman_step0_2", c);
            if (ramps[rr] >= 5) cleanCheck(c, r);
        }
    }
    //=== Pitman adversarial: a speed change starting k trolleys before the flag reaches scale 1
    if (want(argc, argv, "pitman_steppos"))
    {
        printf("\n== Pitman adversarial step position: step starts k trolleys before the 3rd flag (after boot confirmation)\n");
        const double pr[5][2] = { {21,55}, {21,72}, {33,70}, {55,21}, {72,21} };
        const double ramps[2] = { 0, 2 };
        for (int p = 0; p < 5; p++) for (int rr = 0; rr < 2; rr++)
        {
            int fz = 0, ms = 0, sil = 0, al = 0; std::string bad;
            for (int k = -3; k <= 30; k++)
            {
                char nm[96]; snprintf(nm, sizeof nm, "step %2.0f->%2.0f ramp %.0fs k=%d", pr[p][0], pr[p][1], ramps[rr], k);
                Cfg c = mk(nm, 304, 1, 0.42, pr[p][0], 1);
                double L = 608;
                c.trigPos = 5 * L - k;                // scale-1 sensor position (trolleys) at the step
                c.trigSpm = pr[p][1]; c.trigRamp = ramps[rr];
                double tA = (c.trigPos - (3 - c.startFrac) * L) / (pr[p][0] * 2 / 60.0);
                c.dur = tA + 1.6 * 304 / pr[p][1] * 60.0;
                c.useDS1 = false;
                RunRes r = run(c, false); addTot("pitman_steppos", r);
                const SyncStats& st = r.s[SY_SC1];
                fz += st.accFalse; ms += st.missed; sil += st.silent; al += st.nd + st.early + st.late + st.mism;
                CHECK(st.spacingViol == 0 && st.maxPerRev <= 1, "%s: alarm rule violated", nm);
                if (st.accFalse || st.missed) { char b[64]; snprintf(b, sizeof b, " k=%d:%s%s%s", k, st.accFalse ? "F" : "", st.missed ? "M" : "", st.silent ? "(silent)" : ""); bad += b; }
            }
            printf("  step %2.0f->%2.0f ramp %.0fs, k=-3..30: false %d miss %d silent %d alarms %d |%s\n",
                   pr[p][0], pr[p][1], ramps[rr], fz, ms, sil, al, bad.c_str());
        }
    }
    //=== Pitman start / stop
    if (want(argc, argv, "pitman_stop"))
    {
        printf("\n== Pitman stop / start\n");
        for (int j = 0; j < 4; j++) for (int v = 0; v < 3; v++)
        {
            const double sp[3] = { 21, 33, 55 };
            char nm[96]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM, 30 s stop every 211 s (ramps 2 s), jit %2.0f%%", sp[v], jits[j] * 100);
            Cfg c = mk(nm, 304, 1, 0.42, sp[v], 1);
            double tz = (c.startFrac + 1.0) * 304 / sp[v] * 60.0 + 30;
            c.dur = tz + 3.5 * 304 / sp[v] * 60.0 * 1.15;
            c.prof.clear(); c.prof.push_back(Knot{0, sp[v]});
            for (double t = tz; t < c.dur; t += 211) { c.prof.push_back(Knot{t, sp[v]}); c.prof.push_back(Knot{t + 2, 0}); c.prof.push_back(Knot{t + 32, 0}); c.prof.push_back(Knot{t + 34, sp[v]}); }
            c.jit = jits[j]; c.seed = 300 + j * 3 + v; c.runOld = true;
            go("pitman_stop", c);
        }
        // the known residual: the chain stops with the flag in front of the scale-1 sensor
        const double where[5] = { 0.05, 0.21, 0.30, 0.47, 0.70 };
        const char*  what[5]  = { "on the body", "between body and tab", "between body and tab", "on the tab", "after the tab" };
        for (int w = 0; w < 5; w++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "stop 20 s at flag+%.2f (%s), 33 SPM", where[w], what[w]);
            Cfg c = mk(nm, 304, 1, 0.42, 33, 1);
            c.trigPos = 5 * 608 + where[w]; c.trigSpm = 0; c.trigRamp = 0; c.trigHold = 20; c.trigBack = 33; c.trigRamp2 = 0;
            c.dur = (c.trigPos - (3 - c.startFrac) * 608) / (33 * 2 / 60.0) + 20 + 1.5 * 304 / 33.0 * 60;
            c.useDS1 = false;
            go("pitman_stopflag", c, true);
        }
    }
    //=== Chicken: skip 0, 1189 shackles, tab 0.40
    if (want(argc, argv, "chicken"))
    {
        printf("\n== Chicken (skip 0, 1189 shk, tab 0.40T, body 0.18 tab 0.15)\n");
        const double sp[] = { 60, 80, 100, 120, 140, 160, 180, 200 };
        for (double v : sp) for (int j = 0; j < 4; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Chicken %3.0f SPM jitter %2.0f%%", v, jits[j] * 100);
            Cfg c = mk(nm, 1189, 0, 0.40, v, 4.2); c.wb = 0.18; c.wt = 0.15; c.railMax = 250;
            c.jit = jits[j]; c.seed = 500 + j; c.runOld = true;
            RunRes r = go("chicken_steady", c); cleanCheck(c, r);
        }
        const double pr[2][2] = { {140,180}, {60,180} };
        const double ramps[4] = { 0, 2, 5, 15 };
        for (int p = 0; p < 2; p++) for (int rr = 0; rr < 4; rr++) for (int j = 0; j < 4; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Chicken %3.0f<->%3.0f ramp %2.0fs jit %2.0f%%", pr[p][0], pr[p][1], ramps[rr], jits[j] * 100);
            Cfg c = mk(nm, 1189, 0, 0.40, pr[p][0], 1); c.wb = 0.18; c.wt = 0.15; c.railMax = 250;
            c.dur = 6.0 * 1189 / pr[p][0] * 60.0 * 0.8;
            double tz = (c.startFrac + 1.0) * 1189 / pr[p][0] * 60.0 + 20;
            c.dur += tz;
            c.prof = toggle(pr[p][0], pr[p][1], 61, ramps[rr], c.dur, tz);
            c.jit = jits[j]; c.seed = 600 + p * 16 + rr * 4 + j; c.runOld = true;
            RunRes r = go(ramps[rr] >= 5 ? "chicken_ramp5_15" : "chicken_step0_2", c);
            if (ramps[rr] >= 5) cleanCheck(c, r);
        }
    }
    //=== Alarm rule: missing flag (sensor fault) on three syncs; noise flood
    if (want(argc, argv, "alarms"))
    {
        printf("\n== Alarm rule (per sync, once per revolution): scale + drop sync 9 trolleys apart + grade sync\n");
        {
            // boot before flag 3; first zero (boot confirmation) at flag 4; flags 5..10 missing
            Cfg c = mk("flag missing for 6 revolutions after the first zero, 21 SPM, grading", 304, 1, 0.42, 21, 10.5);
            c.offDS1 = 9.37; c.grading = true; c.missFrom = 5; c.missTo = 11; c.runOld = true;
            RunRes r = go("alarms", c, true);
            for (int s = 0; s < NSY; s++) if (r.active[s])
            {
                CHECK(r.s[s].nd >= 5, "missing flag: %s raised only %d 'NOT Detected' over 6 missing revolutions", syName[s], r.s[s].nd);
                CHECK(r.s[s].reacq == 1, "missing flag: %s announced re-acquired %d times", syName[s], r.s[s].reacq);
            }
        }
        {
            Cfg c = mk("flag missing for 6 revolutions after the first zero, 55 SPM, grade sync 2", 304, 1, 0.42, 55, 10.5);
            c.offDS1 = 9.37; c.grading = true; c.gs2 = true; c.missFrom = 5; c.missTo = 11; c.runOld = true;
            RunRes r = go("alarms", c, true);
            for (int s = 0; s < NSY; s++) if (r.active[s])
            {
                CHECK(r.s[s].nd >= 5, "missing flag: %s raised only %d 'NOT Detected' over 6 missing revolutions", syName[s], r.s[s].nd);
                CHECK(r.s[s].reacq == 1, "missing flag: %s announced re-acquired %d times", syName[s], r.s[s].reacq);
            }
        }
        {
            Cfg c = mk("noise: 2% of trolleys carry a tab-like pulse, 33 SPM, grading", 304, 1, 0.42, 33, 8);
            c.offDS1 = 9.37; c.grading = true; c.bounce = 0.02; c.runOld = true;
            RunRes r = go("alarms", c, true);
            for (int s = 0; s < NSY; s++) if (r.active[s])     // liveness under heavy noise: it must zero
                CHECK(r.s[s].firstZeroFlag >= 0 || r.s[s].firstZeroFalse, "2%% noise: %s never zeroed", syName[s]);
        }
        {
            Cfg c = mk("noise: 0.5% bounce, chicken 140 SPM", 1189, 0, 0.40, 140, 8); c.wb = 0.18; c.wt = 0.15; c.railMax = 250;
            c.bounce = 0.005; c.runOld = true;
            go("alarms", c, true);
        }
        {
            Cfg c = mk("flag missing from boot (never seen), 33 SPM", 304, 1, 0.42, 33, 5);
            c.missFrom = 0; c.missTo = 1000; c.runOld = true;
            RunRes r = go("alarms", c, true);
            for (int s = 0; s < NSY; s++) if (r.active[s])
                CHECK(r.s[s].nd >= 4, "never-seen flag: %s raised only %d 'NOT Detected' in 5 revolutions", syName[s], r.s[s].nd);
        }
    }
    //=== Boot with noise: is the first zero ever a false one?
    if (want(argc, argv, "boot"))
    {
        printf("\n== Boot with noise (tab-shaped bounce pulses at the flag's own 0.42), 24 boots x 2 speeds per level\n");
        const double lv[4] = { 0.0005, 0.001, 0.003, 0.01 };
        for (int li = 0; li < 4; li++)
        {
            int fz = 0, boots = 0, late = 0, firstFalse = 0, never = 0;
            for (int b = 0; b < 24; b++) for (int v = 0; v < 2; v++)
            {
                const double sp[2] = { 25, 60 };
                char nm[96]; snprintf(nm, sizeof nm, "boot %d, %2.0f SPM, noise %.2f%%", b, sp[v], lv[li] * 100);
                Cfg c = mk(nm, 304, 1, 0.42, sp[v], 3.3);
                c.startFrac = 0.04 + 0.04 * b; c.bounce = lv[li]; c.seed = 900 + li * 100 + b * 2 + v; c.useDS1 = false;
                RunRes r = run(c, false); addTot("boot", r); boots++;
                fz += r.s[SY_SC1].accFalse; if (r.s[SY_SC1].firstZeroFlag != (SS_BOOT_CONFIRM ? 2 : 1)) late++;
                if (r.s[SY_SC1].firstZeroFalse) firstFalse++;
                if (r.s[SY_SC1].silent) { printf("    silent fault(s):"); row(c, r, 0, true); }
                if (r.s[SY_SC1].firstZeroFlag < 0 && !r.s[SY_SC1].firstZeroFalse) never++;
                CHECK(r.s[SY_SC1].spacingViol == 0, "%s: alarm rule", nm);
            }
            printf("  noise %.2f%% of trolleys (%.1f per revolution): %d boots, FIRST zero false %d, never zeroed %d, first zero not at flag %d: %d;"
                   " false zeros in the steady running that followed: %d\n",
                   lv[li] * 100, lv[li] * 608, boots, firstFalse, never, SS_BOOT_CONFIRM ? 2 : 1, late, fz - firstFalse);
            CHECK(never == 0, "noise %.2f%%: %d boots never zeroed", lv[li] * 100, never);
        }
    }
    //=== Any plant: skip 0/1, 100..1500 shackles, 10..220 SPM, tab 0.25..0.50, jitter
    const int Ns[5] = { 100, 304, 600, 1189, 1500 };
    for (int skip = 0; skip < 2; skip++) for (int ni = 0; ni < 5; ni++)
    {
        char gname[32]; snprintf(gname, sizeof gname, "anyplant_s%d_%d", skip, Ns[ni]);
        if (!want(argc, argv, gname)) continue;
        printf("\n== Any plant: steady speed grid, %d shackles, skip %d (scale + drop sync; failing speeds listed as SPM:misses/falses)\n", Ns[ni], skip);
        const double fs[5] = { 0.25, 0.30, 0.35, 0.42, 0.50 };
        const double js[3] = { 0, 0.10, 0.15 };
        const double sp0[7] = { 10, 30, 60, 100, 140, 180, 220 };
        const double sp1[6] = { 10, 20, 33, 50, 72, 100 };
        for (int fi = 0; fi < 5; fi++) for (int ji = 0; ji < 3; ji++)
        {
            int tabs = 0, miss = 0, fz = 0, al = 0, lost = 0; std::string bad;
            const double* sp = skip ? sp1 : sp0; int nsp = skip ? 6 : 7;
            for (int si = 0; si < nsp; si++)
            {
                char nm[96]; snprintf(nm, sizeof nm, "N%d skip%d f%.2f j%.0f %gSPM", Ns[ni], skip, fs[fi], js[ji] * 100, sp[si]);
                Cfg c = mk(nm, Ns[ni], skip, fs[fi], sp[si], 3.3);
                c.wb = 0.15; c.wt = 0.12; c.jit = js[ji]; c.seed = 2000 + skip * 1000 + ni * 100 + fi * 10 + ji;
                RunRes r = run(c, false); addTot(fs[fi] < 0.27 ? "anyplant_f25" : "anyplant_f30_50", r);
                for (int s = 0; s < NSY; s++) if (r.active[s])
                {
                    const SyncStats& st = r.s[s];
                    tabs += st.tabsPassed; miss += st.missed; fz += st.accFalse; al += st.nd + st.early + st.late + st.mism;
                    lost += st.tabsPassed - st.tabEdges;
                    CHECK(st.spacingViol == 0 && st.maxPerRev <= 1, "%s: alarm rule", nm);
                    if (fs[fi] >= 0.30 && js[ji] <= 0.10)
                        CHECK(st.missed == 0 && st.accFalse == 0 && st.firstZeroFlag == (SS_BOOT_CONFIRM ? 2 : 1),
                              "%s %s: miss %d false %d first %d", nm, syName[s], st.missed, st.accFalse, st.firstZeroFlag);
                }
                int m2 = 0, f2 = 0; for (int s = 0; s < NSY; s++) if (r.active[s]) { m2 += r.s[s].missed; f2 += r.s[s].accFalse; }
                if (m2 || f2) { char b[48]; snprintf(b, sizeof b, " %gSPM:%dm/%df", sp[si], m2, f2); bad += b; }
            }
            printf("  N%-4d skip %d tab %.2f jitter %2.0f%%: tabs %4d miss %3d false %3d alarms %3d%s%s\n",
                   Ns[ni], skip, fs[fi], js[ji] * 100, tabs, miss, fz, al, lost ? " LOST" : "", bad.c_str());
        }
    }
    if (want(argc, argv, "anyplant_3x"))
    {
        printf("\n== Any plant: 3x speed steps every ~0.3 revolution, instant and 5 s ramp (tab 0.42, jitter 10%%)\n");
        for (int skip = 0; skip < 2; skip++) for (int ni = 0; ni < 5; ni += 2) for (int rr = 0; rr < 2; rr++)
        {
            const double base[2][3] = { {20, 60, 70}, {10, 25, 33} };
            for (int bi = 0; bi < 3; bi++)
            {
                double a = base[skip][bi], b = a * 3;
                char nm[96]; snprintf(nm, sizeof nm, "N%d skip%d %g<->%g SPM ramp %ds", Ns[ni], skip, a, b, rr ? 5 : 0);
                Cfg c = mk(nm, Ns[ni], skip, 0.42, a, 1); c.wb = 0.15; c.wt = 0.12; c.jit = 0.10;
                double revA = Ns[ni] / a * 60.0;
                c.dur = 6.0 * revA * 0.7;
                c.prof = toggle(a, b, revA * 0.3 + 7.3, rr ? 5 : 0, c.dur, revA * 2.2);
                c.seed = 4000 + skip * 100 + ni * 10 + rr * 3 + bi;
                go(rr ? "anyplant_3x_ramp5" : "anyplant_3x_step", c);
            }
        }
    }

    printf("\n== Totals (NEW, all syncs)\n");
    for (std::map<std::string, Tot>::iterator it = g_tot.begin(); it != g_tot.end(); ++it)
    {
        const Tot& t = it->second;
        printf("  %-20s runs %4d  tabs %6d  missed %4d  false %4d  silent %3d  alarms %4d  max alarms/rev/sync %d  rule viol %d%s\n",
               it->first.c_str(), t.runs, t.tabs, t.miss, t.fz, t.silent, t.alarms, t.maxPerRev, t.viol, t.lost ? "  (tab pulses lost by debounce!)" : "");
    }
#ifdef XC_REAL_ERRQ
    errq_report();
#endif
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
