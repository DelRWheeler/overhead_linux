//--------------------------------------------------------------------------
//  sszero_crosscheck_test.cpp - single-sensor cross-sync count check (15.7.16)
//
//  Build + run:   linux_port/tools/sszero_crosscheck_test.sh [group ...]
//  (the script cuts the REAL ProcessSyncs / GradeSyncs / SingleSensor* / SingleSensorXC*
//   out of the working tree (NEW), ProcessSyncs out of b269cf8 = 15.7.15 as deployed at
//   Pitman (OLD) and out of f9f2968 = 15.7.13 (OLD13), plus two test-only mutants:
//   SPEC = the literal spec verdict (symmetric 2-of-3 majority), NOTRACK = no offset
//   tracking; and compiles this file against the controller headers.)
//
//  REAL   the Pitman Sensor Scope captures (5 ms raw count bytes, 2026-10-05) are replayed
//         scan by scan. Ground truth per sync: every debounced edge is a trolley, except the
//         tab edges (the field controller's own zero marks, all verified physical tabs) and
//         the re-counted bodies found by hand from the raw pulses (rollback: the input fell
//         during a stop and rose again on restart) - listed in kReal[] with the evidence.
//         The truth is checked: flag to flag it must be exactly 606 trolleys.
//         Start state = the field's: counts exact at the first scan (verified: OLD then
//         reproduces the field alarms), alarm budgets from the field log.
//  SYNTH  a chain model: trolley k at chain position k + pitch profile (smooth, so the
//         inter-sensor offset wanders +/-0.2 trolley as measured at Pitman) + jitter; flag
//         tab on trolley 0; sensors at fixed positions; speed profiles with steps, ramps,
//         stops; at a stop the chain ROLLS BACK r trolleys, so any sensor sitting on a body
//         re-counts it on restart (the real mechanism, not an injected count); injected
//         extra pulses and suppressed bodies. Truth = the trolley under the sensor.
//
//  For every run: error of each sync's count against the truth at every counted edge
//  (episodes: start, end, size, how it ended), corrections (right = moved an error +1 to 0;
//  FALSE = anything else), disagreement alarms, zero alarms, and the pass accounting: every
//  missed-bird pass (drop section) per revolution must cover each shackle exactly once, and
//  the scale's weigh triggers.
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
#include <time.h>

#define ProcessSyncs ProcessSyncs(); void ProcessSyncs_OLD(); void ProcessSyncs_SPEC(); void ProcessSyncs_NOTRACK(); \
    void ProcessSyncs_OLD13(); bool SingleSensorIsZeroTab_OLD13(__int64 &lastTrolleyTick, __int64 &interval, int &stall, int &tabRun, int &shrink); \
    bool SingleSensorWarnOk_OLD13(); __int64 ss_last_warn_tick; \
    int SingleSensorXCCheck_NOTRACK(int i, int numSyncs); int SpecXCCheck(int i, int numSyncs); int spec_vk[MAXSYNCS]; int spec_vcnt[MAXSYNCS]; \
    void ProcessSyncs_NEW
#define private public
#include "types.h"
#undef private
#undef ProcessSyncs

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
int  RtPrintf(const char*, ...) { return 0; }
void DebugTrace(UINT, char*, ...) {}
#define _FILE_ "overhead.cpp"
typedef app_type::SHARE_MEMORY SHARE_MEMORY;
#define RtReadPortUchar(a) ((UCHAR) 0)

struct Msg { long tick; std::string txt; };
struct MBRec { long tick; int k; int shk; };
static std::vector<Msg>*   g_msgs = 0;
static std::vector<MBRec>* g_mb   = 0;
static long                g_tick = 0;
void overhead::SetOutput(int, DBOOL)                 {}
bool overhead::MissedBirdCheck(int k, int shk, int)  { if (g_mb) g_mb->push_back(MBRec{g_tick, k, shk}); return false; }
void overhead::AddDropRecord(int)                    {}
void overhead::GenError(int, char* txt)              { g_msgs->push_back(Msg{g_tick, txt}); }
void overhead::GradeProcess(int)                     {}
int  overhead::SendLineMsg(int, int, int, BYTE*, int) { return 0; }

#include "xc_shared.inc"
#include "xc_ps.inc"
#include "xc_old13.inc"

//----- SPEC: the literal spec verdict, for comparison only ------------------------------
// "correct sync i if, for at least 2 other syncs, |e_ij - k| < 0.3 with the same integer
//  k = +/-1, and those other syncs agree with each other (|e_jl| < 0.35); same verdict on 2
//  consecutive counted edges; suspended unless steady". Offsets / tracking / alarms / the
//  steadiness bookkeeping are the real 15.7.16 ones (SingleSensorXCCheck runs first); only
//  the verdict differs: symmetric, no anchor rule, no stop rule.
int overhead::SpecXCCheck(int i, int numSyncs)
{
    SingleSensorXCCheck(i, numSyncs);
    ss_xc_vcnt[i] = 0;
    int v = 0;
    if (SingleSensorXCEligible(i, numSyncs) && SingleSensorXCSteady(i))
    {
        std::vector<int> others; bool allSteady = true;
        for (int j = 0; j < numSyncs; j++)
        {
            if (j == i || !SingleSensorXCEligible(j, numSyncs)) continue;
            if (ss_xc_pair[i < j ? i : j][i < j ? j : i].state != 2) continue;
            if (!SingleSensorXCSteady(j)) allSteady = false;
            others.push_back(j);
        }
        for (int k = 1; k >= -1 && allSteady; k -= 2)
        {
            std::vector<int> J;
            for (int j : others) if (fabs(SingleSensorXCOffset(i, j) - k) < 0.3) J.push_back(j);
            if (J.size() < 2) continue;
            bool agree = true;
            for (size_t a = 0; a < J.size(); a++) for (size_t b = a + 1; b < J.size(); b++)
                if (ss_xc_pair[J[a] < J[b] ? J[a] : J[b]][J[a] < J[b] ? J[b] : J[a]].state != 2 ||
                    fabs(SingleSensorXCOffset(J[a], J[b])) >= 0.35) agree = false;
            if (agree) { v = k; break; }
        }
    }
    if (v && v == spec_vk[i]) spec_vcnt[i]++; else { spec_vk[i] = v; spec_vcnt[i] = v ? 1 : 0; }
    return spec_vcnt[i] >= SS_XC_HYST_EDGES ? -v : 0;
}

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static inline double urand(unsigned long long k, unsigned long long salt)
{
    unsigned long long z = k * 0x9E3779B97F4A7C15ULL + salt * 0xBF58476D1CE4E5B9ULL + 0x94D049BB133111EBULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return (double)(z >> 11) / 9007199254740992.0;
}

enum Variant { V_OLD13, V_OLD, V_NEW, V_SPEC, V_NOTRACK };
static const char* vName[] = { "15.7.13", "15.7.15", "15.7.16", "SPEC-rule", "NOTRACK" };

//===========================================================================================
//  One run: a scan source + truth, the controller under test, the bookkeeping
//===========================================================================================
struct Source
{
    int N, skip, syncOn;
    std::vector<int> bits;                         // count syncs in this line (sync index = input bit)
    long nScans;
    virtual unsigned char input(long n) = 0;       // raw count byte at scan n
    // truth at a counted edge of sync bit b, scan n: >= 0 absolute trolley index (mod L taken by the
    // caller), TRUTH_TAB the flag tab, TRUTH_SAME no new trolley (re-counted body / noise pulse)
    virtual long truth(int b, long n) = 0;
    virtual ~Source() {}
};
enum { TRUTH_TAB = -1, TRUTH_SAME = -2 };

struct InitState { bool warm; long tau[MAXSYNCS]; int alarmCnt[MAXSYNCS]; bool zeroLost[MAXSYNCS]; };

struct Episode { int sync; long start, end; int maxErr; int edges; std::string how; int lastErr; };
struct Corr    { long tick; int sync; int errAfter; bool right; };
struct RunOut
{
    std::vector<Msg> msgs;
    std::vector<Episode> eps;
    std::vector<Corr> corr;
    int corrRight = 0, corrFalse = 0, disagree = 0, zeroAlarms = 0;
    long errEdges[MAXSYNCS] = {0}, edges[MAXSYNCS] = {0};
    std::vector<std::pair<long,int>> zeros;        // accepted tabs (tick, sync)
    std::vector<unsigned long long> stateHash;     // per-scan counter hash (identity checks)
    int mbDup = 0, mbMiss = 0, mbRevs = 0, weighRelabel = 0, weighDup = 0;
    int mbCorrRevs = 0, mbCorrDup = 0, mbCorrMiss = 0;   // revolutions with a correction and no overrun
    int alarmSpacingViol = 0;                      // disagreement alarms for one pair closer than a revolution
    double maxFracSteady = 0; long pairChecks = 0, pairWrongInt = 0, pairWrong1 = 0;   // armed-pair measurement quality
                                                   // (WrongInt: wrong at 2 consecutive checks of the pair; Wrong1: single-shot)
    bool seeded = false; long seedTick = -1;
    int armedAt[MAXSYNCS][MAXSYNCS];
};

static const char* g_debug = getenv("XCDEBUG");
static const char* g_edge  = getenv("XCEDGE");      // "sync:t0:t1" - trace that sync's counted edges (15.7.16 runs)
static int    g_edgeSync = g_edge ? atoi(g_edge) : -1;
static double g_edgeT0 = g_edge && strchr(g_edge, ':') ? atof(strchr(g_edge, ':') + 1) : 0;
static double g_edgeT1 = g_edge && strrchr(g_edge, ':') ? atof(strrchr(g_edge, ':') + 1) : 0;

static RunOut runOne(Source& src, Variant var, const InitState& init, bool seedOracle, bool grading = false)
{
    RunOut R; g_msgs = &R.msgs;
    std::vector<MBRec> mb; g_mb = &mb;
    overhead*     o   = (overhead*)     calloc(1, sizeof(overhead));
    SHARE_MEMORY* shm = (SHARE_MEMORY*) calloc(1, sizeof(SHARE_MEMORY));
    app = o; o->pShm = shm;
    memset(trc, 0, sizeof(trc)); memset(trc_buf, 0, sizeof(trc_buf));
    for (int a = 0; a < MAXSYNCS; a++) for (int b = 0; b < MAXSYNCS; b++) R.armedAt[a][b] = -1;

    const int N = src.N, S1 = src.skip + 1;
    const long L = (long) N * S1;
    shm->sys_set.Shackles     = N;
    shm->sys_set.SkipTrollies = src.skip;
    shm->sys_set.SyncOn       = src.syncOn;
    shm->sys_set.Grading      = grading;
    shm->scl_set.NumScales    = 1;
    shm->ZeroFlagMode         = 1;
    shm->ZeroTabWindowMinMs   = 30;
    shm->ZeroTabWindowMaxMs   = 375;
    shm->OpMode               = ModeRun;
    int drop = 1;
    for (int b : src.bits) if (b >= 2) { shm->sys_set.SyncSettings[b].first = drop; shm->sys_set.SyncSettings[b].last = drop; drop++; }
    // missed-bird pass on the first two drop syncs (pass accounting)
    int mbSync[2] = { -1, -1 }, nmb = 0;
    for (int b : src.bits) if (b >= 2 && nmb < 2) mbSync[nmb++] = b;
    shm->sys_set.MBSync[0] = mbSync[0] >= 0 ? mbSync[0] - 1 : 7; shm->sys_set.MBOffset[0] = 5;
    shm->sys_set.MBSync[1] = mbSync[1] >= 0 ? mbSync[1] - 1 : 7; shm->sys_set.MBOffset[1] = 7;
    o->this_lineid = 1; o->dual_scale = false; o->syncOffset = 0;
    for (int i = 0; i < MAXSYNCS; i++)
    {
        o->sync_armed[i] = true; o->sync_debounce[i] = src.syncOn;
        o->ss_tab_run[i] = (var == V_OLD13) ? SS_MIN_TROLLEYS_BETWEEN_TABS : 0;
    }
    for (int g = 0; g < MAXGRADESYNCS; g++)
    { o->grade_armed[g] = false; o->grade_zeroed[g] = false; o->grade_debounce[g] = src.syncOn; o->ss_grade_tab_run[g] = 0; }
    o->ss_scan_tick = 0; o->ss_last_warn_tick = -100000; o->shk2shk_ticks = 50;
    unsigned char in0 = src.input(0);
    long tau[MAXSYNCS];
    bool zeroedTruth[MAXSYNCS] = {false};
    for (int b : src.bits)
    {
        if ((in0 >> b) & 1) o->sync_armed[b] = false;          // mid-pulse at scan 0: already counted
        tau[b] = 0;
        if (init.warm)
        {
            long t = init.tau[b];
            tau[b] = t; zeroedTruth[b] = true;
            shm->SyncStatus[b].shackleno = (int) (t / S1) + 1;
            o->trolly_counters[b]        = (int) (t % S1);
            o->true_shackle_count[b]     = shm->SyncStatus[b].shackleno;
            shm->SyncStatus[b].zeroed    = true;
            o->ss_rule[b].confirmed      = true;
            o->ss_tab_run[b]             = (int) t;
            o->ss_alarm_cnt[b]           = init.alarmCnt[b];
            o->ss_zero_lost[b]           = init.zeroLost[b];
        }
    }
    if (init.warm) o->previousShackle1 = shm->SyncStatus[0].shackleno;
    for (int i = 0; i < MAXSYNCS; i++) { o->spec_vk[i] = 0; o->spec_vcnt[i] = 0; }

    // bookkeeping
    int  curErr[MAXSYNCS] = {0}; long epStart[MAXSYNCS]; int epMax[MAXSYNCS] = {0}, epEdges[MAXSYNCS] = {0};
    for (int i = 0; i < MAXSYNCS; i++) epStart[i] = -1;
    bool everZero[MAXSYNCS] = {false};
    bool lastSame[MAXSYNCS] = {false};       // the sync's latest edge was not a trolley (noise / re-count)
    bool pairWasWrong[MAXSYNCS][MAXSYNCS] = {{false}};
    long lastAlarmEdges[MAXSYNCS][MAXSYNCS]; for (int a = 0; a < MAXSYNCS; a++) for (int b = 0; b < MAXSYNCS; b++) lastAlarmEdges[a][b] = -1;
    for (int b : src.bits) if (init.warm) everZero[b] = true;
    std::map<int, std::vector<int>> revMB[2]; int revIdx[2] = {0, 0};
    std::vector<int> curRevMB[2]; bool mbStarted[2] = {false, false};
    bool revCorr[2] = {false, false}, revOver[2] = {false, false};
    std::map<int,int> weighCnt; bool weighStarted = false;
    const bool dbg = g_debug != 0;

    auto wrapL = [&](long x) { x %= L; if (x < 0) x += L; if (x > L / 2) x -= L; return (int) x; };
    auto closeRev = [&](int k)
    {
        if (!mbStarted[k]) { mbStarted[k] = true; curRevMB[k].clear(); return; }
        std::map<int,int> c; for (int v : curRevMB[k]) c[v]++;
        bool cr = revCorr[k] && !revOver[k];
        for (int s = 1; s <= N; s++)
        {
            int n = c.count(s) ? c[s] : 0;
            if (n > 1) { R.mbDup += n - 1; if (cr) R.mbCorrDup += n - 1; }
            if (n == 0) { R.mbMiss++; if (cr) R.mbCorrMiss++; }
        }
        R.mbRevs++; if (cr) R.mbCorrRevs++; curRevMB[k].clear(); revCorr[k] = revOver[k] = false;
    };

    for (long n = 1; n < src.nScans; n++)
    {
        g_tick = n;
        o->ss_scan_tick++;
        memset(o->sync_in, 0, sizeof(o->sync_in)); memset(o->sync_zero, 0, sizeof(o->sync_zero));
        o->switch_in[0] = 0;
        o->sync_in[0] = (char) src.input(n);
        bool armedPre[MAXSYNCS]; int shkPre[MAXSYNCS];
        for (int b : src.bits) { armedPre[b] = o->sync_armed[b]; shkPre[b] = shm->SyncStatus[b].shackleno; }
        size_t msgPre = R.msgs.size(), mbPre = mb.size();
        o->weigh_state[0] = WeighIdle;
        if (grading) o->GradeSyncs();
        switch (var)
        {
            case V_OLD13:   o->ProcessSyncs_OLD13();   break;
            case V_OLD:     o->ProcessSyncs_OLD();     break;
            case V_NEW:     o->ProcessSyncs_NEW();     break;
            case V_SPEC:    o->ProcessSyncs_SPEC();    break;
            case V_NOTRACK: o->ProcessSyncs_NOTRACK(); break;
        }
        // weigh triggers (scale block)
        if (o->weigh_state[0] == WeighActive)
        {
            int ws = shm->WeighShackle[0];
            if (weighStarted) weighCnt[ws]++;
        }
        // corrections this scan
        std::set<int> corrected;
        for (size_t m = msgPre; m < R.msgs.size(); m++)
        {
            const std::string& t = R.msgs[m].txt;
            if (dbg) printf("    [%s] t=%ld %s", vName[var], n, t.c_str());
            if (t.compare(0, 16, "Count corrected:") == 0)
            {
                for (int b : src.bits) if (t.compare(17, strlen(sync_desc[b]), sync_desc[b]) == 0 && t[17 + strlen(sync_desc[b])] == ' ') corrected.insert(b);
            }
            else if (t.compare(0, 19, "Count disagreement:") == 0)
            {
                R.disagree++;
                int a = -1, b = -1;
                for (int q : src.bits) { std::string h = std::string("Count disagreement: ") + sync_desc[q] + " vs "; if (t.compare(0, h.size(), h) == 0) a = q; }
                for (int q : src.bits) { std::string h = std::string(" vs ") + sync_desc[q] + " by "; if (t.find(h) != std::string::npos) b = q; }
                if (a >= 0 && b >= 0)
                {
                    long ed = o->ss_xc_edges[a];
                    long& last = lastAlarmEdges[a][b];
                    if (last >= 0 && ed - last < L) R.alarmSpacingViol++;
                    last = ed;
                }
            }
            else if (t.find("Zero Flag NOT") != std::string::npos || t.find("Early Zero") != std::string::npos ||
                     t.find("Late Zero") != std::string::npos || t.find("mismatch") != std::string::npos) R.zeroAlarms++;
        }
        // per-sync counted edges vs truth
        for (int b : src.bits)
        {
            bool edge = armedPre[b] && !o->sync_armed[b];
            if (!edge) continue;
            bool acc = o->ss_last_trolley_tick[b] != o->ss_scan_tick && shm->SyncStatus[b].shackleno == 1 && shm->SyncStatus[b].zeroed;
            if (!acc && shkPre[b] > N && shm->SyncStatus[b].shackleno == 1)
                for (int k = 0; k < 2; k++) if (mbSync[k] == b) revOver[k] = true;
            if (acc) R.zeros.push_back(std::make_pair(n, b));
            long tr = src.truth(b, n);
            lastSame[b] = (tr == TRUTH_SAME);
            if (tr >= 0) tau[b] = tr % L;
            else if (tr == TRUTH_TAB) { tau[b] = 0; zeroedTruth[b] = true; }
            if (acc) everZero[b] = true;
            if (!shm->SyncStatus[b].zeroed || !everZero[b] || !zeroedTruth[b]) continue;
            long T = (long) (shm->SyncStatus[b].shackleno - 1) * S1 + o->trolly_counters[b];
            int err = wrapL(T - tau[b]);
            if (g_edge && var == V_NEW && b == g_edgeSync && n * 0.005 >= g_edgeT0 && n * 0.005 <= g_edgeT1)
                printf("    edge t=%.2fs %s shk %d/%d tau %ld err %+d tr %ld acc %d steady %d anchor %d clean %d vcnt %d gap %lld/%lld dist open %d pre %d zero %d mask %x lineok %d\n",
                       n * 0.005, sync_desc[b], shm->SyncStatus[b].shackleno, o->trolly_counters[b], tau[b], err, tr, acc, o->ss_xc_steady[b], o->ss_xc_anchor[b],
                       o->ss_xc_clean[b], o->ss_xc_vcnt[b], (long long) o->ss_rule[b].gap, (long long) o->ss_rule[b].gapPrev, o->ss_xc_dist_open, o->ss_xc_dist_pre_ok,
                       o->ss_xc_dist_zero, o->ss_xc_dist_mask, o->ss_xc_line_ok);
            R.edges[b]++;
            if (err) R.errEdges[b]++;
            if (corrected.count(b))
            {
                // right = it moved the count TOWARDS the truth (a common-mode error of every sync
                // - all re-counted at one stop - is invisible to any cross-check and stays)
                bool right = abs(err) < abs(curErr[b]);
                if (getenv("XCCORRDBG"))
                {
                    printf("    corr t=%.2fs %s err %+d -> %+d %s | errs:", n * 0.005, sync_desc[b], curErr[b], err, right ? "right" : "FALSE");
                    for (int q : src.bits) printf(" %s %+d", sync_desc[q], q == b ? err : curErr[q]);
                    printf(" | dist open %d pre %d zero %d mask %x anchors", o->ss_xc_dist_open, o->ss_xc_dist_pre_ok, o->ss_xc_dist_zero, o->ss_xc_dist_mask);
                    for (int q : src.bits) printf(" %d", o->ss_xc_anchor[q]);
                    printf(" | e:");
                    for (int q : src.bits) for (int r2 : src.bits) if (r2 > q) printf(" %d-%d %.2f", q, r2, o->SingleSensorXCOffset(q, r2));
                    printf("\n");
                }
                if (mbSync[0] == b) revCorr[0] = true;
                if (mbSync[1] == b) revCorr[1] = true;
                R.corr.push_back(Corr{n, b, err, right});
                if (right) R.corrRight++; else R.corrFalse++;
            }
            // episodes
            if (err != 0 && curErr[b] == 0) { epStart[b] = n; epMax[b] = 0; epEdges[b] = 0; }
            if (err != 0) { epEdges[b]++; if (abs(err) > abs(epMax[b])) epMax[b] = err; }
            if (err == 0 && curErr[b] != 0)
            {
                std::string how = corrected.count(b) ? "corrected" : acc ? "zero" : (shkPre[b] > N && shm->SyncStatus[b].shackleno == 1) ? "overrun" : "other";
                R.eps.push_back(Episode{b, epStart[b], n, epMax[b], epEdges[b], how, curErr[b]});
            }
            curErr[b] = err;
        }
        // missed-bird pass accounting: per revolution of that sync (zero to zero), each shackle once
        for (size_t m = mbPre; m < mb.size(); m++) curRevMB[mb[m].k].push_back(mb[m].shk);
        for (int k = 0; k < 2; k++)
        {
            int b = mbSync[k]; if (b < 0) continue;
            for (auto& z : R.zeros) if (z.first == n && z.second == b) closeRev(k);
        }
        for (auto& z : R.zeros) if (z.first == n && z.second == 0)
        {
            if (weighStarted) for (auto& w : weighCnt) { if (w.second > 1) R.weighDup += w.second - 1; }
            weighCnt.clear(); weighStarted = true;
        }
        // armed-pair measurement quality vs truth (NEW / NOTRACK)
        if (var >= V_NEW)
        {
            for (int a : src.bits) for (int b : src.bits)
            {
                if (b <= a) continue;
                SSXCPair& p = o->ss_xc_pair[a][b];
                if (p.state == 2 && R.armedAt[a][b] < 0) R.armedAt[a][b] = (int) n;
                if (p.state != 2 || !o->SingleSensorXCSteady(a) || !o->SingleSensorXCSteady(b) ||
                    !shm->SyncStatus[a].zeroed || !shm->SyncStatus[b].zeroed) continue;
                if (!(o->ss_last_trolley_tick[a] == o->ss_scan_tick)) continue;   // at a's counted edges
                if (lastSame[a] || lastSame[b]) continue;   // at a noise / re-count edge the truth counts it before the position shows it
                double e = o->SingleSensorXCOffset(a, b);
                int k = (int) lround(e);
                double f = fabs(e - k);
                R.pairChecks++;
                if (f > R.maxFracSteady) R.maxFracSteady = f;
                bool wrong = (k != curErr[a] - curErr[b] && f < 0.3);
                if (wrong) { if (pairWasWrong[a][b]) R.pairWrongInt++; else R.pairWrong1++; }
                pairWasWrong[a][b] = wrong;
                if (getenv("XCPAIRDBG") && var == V_NEW && (f > 0.25 || (k != curErr[a] - curErr[b])))
                    printf("    pairdbg t=%ld %s-%s e=%.3f k=%d truth %d-%d steady %d/%d gaps a %lld/%lld b %lld/%lld open b %lld anchor %d/%d\n", n, sync_desc[a], sync_desc[b], e, k,
                           curErr[a], curErr[b], o->ss_xc_steady[a], o->ss_xc_steady[b], (long long) o->ss_rule[a].gap, (long long) o->ss_rule[a].gapPrev,
                           (long long) o->ss_rule[b].gap, (long long) o->ss_rule[b].gapPrev, (long long) (o->ss_scan_tick - o->ss_last_trolley_tick[b]), o->ss_xc_anchor[a], o->ss_xc_anchor[b]);
            }
            // oracle warm start: as if the offsets had been learned before scan 0
            if (seedOracle && !R.seeded)
            {
                bool ok = true;
                for (int b : src.bits) if (!o->SingleSensorXCEligible(b, MAXSYNCS) || !o->SingleSensorXCSteady(b)) ok = false;
                if (ok)
                {
                    for (int a : src.bits) for (int b : src.bits)
                        if (b > a) { o->ss_xc_pair[a][b].state = 2; o->ss_xc_pair[a][b].d = o->SingleSensorXCWrap(o->SingleSensorXCPos(a) - o->SingleSensorXCPos(b)); }
                    for (int b : src.bits) o->ss_xc_clean[b] = true;
                    o->ss_xc_line_ok = true;
                    R.seeded = true; R.seedTick = n;
                }
            }
        }
        // identity hash of the counting state
        unsigned long long h = 1469598103934665603ULL;
        for (int b : src.bits)
        {
            unsigned long long v[4] = { (unsigned long long) shm->SyncStatus[b].shackleno, (unsigned long long) o->trolly_counters[b],
                                        (unsigned long long) shm->SyncStatus[b].zeroed, (unsigned long long) o->true_shackle_count[b] };
            for (int q = 0; q < 4; q++) { h ^= v[q]; h *= 1099511628211ULL; }
        }
        h ^= (unsigned long long) shm->WeighShackle[0]; h *= 1099511628211ULL;
        R.stateHash.push_back(h);
    }
    for (int b : src.bits) if (curErr[b] != 0) R.eps.push_back(Episode{b, epStart[b], -1, epMax[b], epEdges[b], "open at end", curErr[b]});
    free(shm); free(o); app = 0; g_mb = 0;
    return R;
}

//===========================================================================================
//  REAL captures
//===========================================================================================
struct RealSpec
{
    const char* file; const char* what; const char* binary;
    std::vector<std::pair<int,long>> extra;       // (sync bit, scan) of a re-counted body
    int alarmCnt[4]; bool zeroLost[4];
};
// Re-counted bodies, found from the raw pulses (sensor high while the chain stopped, low during
// the stop - rolled off the body - and high again on restart). Cross-checked by the step of each
// pair offset across the stop, by flag-to-flag counts (DS2 606 + 2 between 16:54:22 and 17:03:12;
// DS2 606 across the 16:35 stop in scope-rev) and by the field alarm texts.
static const RealSpec kReal[] = {
    { "scope-20261005", "14:45-14:54 PDT, 45.5 SPM steady, no stops", "15.7.13", {},
      { 100000, 0, 100000, 100000 }, { false, false, false, false } },
    { "scope-rev", "16:26-16:40 PDT, 46 -> 20 SPM, two stops 16:34:52 / 16:35:01", "15.7.15",
      { {0, 103899}, {2, 103741} },                 // Scale 1 + Drop Sync 1 at the same stop (16:35:00 jog)
      { 100000, 0, 100000, 100000 }, { false, false, false, false } },
    { "scope-watch-20261005-1648-1710", "16:48-17:10 PDT, 9 stops", "15.7.15",
      { {2, 2581}, {3, 2760},                       // DS1 + DS2 at the same 16:48:46-52 stops
        {0, 4980},                                  // Scale 1 16:49:03 (on a body for 23 s, rolled off/on)
        {3, 67592}, {3, 88818}, {3, 103486} },      // DS2 16:54:16, 16:56:02, 16:57:15
      // alarm budgets (counts since the sync's last zero-flag alarm): DS1 alarmed 16:44:04 at the
      // flag just before the capture (set below from its count); others > a revolution ago.
      { 100000, 0, -1, 100000 }, { false, false, true, false } },
};

struct RealSource : Source
{
    std::vector<unsigned char> cnt, ev; long long t0ms;
    std::set<long> extra[MAXSYNCS], tab[MAXSYNCS];
    long counter[MAXSYNCS];
    unsigned char input(long n) { return n < (long) cnt.size() ? cnt[n] : 0; }
    long truth(int b, long n)
    {
        if (tab[b].count(n)) return TRUTH_TAB;
        if (extra[b].count(n)) return TRUTH_SAME;
        return -3;   // a trolley: the caller counts (see runReal)
    }
};
static std::string pdt(long long t0ms, long n)
{
    time_t s = (time_t) ((t0ms + n * 5) / 1000) - 7 * 3600;
    struct tm tm; gmtime_r(&s, &tm);
    char b[32]; snprintf(b, sizeof b, "%02d:%02d:%02d.%01d", tm.tm_hour, tm.tm_min, tm.tm_sec, (int) (((t0ms + n * 5) % 1000) / 100));
    return b;
}

// counting truth: wraps RealSource so a plain trolley edge advances the per-sync index
struct CountingTruth : Source
{
    RealSource* r; long idx[MAXSYNCS];
    unsigned char input(long n) { return r->input(n); }
    long truth(int b, long n)
    {
        long t = r->truth(b, n);
        if (t == TRUTH_TAB)  { idx[b] = 0; return TRUTH_TAB; }
        if (t == TRUTH_SAME) return TRUTH_SAME;
        return ++idx[b];
    }
};

static void printEps(const RunOut& r, const RealSource& s, const char* tag)
{
    for (const Episode& e : r.eps)
        printf("      %-8s %-12s off %+d%s trolley from %s to %s (%d counted edges wrong) -> %s\n", tag, sync_desc[e.sync], e.maxErr, e.lastErr != e.maxErr ? (e.lastErr > 0 ? " then +" : " then -") : "",
               pdt(s.t0ms, e.start).c_str(), e.end < 0 ? "end" : pdt(s.t0ms, e.end).c_str(), e.edges, e.how.c_str());
}

static void groupReal(const char* capdir)
{
    printf("\n== REAL: Pitman Sensor Scope captures 2026-10-05 (Shackles 303, SkipTrollies 1, SyncOn 5, ZeroFlagMode 1, syncs 0/2/3, no grading)\n");
    const int N = 303, skip = 1; const long L = 606;
    int totFalse[5] = {0}, totRight[5] = {0};
    for (const RealSpec& rs : kReal)
    {
        if (getenv("XCCAP") && !strstr(rs.file, getenv("XCCAP"))) continue;
        char path[512]; snprintf(path, sizeof path, "%s/%s.scan", capdir, rs.file);
        FILE* f = fopen(path, "rb");
        if (!f) { printf("  (%s not found - skipped)\n", rs.file); continue; }
        RealSource src; src.N = N; src.skip = skip; src.syncOn = 5; src.bits = {0, 2, 3};
        fread(&src.t0ms, 8, 1, f);
        int c;
        while ((c = fgetc(f)) != EOF) { int e = fgetc(f); src.cnt.push_back((unsigned char) c); src.ev.push_back((unsigned char) e); }
        fclose(f);
        src.nScans = (long) src.cnt.size();
        for (long n = 0; n < src.nScans; n++) if (src.ev[n] & 0x40) src.tab[src.ev[n] & 7].insert(n);
        for (auto& x : rs.extra) src.extra[x.first].insert(x.second);
        printf("\n  -- %s: %s, recorded by %s, %ld scans (%.0f s), start %s PDT\n", rs.file, rs.what, rs.binary, src.nScans, src.nScans * 0.005, pdt(src.t0ms, 0).c_str());

        // truth pre-pass: debounced edges exactly as ProcessSyncs debounces them
        InitState init; init.warm = true;
        bool ok = true;
        for (int b : src.bits)
        {
            bool armed = !((src.cnt[0] >> b) & 1); int deb = 5;
            long trolleys = 0, firstFlagBody = -1; std::vector<long> flagCounts; long sinceFlag = -1;
            for (long n = 1; n < src.nScans; n++)
            {
                bool hi = (src.cnt[n] >> b) & 1;
                if (hi && armed) { if (deb > 0) deb--; if (deb == 0) { armed = false;
                    if (src.tab[b].count(n)) { if (firstFlagBody < 0) firstFlagBody = trolleys; if (sinceFlag >= 0) flagCounts.push_back(sinceFlag); sinceFlag = 0; }
                    else if (!src.extra[b].count(n)) { trolleys++; if (sinceFlag >= 0) sinceFlag++; } } }
                else if (!hi) { armed = true; deb = 5; }
            }
            // the flag body is the last trolley before the tab: tau(start) + firstFlagBody = L
            init.tau[b] = firstFlagBody >= 0 ? (L - firstFlagBody % L) % L : 0;
            init.alarmCnt[b] = rs.alarmCnt[b] >= 0 ? rs.alarmCnt[b] : (int) (init.tau[b] / (skip + 1));   // shackleno-1
            init.zeroLost[b] = rs.zeroLost[b];
            printf("     %-12s first flag after %ld trolleys -> start at trolley %ld (shackle %ld/%ld); flag-to-flag trolleys:",
                   sync_desc[b], firstFlagBody, init.tau[b], init.tau[b] / 2 + 1, init.tau[b] % 2);
            for (long x : flagCounts) { printf(" %ld", x); CHECK(x == L, "%s %s: %ld trolleys between flags (truth wrong)", rs.file, sync_desc[b], x); if (x != L) ok = false; }
            printf("%s\n", flagCounts.empty() ? " (one flag only)" : "");
        }
        printf("     truth: re-counted bodies %zu, every flag-to-flag interval 606 trolleys: %s\n", rs.extra.size(), ok ? "yes" : "NO");

        Variant vars[5] = { strcmp(rs.binary, "15.7.13") == 0 ? V_OLD13 : V_OLD, V_NEW, V_SPEC, V_NOTRACK, V_OLD };
        RunOut res[5];
        for (int vi = 0; vi < 5; vi++)
        {
            CountingTruth ct; ct.r = &src; ct.N = N; ct.skip = skip; ct.syncOn = 5; ct.bits = src.bits; ct.nScans = src.nScans;
            for (int b : src.bits) ct.idx[b] = init.tau[b];
            res[vars[vi]] = runOne(ct, vars[vi], init, vars[vi] >= V_NEW);
        }
        // fidelity: the recording binary's zeros == the field controller's zero marks
        {
            const RunOut& r = res[vars[0]];
            std::set<std::pair<long,int>> mine(r.zeros.begin(), r.zeros.end()), field;
            for (int b : src.bits) for (long n : src.tab[b]) field.insert(std::make_pair(n, b));
            printf("     FIDELITY %s replay: zeros %zu, field zero marks %zu, identical scans: %s\n", vName[vars[0]], mine.size(), field.size(), mine == field ? "YES" : "NO");
            CHECK(mine == field, "%s: replayed zeros differ from the field controller's zero marks", rs.file);
            printf("     %s messages (the field log):\n", vName[vars[0]]);
            for (const Msg& m : r.msgs) printf("        %s  %s", pdt(src.t0ms, m.tick).c_str(), m.txt.c_str());
            printf("     %s count errors vs truth:\n", vName[vars[0]]); printEps(r, src, vName[vars[0]]);
        }
        for (int vi = 1; vi < 4; vi++)
        {
            const RunOut& r = res[vars[vi]];
            printf("     %s (offsets learned before the capture, seeded at %s): corrections %d (right %d, FALSE %d), disagreement alarms %d, zero alarms %d\n",
                   vName[vars[vi]], pdt(src.t0ms, r.seedTick).c_str(), (int) r.corr.size(), r.corrRight, r.corrFalse, r.disagree, r.zeroAlarms);
            for (const Corr& c : r.corr)
                printf("        %s  Count corrected: %-12s -> count error after %+d  %s\n", pdt(src.t0ms, c.tick).c_str(), sync_desc[c.sync], c.errAfter, c.right ? "right" : "FALSE");
            for (const Msg& m : r.msgs) if (m.txt.compare(0, 19, "Count disagreement:") == 0 || m.txt.find("Zero Flag") != std::string::npos)
                printf("        %s  %s", pdt(src.t0ms, m.tick).c_str(), m.txt.c_str());
            printEps(r, src, vName[vars[vi]]);
            printf("        armed-pair checks %ld: max |e - round(e)| %.3f, integer offset disagreeing with truth %ld (+%ld single-shot); missed-bird passes: %d revolutions, dup %d missing %d (in %d corrected revolutions: dup %d missing %d); scale re-labels %d\n",
                   r.pairChecks, r.maxFracSteady, r.pairWrongInt, r.pairWrong1, r.mbRevs, r.mbDup, r.mbMiss, r.mbCorrRevs, r.mbCorrDup, r.mbCorrMiss, r.weighDup);
            totFalse[vars[vi]] += r.corrFalse; totRight[vars[vi]] += r.corrRight;
        }
        // NEW must never correct wrongly and its integer reading must always match the truth
        CHECK(res[V_NEW].corrFalse == 0, "%s: 15.7.16 made %d FALSE corrections", rs.file, res[V_NEW].corrFalse);
        CHECK(res[V_NEW].pairWrongInt == 0, "%s: 15.7.16 read %ld wrong integer offsets", rs.file, res[V_NEW].pairWrongInt);
        CHECK(res[V_NEW].alarmSpacingViol == 0, "%s: disagreement alarm spacing", rs.file);
        CHECK(res[V_NEW].mbCorrDup == 0 && res[V_NEW].mbCorrMiss == 0, "%s: 15.7.16 pass accounting in corrected revolutions dup %d miss %d", rs.file, res[V_NEW].mbCorrDup, res[V_NEW].mbCorrMiss);

        // cold starts: no offsets learned (counts exact) / from boot - must equal 15.7.15 scan for scan
        {
            CountingTruth ct; ct.r = &src; ct.N = N; ct.skip = skip; ct.syncOn = 5; ct.bits = src.bits; ct.nScans = src.nScans;
            for (int b : src.bits) ct.idx[b] = init.tau[b];
            RunOut cold = runOne(ct, V_NEW, init, false);
            bool same = cold.stateHash == res[V_OLD].stateHash;
            int pairs1 = 0, pairs2 = 0;
            printf("     15.7.16 cold (nothing learned at scan 0): corrections %d, disagreement alarms %d, counter state == 15.7.15 every scan: %s\n",
                   (int) cold.corr.size(), cold.disagree, same ? "YES" : "NO");
            CHECK(cold.corr.empty() && same, "%s: cold 15.7.16 differs from 15.7.15", rs.file);
            InitState boot = init; boot.warm = false;
            CountingTruth cb; cb.r = &src; cb.N = N; cb.skip = skip; cb.syncOn = 5; cb.bits = src.bits; cb.nScans = src.nScans;
            for (int b : src.bits) cb.idx[b] = init.tau[b];
            RunOut b1 = runOne(cb, V_OLD, boot, false);
            for (int b : src.bits) cb.idx[b] = init.tau[b];
            RunOut b2 = runOne(cb, V_NEW, boot, false);
            printf("     15.7.16 from boot: corrections %d, counter state == 15.7.15 from boot every scan: %s\n", (int) b2.corr.size(), b1.stateHash == b2.stateHash ? "YES" : "NO");
            CHECK(b2.corr.empty() && b1.stateHash == b2.stateHash, "%s: boot 15.7.16 differs from 15.7.15", rs.file);
            (void) pairs1; (void) pairs2;
        }
    }
    printf("\n  REAL totals: 15.7.16 corrections right %d FALSE %d | SPEC-rule right %d FALSE %d | NOTRACK right %d FALSE %d\n",
           totRight[V_NEW], totFalse[V_NEW], totRight[V_SPEC], totFalse[V_SPEC], totRight[V_NOTRACK], totFalse[V_NOTRACK]);
}

//===========================================================================================
//  SYNTHETIC chain
//===========================================================================================
struct Knot { double t, spm; };
struct Stop { double t, dur, roll; };             // chain stops at t for dur s, rolls back `roll` trolleys
struct Fault { int bit; double t; int type; };     // type 0 = extra pulse, 1 = missing body

struct SynCfg
{
    std::string name;
    int N = 303, skip = 1, syncOn = 5;
    std::vector<int> bits = {0, 2, 3};
    std::vector<double> pos = {0.0, 249.37, 394.71};   // sensor chain positions (trolleys)
    double f = 0.41, wb = 0.20, wt = 0.20, jit = 0.0, prof = 0.12;   // tab lead, body/tab width, jitter, pitch-profile amplitude
    std::vector<Knot> spm;
    std::vector<Stop> stops;
    std::vector<Fault> faults;
    double dur = 600;
    unsigned seed = 1;
    double startRev = 0.37;
};

struct SynSource : Source
{
    SynCfg c; long long L;
    std::vector<double> x;                         // chain position per scan
    std::vector<std::pair<long,int>> extraPulse;   // (scan, bit) start of an 8-scan spurious pulse
    std::set<std::pair<int,long long>> missing;    // (bit, trolley k) bodies the sensor does not see
    double pk(long long k) const
    {
        long long km = ((k % L) + L) % L;
        return (double) k + c.prof * sin(2 * M_PI * (double) km / (double) L + 0.7) + c.jit * (urand((unsigned long long) k, c.seed * 7 + 1) - 0.5);
    }
    // feature under sensor b at scan n: >=0 4*k+type (0 body, 1 tab, 2 noise), -1 low
    long long feat(int bi, long n) const
    {
        int b = c.bits[bi];
        for (auto& e : extraPulse) if (e.second == b && n >= e.first && n < e.first + 8) return 2;
        double y = x[n] - c.pos[bi];
        long long k0 = (long long) floor(y);
        for (long long kk = k0 + 2; kk >= k0 - 2; kk--)
        {
            double e = pk(kk);
            if (y >= e && y < e + c.wb) { if (missing.count(std::make_pair(b, kk))) return -1; return 4 * kk; }
            if (((kk % L) + L) % L == 0) { double te = e + c.f; if (y >= te && y < te + c.wt) return 4 * kk + 1; }
        }
        return -1;
    }
    unsigned char input(long n)
    {
        unsigned char v = 0;
        for (size_t bi = 0; bi < c.bits.size(); bi++) if (feat((int) bi, n) >= 0) v |= (unsigned char) (1 << c.bits[bi]);
        return v;
    }
    long lastTruth[MAXSYNCS];
    long truth(int b, long n)
    {
        int bi = 0; for (size_t q = 0; q < c.bits.size(); q++) if (c.bits[q] == b) bi = (int) q;
        long long ft = feat(bi, n);
        if (ft < 0 || (ft & 3) == 2) return TRUTH_SAME;
        if ((ft & 3) == 1) return TRUTH_TAB;
        long long k = ft >> 2;
        return (long) (((k % L) + L) % L);
    }
    void build()
    {
        N = c.N; skip = c.skip; syncOn = c.syncOn; bits = c.bits;
        L = (long long) c.N * (c.skip + 1);
        nScans = (long) (c.dur * 200);
        x.assign(nScans + 1, 0);
        double xx = (1.0 - c.startRev) * L + 0.5;   // first flag at sensor 0 after startRev of a revolution
        xx = L - c.startRev * L;
        xx = (double) L * 2 - c.startRev * L;
        auto spmAt = [&](double t) {
            const std::vector<Knot>& k = c.spm;
            if (t <= k[0].t) return k[0].spm;
            for (size_t i = 1; i < k.size(); i++) if (t < k[i].t) { double a = k[i-1].t, bb = k[i].t; if (bb <= a) return k[i].spm; return k[i-1].spm + (k[i].spm - k[i-1].spm) * (t - a) / (bb - a); }
            return k.back().spm;
        };
        for (long n = 0; n <= nScans; n++)
        {
            double t = n * 0.005;
            double v = spmAt(t) * (c.skip + 1) / 60.0 * 0.005;    // trolleys per scan
            for (const Stop& s : c.stops)
            {
                // decel 1.5 s before t, stopped [t, t+dur], accel 2 s after
                if (t >= s.t - 1.5 && t < s.t) v *= (s.t - t) / 1.5;
                else if (t >= s.t && t < s.t + s.dur)
                {
                    v = 0;
                    if (t < s.t + 0.6) v = -s.roll / (0.6 * 200);       // roll back during the first 0.6 s
                }
                else if (t >= s.t + s.dur && t < s.t + s.dur + 2.0) v *= (t - s.t - s.dur) / 2.0;
            }
            x[n] = xx; xx += v;
        }
        if (getenv("XCXDUMP") && (getenv("XCONLY") == 0 || c.name.find(getenv("XCONLY")) != std::string::npos))
        {
            double a = atof(getenv("XCXDUMP")), bnd = atof(strchr(getenv("XCXDUMP"), ':') + 1);
            for (long n = (long) (a * 200); n < (long) (bnd * 200) && n < nScans; n += 10)
                printf("    x t=%.2f x=%.4f dx=%.5f in=%02x stops:", n * 0.005, x[n], x[n+1] - x[n], input(n)), [&]{ for (const Stop& s2 : c.stops) if (fabs(s2.t - n*0.005) < 120) printf(" [%.2f dur %.1f roll %.2f]", s2.t, s2.dur, s2.roll); printf("\n"); }();
        }
        for (const Fault& fl : c.faults)
        {
            long n = (long) (fl.t * 200);
            if (fl.type == 0)
            {
                // place the pulse where this sensor is in a gap (low 4 scans before to 12 after), so
                // it really is an extra edge and does not just merge into a body
                int bi = 0; for (size_t q = 0; q < c.bits.size(); q++) if (c.bits[q] == fl.bit) bi = (int) q;
                for (; n + 16 < nScans; n++)
                {
                    bool low = true;
                    for (long m = n - 4; m < n + 12 && low; m++) if (feat(bi, m) >= 0) low = false;
                    if (low) break;
                }
                extraPulse.push_back(std::make_pair(n, fl.bit));
            }
            else
            {
                int bi = 0; for (size_t q = 0; q < c.bits.size(); q++) if (c.bits[q] == fl.bit) bi = (int) q;
                long long k = (long long) floor(x[n] - c.pos[bi]) + 1;     // the next body to reach the sensor
                missing.insert(std::make_pair(fl.bit, k));
            }
        }
    }
};

struct Tot { int weighDupOld = 0; long checks = 0, wrong1 = 0; int unfixed = 0, corrRevs = 0, identical = 0, zeroResid = 0; int runs = 0, faults = 0, right = 0, falseC = 0, disagree = 0, zeroAl = 0, mbDup = 0, mbMiss = 0, weighDup = 0; long errOld = 0, errNew = 0; long wrongInt = 0; double maxFrac = 0; };
static std::map<std::string, Tot> g_tot;

static RunOut synRun(const std::string& grp, SynCfg c, bool verbose = false, Variant cmp = V_OLD, bool expectNone = false)
{
    if (getenv("XCONLY") && c.name.find(getenv("XCONLY")) == std::string::npos) return RunOut();
    SynSource s; s.c = c; s.build();
    InitState boot; boot.warm = false;
    RunOut rn = runOne(s, V_NEW, boot, false);
    RunOut ro = runOne(s, cmp, boot, false);
    Tot& t = g_tot[grp]; t.runs++;
    t.right += rn.corrRight; t.falseC += rn.corrFalse; t.disagree += rn.disagree; t.zeroAl += rn.zeroAlarms;
    t.mbDup += rn.mbCorrDup; t.mbMiss += rn.mbCorrMiss; t.wrong1 += rn.pairWrong1; t.checks += rn.pairChecks; t.weighDupOld += ro.weighDup; t.corrRevs += rn.mbCorrRevs; t.weighDup += rn.weighDup; t.wrongInt += rn.pairWrongInt;
    if (rn.maxFracSteady > t.maxFrac) t.maxFrac = rn.maxFracSteady;
    long eo = 0, en = 0; for (int b : c.bits) { eo += ro.errEdges[b]; en += rn.errEdges[b]; }
    t.errOld += eo; t.errNew += en;
    int nf = 0; for (auto& e : ro.eps) if (e.maxErr) nf++;
    t.faults += nf;
    if (verbose || rn.corrFalse || rn.pairWrongInt || getenv("XCEPS"))
    {
        printf("    %-64s err-edges %s %5ld -> 15.7.16 %5ld | corr %d right %d FALSE %d | disagree %d | max|frac| %.3f wrongInt %ld\n",
               c.name.c_str(), vName[cmp], eo, en, (int) rn.corr.size(), rn.corrRight, rn.corrFalse, rn.disagree, rn.maxFracSteady, rn.pairWrongInt);
        if (verbose || getenv("XCEPS")) for (const Episode& e : rn.eps)
            printf("        %-12s off %+d at %.1fs .. %s (%d edges) -> %s\n", sync_desc[e.sync], e.maxErr, e.start * 0.005,
                   e.end < 0 ? "end" : (std::to_string(e.end * 0.005).substr(0, 6) + "s").c_str(), e.edges, e.how.c_str());
    }
    CHECK(rn.corrFalse == 0, "%s: %d FALSE corrections", c.name.c_str(), rn.corrFalse);
    CHECK(rn.pairWrongInt == 0, "%s: %ld wrong integer offsets", c.name.c_str(), rn.pairWrongInt);
    CHECK(rn.alarmSpacingViol == 0, "%s: %d disagreement alarms closer than one revolution for a pair", c.name.c_str(), rn.alarmSpacingViol);
    CHECK(rn.mbCorrDup == 0 && rn.mbCorrMiss == 0, "%s: pass accounting in corrected revolutions dup %d miss %d", c.name.c_str(), rn.mbCorrDup, rn.mbCorrMiss);
    { int open1 = 0; for (const Episode& e : rn.eps) if (e.lastErr == 1 && (e.how == "open at end" || e.how == "other")) open1++; t.unfixed += open1; }
    if (expectNone)
    {
        // no physical miscount on these streams. If the zero detector itself (identical in both
        // versions) never miscounts either, 15.7.16 must be 15.7.15 scan for scan; where 15.7.15's
        // known zero residuals (big steps / stops at the flag) do miscount, 15.7.16 may only make it better
        if (nf == 0)
        {
            CHECK(rn.corr.empty() && rn.disagree == 0, "%s: clean line but %d corrections %d disagreement alarms", c.name.c_str(), (int) rn.corr.size(), rn.disagree);
            CHECK(rn.stateHash == ro.stateHash, "%s: clean line but counter state differs from %s", c.name.c_str(), vName[cmp]);
            t.identical++;
        }
        else
        {
            CHECK(en <= eo, "%s: 15.7.16 %ld wrong-count edges vs %s %ld", c.name.c_str(), en, vName[cmp], eo);
            t.zeroResid++;
            if (verbose == false && getenv("XCRESID")) { printf("    (zero residual) %s: %s eps %d err-edges %ld -> %ld, corr %d\n", c.name.c_str(), vName[cmp], nf, eo, en, (int) rn.corr.size());
                for (const Episode& e : ro.eps) printf("        15.7.15 %-12s off %+d at %.1fs (%d edges) -> %s\n", sync_desc[e.sync], e.maxErr, e.start * 0.005, e.edges, e.how.c_str());
                for (const Episode& e : rn.eps) printf("        15.7.16 %-12s off %+d at %.1fs (%d edges) -> %s\n", sync_desc[e.sync], e.maxErr, e.start * 0.005, e.edges, e.how.c_str()); }
        }
    }
    return rn;
}

static std::vector<Knot> flat(double spm) { return { Knot{0, spm} }; }
static bool want(int argc, char** argv, const char* g)
{
    if (argc < 2) return true;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], g)) return true;
    return false;
}
static double revSec(const SynCfg& c, double spm) { return c.N / spm * 60.0; }

// a run long enough to arm the offsets from boot: first zero + 2 clean revolutions + margin
static SynCfg base(const std::string& nm, int N, int skip, double spm, double revsAfterArm)
{
    SynCfg c; c.name = nm; c.N = N; c.skip = skip; c.spm = flat(spm);
    if (skip == 0) { c.pos = {0.0, N * 0.41 + 0.37, N * 0.65 + 0.71}; c.wb = 0.18; c.wt = 0.15; c.f = 0.40; }
    c.dur = revSec(c, spm) * (3.6 + revsAfterArm);
    return c;
}
static double armT(const SynCfg& c, double spm) { return revSec(c, spm) * 3.6; }   // offsets armed by then

int main(int argc, char** argv)
{
    printf("SS_CROSSCHECK=%d  K_TOL %.2f  AGREE %.2f  steady %d gaps %d..%d%%  open gap <=%d%%  hyst %d  min syncs %d  confirm %.2f  relearn %.2f  track %.2f/%d%%/1:%d  alarm %d edges\n",
           SS_CROSSCHECK, SS_XC_K_TOL_PERMIL / 1000.0, SS_XC_AGREE_TOL_PERMIL / 1000.0, SS_XC_STEADY_GAPS, SS_XC_GAP_LO_PCT, SS_XC_GAP_HI_PCT, SS_XC_OPEN_GAP_PCT, SS_XC_HYST_EDGES,
           SS_XC_MIN_SYNCS, SS_XC_CONFIRM_TOL_PERMIL / 1000.0, SS_XC_RELEARN_TOL_PERMIL / 1000.0, SS_XC_TRACK_TOL_PERMIL / 1000.0,
           SS_XC_TRACK_GAP_PCT, SS_XC_TRACK_GAIN, SS_XC_ALARM_EDGES);
    const char* caps = getenv("CAPS");
    if (want(argc, argv, "real") && caps) groupReal(caps);

    const double jits[3] = { 0.0, 0.05, 0.10 };

    //=== clean lines: steady, steps, ramps, stops without rollback -> nothing may change
    if (want(argc, argv, "identity"))
    {
        printf("\n== CLEAN LINES (no miscount): 15.7.16 must equal 15.7.15 scan for scan, 0 corrections, 0 alarms\n");
        const double sp[] = { 7, 12, 20, 33, 45, 55, 71 };
        for (double v : sp) for (int j = 0; j < 3; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM jitter %2.0f%%", v, jits[j] * 100);
            SynCfg c = base(nm, 303, 1, v, 1.5); c.jit = jits[j]; c.seed = 10 + j;
            synRun("clean_pitman_steady", c, false, V_OLD, true);
        }
        const double sc[] = { 60, 100, 140, 180, 200 };
        for (double v : sc) for (int j = 0; j < 3; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Chicken 1189 %3.0f SPM jitter %2.0f%%", v, jits[j] * 100);
            SynCfg c = base(nm, 1189, 0, v, 1.5); c.jit = jits[j]; c.seed = 20 + j;
            synRun("clean_chicken_steady", c, false, V_OLD, true);
        }
        // speed changes 7..71 SPM: steps and ramps, all after arming
        const double pr[4][2] = { {20, 55}, {7, 71}, {45, 20}, {33, 70} };
        const double ramps[4] = { 0, 2, 5, 15 };
        for (int p = 0; p < 4; p++) for (int r = 0; r < 4; r++) for (int j = 0; j < 3; j += 2)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Pitman %2.0f<->%2.0f ramp %2.0fs jit %2.0f%%", pr[p][0], pr[p][1], ramps[r], jits[j] * 100);
            SynCfg c = base(nm, 303, 1, pr[p][0], 1.5); c.jit = jits[j]; c.seed = 100 + p * 8 + r * 2 + j;
            double t0 = armT(c, pr[p][0]);
            double slow = (std::min)(pr[p][0], pr[p][1]);
            c.dur = t0 + revSec(c, slow) * 1.6;
            for (double t = t0; t < c.dur; t += 47) { c.spm.push_back(Knot{t, c.spm.back().spm}); double to = c.spm.back().spm == pr[p][0] ? pr[p][1] : pr[p][0]; c.spm.push_back(Knot{t + ramps[r], to}); }
            synRun("clean_pitman_speed", c, false, V_OLD, true);
        }
        for (int p = 0; p < 2; p++) for (int r = 0; r < 4; r += 1)
        {
            const double cp[2][2] = { {140, 180}, {60, 180} };
            char nm[96]; snprintf(nm, sizeof nm, "Chicken %3.0f<->%3.0f ramp %2.0fs jit 5%%", cp[p][0], cp[p][1], ramps[r]);
            SynCfg c = base(nm, 1189, 0, cp[p][0], 1.5); c.jit = 0.05; c.seed = 200 + p * 4 + r;
            double t0 = armT(c, cp[p][0]);
            c.dur = t0 + revSec(c, (std::min)(cp[p][0], cp[p][1])) * 1.4;
            for (double t = t0; t < c.dur; t += 31) { c.spm.push_back(Knot{t, c.spm.back().spm}); double to = c.spm.back().spm == cp[p][0] ? cp[p][1] : cp[p][0]; c.spm.push_back(Knot{t + ramps[r], to}); }
            synRun("clean_chicken_speed", c, false, V_OLD, true);
        }
        // 9-hour soak like Pitman 2026-10-06 (19,500 birds 00:37-09:48, 49-71 SPM, no zero alarm on 15.7.15):
        // speed wandering 49..71 SPM with 20-90 s ramps, short stops without rollback, jitter 0/5%
        for (int j = 0; j < 2; j++)
        {
            char nm[96]; snprintf(nm, sizeof nm, "Pitman 9 h soak 49-71 SPM, slow ramps + clean stops, jit %2.0f%%", jits[j] * 100);
            SynCfg c = base(nm, 303, 1, 60, 0); c.jit = jits[j]; c.seed = 77 + j; c.dur = 9 * 3600;
            double t = armT(c, 60), v = 60;
            while (t < c.dur - 200)
            {
                double to = 49 + 22 * urand((unsigned long long) t, 41), ramp = 20 + 70 * urand((unsigned long long) t, 42);
                c.spm.push_back(Knot{t, v}); c.spm.push_back(Knot{t + ramp, to}); v = to;
                t += ramp + 120 + 600 * urand((unsigned long long) t, 43);
                if (urand((unsigned long long) t, 44) < 0.4) { c.stops.push_back(Stop{t, 3 + 40 * urand((unsigned long long) t, 45), 0.0}); t += 60; }
            }
            RunOut r = synRun("clean_soak_9h", c, false, V_OLD, true);
            CHECK(r.corr.empty() && r.disagree == 0, "%s: %d corrections %d disagreement alarms", nm, (int) r.corr.size(), r.disagree);
        }
        // stops WITHOUT rollback: no miscount happens, nothing may be corrected
        for (int j = 0; j < 3; j++) for (int v = 0; v < 3; v++)
        {
            const double sp3[3] = { 20, 45, 71 };
            char nm[96]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM, stops (no rollback) every 61 s, jit %2.0f%%", sp3[v], jits[j] * 100);
            SynCfg c = base(nm, 303, 1, sp3[v], 1.5); c.jit = jits[j]; c.seed = 300 + j * 3 + v;
            double t0 = armT(c, sp3[v]);
            for (double t = t0; t < c.dur - 60; t += 61) c.stops.push_back(Stop{t, 5 + urand((unsigned long long) t, 9) * 40, 0.0});
            synRun("clean_pitman_stops", c, false, V_OLD, true);
        }
    }

    //=== stops WITH rollback (the real mechanism): every re-count must be fixed, nothing wrongly
    if (want(argc, argv, "synth_pitman") || want(argc, argv, "synth_chicken"))
    {
        printf("\n== STOPS WITH ROLLBACK (bodies re-counted by the real mechanism)\n");
        for (int cfg = 0; cfg < 2; cfg++)
        {
            if (cfg == 0 && !want(argc, argv, "synth_pitman")) continue;
            if (cfg == 1 && !want(argc, argv, "synth_chicken")) continue;
            for (int seed = 0; seed < 24; seed++)
            {
                double v = cfg == 0 ? 20 + (seed % 6) * 10 : 60 + (seed % 8) * 20;
                char nm[128]; snprintf(nm, sizeof nm, "%s %3.0f SPM, rollback stops, jit %2.0f%%, seed %d", cfg ? "Chicken" : "Pitman", v, jits[seed % 3] * 100, seed);
                SynCfg c = cfg == 0 ? base(nm, 303, 1, v, 1.5) : base(nm, 1189, 0, v, 1.2);
                c.jit = jits[seed % 3]; c.seed = 400 + seed + cfg * 100;
                double t0 = armT(c, v);
                for (double t = t0 + 10; t < c.dur - 60; t += 35 + urand(seed, (unsigned long long) t) * 50)
                    c.stops.push_back(Stop{t, 2 + urand((unsigned long long) t, 3) * 60, 0.05 + 0.35 * urand((unsigned long long) t, 5)});
                synRun(cfg ? "rollback_chicken" : "rollback_pitman", c, seed < 2);
            }
        }
    }

    //=== single extra / missing edges on one sync, in steady running
    if (want(argc, argv, "synth_faults"))
    {
        printf("\n== SINGLE FAULTS on one sync (extra pulse = corrected; missing body = alarm only, by design)\n");
        for (int cfg = 0; cfg < 2; cfg++) for (int type = 0; type < 2; type++) for (int seed = 0; seed < 16; seed++)
        {
            double v = cfg == 0 ? 20 + (seed % 5) * 12.5 : 60 + (seed % 8) * 20;
            char nm[128]; snprintf(nm, sizeof nm, "%s %3.0f SPM %s, jit %2.0f%%, seed %d", cfg ? "Chicken" : "Pitman", v, type ? "missing body" : "extra pulse", jits[seed % 3] * 100, seed);
            SynCfg c = cfg == 0 ? base(nm, 303, 1, v, 1.0) : base(nm, 1189, 0, v, 0.8);
            c.jit = jits[seed % 3]; c.seed = 600 + seed + type * 50 + cfg * 100;
            double t0 = armT(c, v);
            int bits3[3] = {0, 2, 3};
            for (int q = 0; q < 3; q++) c.faults.push_back(Fault{bits3[(seed + q) % 3], t0 + 20 + q * (c.dur - t0 - 60) / 3 + urand(seed, q) * 10, type});
            RunOut r = synRun(type ? (cfg ? "missing_chicken" : "missing_pitman") : (cfg ? "extra_chicken" : "extra_pitman"), c, seed == 0);
            if (type == 1) CHECK(r.corr.empty(), "%s: a missing body must not be corrected", nm);
            if (type == 0 && jits[seed % 3] <= 0.05) { int open1 = 0; for (const Episode& e : r.eps) if (e.lastErr == 1 && e.how != "corrected" && e.how != "zero") open1++;
                             CHECK(open1 == 0, "%s: %d extra pulses left uncorrected", nm, open1); }
        }
    }

    //=== 2-sync line: alarm only
    if (want(argc, argv, "synth_2sync"))
    {
        printf("\n== 2-SYNC LINE (scale + one drop sync): never corrects, alarms once per revolution\n");
        for (int seed = 0; seed < 8; seed++)
        {
            double v = 20 + seed * 7;
            char nm[128]; snprintf(nm, sizeof nm, "Pitman 2-sync %2.0f SPM, rollback stops + extra pulses, seed %d", v, seed);
            SynCfg c = base(nm, 303, 1, v, 1.5); c.bits = {0, 2}; c.pos = {0.0, 249.37}; c.seed = 700 + seed; c.jit = 0.05;
            double t0 = armT(c, v);
            for (double t = t0 + 10; t < c.dur - 60; t += 50) c.stops.push_back(Stop{t, 3 + urand((unsigned long long) t, 3) * 30, 0.1 + 0.3 * urand((unsigned long long) t, 5)});
            c.faults.push_back(Fault{2, t0 + 33, 0});
            RunOut r = synRun("two_sync", c, seed == 0);
            CHECK(r.corr.empty(), "%s: a 2-sync line corrected", nm);
            CHECK(r.disagree > 0, "%s: no disagreement alarm", nm);
        }
    }

    //=== two syncs slipping at once
    if (want(argc, argv, "synth_double"))
    {
        printf("\n== TWO SYNCS AT ONCE: in steady running (two extra pulses together) = no correction; at a line stop (both rolled back) = both corrected (P2)\n");
        for (int seed = 0; seed < 12; seed++)
        {
            double v = 20 + (seed % 6) * 10;
            char nm[128]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM, extra pulses on two syncs in one scan, seed %d", v, seed);
            SynCfg c = base(nm, 303, 1, v, 1.0); c.seed = 800 + seed; c.jit = 0.05;
            double t0 = armT(c, v);
            int a = (seed % 3 == 0) ? 0 : 2, b = (seed % 3 == 2) ? 0 : 3;
            c.faults.push_back(Fault{a, t0 + 40, 0}); c.faults.push_back(Fault{b, t0 + 40, 0});
            RunOut r = synRun("double_steady", c, seed == 0);
            // both slipped: nothing may be corrected until one of them has re-zeroed at its own flag
            // (then the other is the lone one, attributably, and is corrected - that is allowed)
            int both = 0; long firstEnd = -1;
            for (const Episode& e : r.eps) if ((e.sync == a || e.sync == b) && e.maxErr == 1 && fabs(e.start * 0.005 - (t0 + 40)) < 5)
            { both++; if (e.end >= 0 && e.how == "zero" && (firstEnd < 0 || e.end < firstEnd)) firstEnd = e.end; }
            CHECK(both == 2, "%s: the two pulses made %d slips, not 2", nm, both);
            int early = 0; for (const Corr& k : r.corr) if (firstEnd < 0 || k.tick < firstEnd) early++;
            CHECK(early == 0, "%s: %d corrections while two syncs were off together", nm, early);
            CHECK(r.disagree > 0, "%s: no disagreement alarm", nm);
        }
        // at a stop: put sensors 2 and 3 on bodies at the same phase (offsets 0.10 apart) so one rollback hits both
        for (int seed = 0; seed < 12; seed++)
        {
            double v = 20 + (seed % 6) * 10;
            char nm[128]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM, DS1+DS2 in phase, rollback stops, seed %d", v, seed);
            SynCfg c = base(nm, 303, 1, v, 1.5); c.seed = 850 + seed; c.jit = 0.03;
            c.pos = {0.0, 249.40, 394.48};
            double t0 = armT(c, v);
            for (double t = t0 + 10; t < c.dur - 60; t += 45) c.stops.push_back(Stop{t, 3 + urand((unsigned long long) t, 3) * 30, 0.15 + 0.25 * urand((unsigned long long) t, 5)});
            synRun("double_stop", c, seed == 0);
        }
    }

    //=== phase-wrap trap: sensors whose edges nearly coincide
    if (want(argc, argv, "synth_phase"))
    {
        printf("\n== NEAR-COINCIDENT SENSOR PHASES (offset fraction 0.00 .. 0.02 / 0.98 .. 1.00), jitter 0-10%%\n");
        const double fr[6] = { 0.000, 0.005, 0.02, 0.98, 0.995, 0.5 };
        for (int a = 0; a < 6; a++) for (int j = 0; j < 3; j++) for (int kind = 0; kind < 2; kind++)
        {
            double v = 25 + a * 8;
            char nm[128]; snprintf(nm, sizeof nm, "Pitman %2.0f SPM, DS phases %.3f/%.3f, jit %2.0f%%, %s", v, fr[a], fr[(a + 3) % 6], jits[j] * 100, kind ? "rollback stops + pulses" : "clean");
            SynCfg c = base(nm, 303, 1, v, 1.5); c.jit = jits[j]; c.seed = 900 + a * 10 + j + kind * 5;
            c.pos = {0.0, 249.0 + fr[a], 394.0 + fr[(a + 3) % 6]};
            double t0 = armT(c, v);
            if (kind)
            {
                for (double t = t0 + 10; t < c.dur - 60; t += 55) c.stops.push_back(Stop{t, 3 + urand((unsigned long long) t, 3) * 30, 0.05 + 0.35 * urand((unsigned long long) t, 5)});
                c.faults.push_back(Fault{3, t0 + 31, 0});
                synRun("phase_faults", c);
            }
            else synRun("phase_clean", c, false, V_OLD, true);
        }
    }

    //=== 4 count syncs, chicken
    if (want(argc, argv, "synth_4sync"))
    {
        printf("\n== 4 COUNT SYNCS (chicken 1189, skip 0), rollback stops + pulses\n");
        for (int seed = 0; seed < 8; seed++)
        {
            double v = 80 + seed * 15;
            char nm[128]; snprintf(nm, sizeof nm, "Chicken 4-sync %3.0f SPM, seed %d", v, seed);
            SynCfg c = base(nm, 1189, 0, v, 1.2); c.bits = {0, 2, 3, 4}; c.pos = {0.0, 301.3, 655.81, 902.47}; c.seed = 1000 + seed; c.jit = 0.05;
            double t0 = armT(c, v);
            for (double t = t0 + 10; t < c.dur - 60; t += 40) c.stops.push_back(Stop{t, 3 + urand((unsigned long long) t, 3) * 30, 0.05 + 0.3 * urand((unsigned long long) t, 5)});
            c.faults.push_back(Fault{4, t0 + 23, 0});
            synRun("four_sync", c, seed == 0);
        }
    }

    printf("\n== SYNTHETIC TOTALS (15.7.16 vs 15.7.15 on the same streams)\n");
    printf("  %-22s %5s %8s %6s %6s %8s %9s %10s %13s %13s %11s %9s %8s\n", "group", "runs", "episodes", "right", "FALSE", "+1 left", "disagree", "zero alarm", "err-edges old", "err-edges new", "corrRev d/m", "weighDup", "max|frac|");
    for (auto& it : g_tot)
    {
        const Tot& t = it.second;
        printf("  %-22s %5d %8d %6d %6d %8d %9d %10d %13ld %13ld %4d:%d/%-4d %9d %8.3f\n", it.first.c_str(), t.runs, t.faults, t.right, t.falseC, t.unfixed,
               t.disagree, t.zeroAl, t.errOld, t.errNew, t.corrRevs, t.mbDup, t.mbMiss, t.weighDup, t.maxFrac);
        printf("  %-22s   (armed-pair readings wrong at 2 consecutive checks: %ld, single-shot: %ld, of %ld checks; scale weigh labels repeated in a revolution: 15.7.15 %d, 15.7.16 %d)\n", "", t.wrongInt, t.wrong1, t.checks, t.weighDupOld, t.weighDup);
        if (t.identical || t.zeroResid) printf("  %-22s   (identical to 15.7.15 scan for scan: %d runs; runs where the shared zero detector miscounted: %d)\n", "", t.identical, t.zeroResid);
    }
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
