//--------------------------------------------------------------------------
//  errqueue_test.cpp - controller -> host message queue (GenError -> SendError), 15.7.16
//
//  Build + run:   linux_port/tools/errqueue_test.sh
//  (the script cuts the REAL GenError / SendError / SendErrorMsg / ErrQueuePending out of the
//   working tree's overhead.cpp, and 15.7.15's GenError / SendError (git b269cf8) for the "before"
//   column, and compiles this file against the controller headers and the real platform mutex)
//
//  THE BUG (rig, 2026-10-06): GenError kept ONE slot pointing at the caller's buffer, SendError
//  (GpSendThread, every 50 ms) sent that slot. Two GenErrors in one scan reached the host as one -
//  the last - and the shared app_err_buf could be rewritten before it was sent.
//
//  Checks: bursts of 1..64 in one scan through the reused app_err_buf (all delivered, in order,
//  right text and severity); overflow (oldest dropped, one summary after the survivors); host not
//  connected (kept, delivered on connect); the send mutex held (kept, delivered later); text too
//  long (not sent, same log line as before); NULL text; the GenError log lines byte-identical to
//  15.7.15; concurrent producers (real threads) against a live consumer - no torn text, no
//  duplicate, per-producer order, delivered + reported-dropped == produced.
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <atomic>

#define SendError SendError(); void SendError_OLD(); void GenError_OLD(int sev, char* txt); void SendError_X
#define private public
#include "types.h"
#undef private
#undef SendError

overhead*   app = 0;
UINT        TraceMask = 0;
trcbuf      trc_buf    [MAXTRCBUFFERS];
tmptrcbuf   tmp_trc_buf[MAXTRCBUFFERS];
trace_ctrl  trc        [MAXTRCBUFFERS];
char        app_err_buf[MAXERRMBUFSIZE];
volatile sig_atomic_t g_outputs_disabled = 0;
#define _FILE_ "overhead.cpp"
typedef app_type::SHARE_MEMORY SHARE_MEMORY;

// isPShmValid() (overheadext.h) reads these
volatile int   g_sentinel_magic = 0;
volatile void* g_sentinel_pShm = 0;

// ---- captures: the dchserver.log (RtPrintf) and the host (SendHostMsg) -----------------------
struct Sent { int cmd, var; std::string txt; };
static pthread_mutex_t     g_cap = PTHREAD_MUTEX_INITIALIZER;
static std::vector<std::string> g_log;
static std::vector<Sent>   g_host;
int RtPrintf(const char* f, ...)
{
    char b[2048]; va_list ap; va_start(ap, f); vsnprintf(b, sizeof b, f, ap); va_end(ap);
    pthread_mutex_lock(&g_cap); g_log.push_back(b); pthread_mutex_unlock(&g_cap);
    return 0;
}
int overhead::SendHostMsg(int cmd, int var, BYTE* data, int len)
{
    pthread_mutex_lock(&g_cap); g_host.push_back(Sent{cmd, var, std::string((char*) data, len)}); pthread_mutex_unlock(&g_cap);
    return 0;
}

#include "errq.inc"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) g_pass++; else { g_fail++; printf("  FAIL %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static overhead* o; static SHARE_MEMORY* shm;
static void fresh(bool hostUp = true)
{
    if (!o) { o = (overhead*) calloc(1, sizeof(overhead)); shm = (SHARE_MEMORY*) calloc(1, sizeof(SHARE_MEMORY)); }
    memset(o, 0, sizeof(overhead)); memset(shm, 0, sizeof(SHARE_MEMORY));
    o->pShm = shm; app = o;
    if (!trc[GPBUFID].mutex) trc[GPBUFID].mutex = RtCreateMutex(NULL, FALSE, "errq_test_gp");
    shm->IsysLineStatus.connected[HOST_INDEX] = hostUp ? 1 : 0;
    // InitLocals' part (the overhead object is plain `new`, InitLocals sets these)
    memset(o->errq, 0, sizeof(o->errq)); o->errq_head = 0; o->errq_tail = 0; o->errq_dropped = 0; o->send_error.send = false;
    g_log.clear(); g_host.clear();
}
// GpSendThread's pass, verbatim condition
static void gpPass() { if (HOST_OK && app->ErrQueuePending()) app->SendError(); }
static int apiSev(int sev) { return sev == critical ? 3 : sev == warning ? 1 : 0; }

int main()
{
    printf("ERRQ_LEN %d, MAXERRMBUFSIZE %d\n", ERRQ_LEN, MAXERRMBUFSIZE);

    //--- 1. the rig bug: N GenErrors in one scan through the shared app_err_buf
    printf("\n== burst in one scan through the reused app_err_buf (as ProcessSyncs does)\n");
    for (int N : {1, 2, 3, 8, 32, 63, 64})
    {
        fresh();
        const int sevs[3] = { warning, informational, critical };
        for (int k = 0; k < N; k++)
        {
            sprintf(app_err_buf, "Count corrected: Drop Sync %d -1 trolley (cross-check with Scale 1, msg %d)\n", k % 6 + 1, k);
            o->GenError(sevs[k % 3], app_err_buf);
        }
        sprintf(app_err_buf, "SCRIBBLED after the burst\n");        // the caller reuses the buffer at once
        gpPass();
        bool ok = (int) g_host.size() == N;
        for (int k = 0; k < N && ok; k++)
        {
            char want[160]; sprintf(want, "Count corrected: Drop Sync %d -1 trolley (cross-check with Scale 1, msg %d)\n", k % 6 + 1, k);
            ok = g_host[k].cmd == ERROR_MSG && g_host[k].var == apiSev(sevs[k % 3]) && g_host[k].txt == want;
        }
        printf("  N=%2d: delivered %zu, in order with the right text and severity: %s\n", N, g_host.size(), ok ? "yes" : "NO");
        CHECK(ok, "burst N=%d", N);
        CHECK(!o->send_error.send && !o->ErrQueuePending(), "N=%d: queue not drained", N);
    }
    // the same burst through 15.7.15's GenError / SendError: only the last survives - and its text is whatever app_err_buf holds now
    {
        fresh();
        for (int k = 0; k < 2; k++) { sprintf(app_err_buf, "msg %d\n", k); o->GenError_OLD(warning, app_err_buf); }
        sprintf(app_err_buf, "SCRIBBLED\n");
        if (HOST_OK && o->send_error.send) { o->SendError_OLD(); o->send_error.send = false; }
        printf("  15.7.15, 2 in one scan: delivered %zu: \"%s\"  (the rig: only the last DS1/DS3 correction arrived)\n",
               g_host.size(), g_host.empty() ? "" : std::string(g_host[0].txt, 0, g_host[0].txt.size() - 1).c_str());
        CHECK(g_host.size() == 1, "OLD reproduction");
    }

    //--- 2. the dchserver.log lines are byte-identical to 15.7.15
    {
        std::vector<std::string> a, b;
        const int sevs[4] = { warning, informational, critical, 4 };
        for (int s = 0; s < 4; s++) { fresh(); sprintf(app_err_buf, "text %d\n", s); o->GenError(sevs[s], app_err_buf); a.insert(a.end(), g_log.begin(), g_log.end()); }
        for (int s = 0; s < 4; s++) { fresh(); sprintf(app_err_buf, "text %d\n", s); o->GenError_OLD(sevs[s], app_err_buf); b.insert(b.end(), g_log.begin(), g_log.end()); }
        printf("\n== GenError log lines (warning / informational / critical / logonly) identical to 15.7.15: %s (%zu lines)\n", a == b ? "yes" : "NO", a.size());
        CHECK(a == b, "log lines differ");
    }

    //--- 3. overflow: the oldest are dropped, one summary after the survivors
    printf("\n== overflow before a pass (ERRQ_LEN %d)\n", ERRQ_LEN);
    for (int extra : {1, 10, 100, 1000})
    {
        fresh();
        int N = ERRQ_LEN + extra;
        for (int k = 0; k < N; k++) { sprintf(app_err_buf, "m%d\n", k); o->GenError(warning, app_err_buf); }
        gpPass();
        bool ok = (int) g_host.size() == ERRQ_LEN + 1;
        for (int k = 0; k < ERRQ_LEN && ok; k++) { char w[32]; sprintf(w, "m%d\n", extra + k); ok = g_host[k].txt == w; }
        char sum[80]; sprintf(sum, "%d controller messages dropped (queue full)\n", extra);
        ok = ok && g_host.back().txt == sum && g_host.back().var == 1;
        printf("  %4d messages: delivered the newest %d in order + \"%.*s\": %s\n", N, ERRQ_LEN, (int) strlen(sum) - 1, sum, ok ? "yes" : "NO");
        CHECK(ok, "overflow %d", extra);
        CHECK(!o->ErrQueuePending(), "overflow %d: summary pending", extra);
        // the next message after the summary is normal again
        g_host.clear(); sprintf(app_err_buf, "after\n"); o->GenError(warning, app_err_buf); gpPass();
        CHECK(g_host.size() == 1 && g_host[0].txt == "after\n", "after overflow");
    }

    //--- 4. host not connected: kept, delivered (in order) when it connects
    {
        fresh(false);
        for (int k = 0; k < 10; k++) { sprintf(app_err_buf, "h%d\n", k); o->GenError(informational, app_err_buf); }
        gpPass(); o->SendError();                                    // even called directly: nothing may go out
        size_t n0 = g_host.size();
        shm->IsysLineStatus.connected[HOST_INDEX] = 1;
        gpPass();
        bool ok = n0 == 0 && g_host.size() == 10;
        for (int k = 0; k < 10 && ok; k++) { char w[16]; sprintf(w, "h%d\n", k); ok = g_host[k].txt == w; }
        printf("\n== host not connected: sent while down %zu, after connect %zu in order: %s\n", n0, g_host.size(), ok ? "yes" : "NO");
        CHECK(ok, "host down");
    }

    //--- 5. send mutex held by another thread (WAIT100MS times out): kept, delivered on the next pass
    {
        fresh();
        for (int k = 0; k < 3; k++) { sprintf(app_err_buf, "x%d\n", k); o->GenError(warning, app_err_buf); }
        std::atomic<int> state(0);
        pthread_t th;
        pthread_create(&th, NULL, [](void* p) -> void* {
            std::atomic<int>* st = (std::atomic<int>*) p;
            RtWaitForSingleObject(trc[GPBUFID].mutex, INFINITE); st->store(1);
            while (st->load() != 2) usleep(1000);
            RtReleaseMutex(trc[GPBUFID].mutex); return 0; }, &state);
        while (state.load() != 1) usleep(1000);
        gpPass();                                                    // waits 100 ms, gives up, keeps them
        size_t n0 = g_host.size();
        state.store(2); pthread_join(th, NULL);
        gpPass();
        bool ok = n0 == 0 && g_host.size() == 3 && g_host[0].txt == "x0\n" && g_host[2].txt == "x2\n";
        printf("== send mutex held: sent meanwhile %zu, next pass %zu in order: %s\n", n0, g_host.size(), ok ? "yes" : "NO");
        CHECK(ok, "mutex held");
    }

    //--- 6. a text of MAXERRMBUFSIZE or more: not sent, same log line as 15.7.15; NULL text
    {
        fresh();
        std::string big(300, 'A'); big += "\n";
        sprintf(app_err_buf, "before\n"); o->GenError(warning, app_err_buf);
        o->GenError(warning, (char*) big.c_str());
        sprintf(app_err_buf, "after\n");  o->GenError(warning, app_err_buf);
        o->GenError(warning, NULL);
        g_log.clear(); gpPass();
        bool ok = g_host.size() == 3 && g_host[0].txt == "before\n" && g_host[1].txt == "after\n" && g_host[2].txt == "";
        bool logged = false; for (auto& l : g_log) if (l.compare(0, 19, "Error file overhead") == 0) logged = true;
        printf("== too-long text: skipped (logged \"Error file ...\": %s), neighbours delivered: %s; NULL text sent empty: %s\n",
               logged ? "yes" : "no", ok ? "yes" : "NO", ok ? "yes" : "NO");
        CHECK(ok && logged, "too long / NULL");
    }

    //--- 7. concurrent producers (real threads) against a live consumer
    printf("\n== concurrent producers vs GpSendThread-like consumer\n");
    for (int mode = 0; mode < 3; mode++)
    {
        fresh();
        // mode 0: 8 producers each 1 message / ms (~8,000 / s, far above any real rate) vs a pass every 1 ms:
        //         nothing may be lost (a pass that oversleeps 5 ms still finds < ERRQ_LEN waiting)
        // mode 1 / 2: 8 producers flooding as fast as they can vs a pass every 2 ms / 50 ms (GpSendThread's
        //             real period) - drops are expected; every message must still be delivered or reported
        const int P = 8, M = mode == 0 ? 1000 : 4000;
        const int periodUs = mode == 0 ? 1000 : mode == 1 ? 2000 : 50000;  // consumer pass period
        static int s_pace; s_pace = mode == 0 ? 1000 : 0;
        static std::atomic<bool> stop; stop = false;
        struct Arg { int id, M; };
        std::vector<pthread_t> th(P); std::vector<Arg> args(P);
        pthread_t cons;
        static int s_period; s_period = periodUs;
        pthread_create(&cons, NULL, [](void*) -> void* { while (!stop.load()) { gpPass(); usleep(s_period); } gpPass(); gpPass(); return 0; }, NULL);
        for (int p = 0; p < P; p++)
        {
            args[p] = Arg{p, M};
            pthread_create(&th[p], NULL, [](void* a) -> void* {
                Arg* g = (Arg*) a; char buf[MAXERRMBUFSIZE];
                for (int k = 0; k < g->M; k++)
                {
                    // a self-checking text: producer, sequence, padding of varying length, checksum
                    int pad = (k * 37 + g->id * 11) % 180;
                    int n = sprintf(buf, "P%d #%06d ", g->id, k);
                    for (int q = 0; q < pad; q++) buf[n++] = (char) ('a' + (k + q) % 26);
                    unsigned sum = 0; for (int q = 0; q < n; q++) sum = sum * 31 + (unsigned char) buf[q];
                    sprintf(buf + n, " %08x\n", sum);
                    app->GenError((k & 7) ? warning : informational, buf);
                    if (s_pace) usleep(s_pace);
                }
                return 0; }, &args[p]);
        }
        for (int p = 0; p < P; p++) pthread_join(th[p], NULL);
        stop = true; pthread_join(cons, NULL);
        // verify
        std::vector<int> last(P, -1); long torn = 0, dup = 0, order = 0, got = 0, reported = 0;
        std::vector<std::vector<bool>> seen(P, std::vector<bool>(M, false));
        for (auto& s : g_host)
        {
            unsigned d;
            if (sscanf(s.txt.c_str(), "%u controller messages dropped (queue full)", &d) == 1) { reported += d; continue; }
            int id, k; if (sscanf(s.txt.c_str(), "P%d #%d ", &id, &k) != 2 || id < 0 || id >= P || k < 0 || k >= M) { torn++; continue; }
            size_t sp = s.txt.rfind(' '); unsigned sum = 0, want = 0;
            for (size_t q = 0; q < sp; q++) sum = sum * 31 + (unsigned char) s.txt[q];
            if (sscanf(s.txt.c_str() + sp + 1, "%x", &want) != 1 || want != sum) { torn++; continue; }
            if (seen[id][k]) dup++; seen[id][k] = true;
            if (k <= last[id]) order++; last[id] = k; got++;
        }
        long produced = (long) P * M;
        printf("  %s, consumer every %5d us: produced %ld, delivered %ld, reported dropped %ld (sum %ld), torn %ld, duplicate %ld, out of order %ld\n",
               mode == 0 ? "paced " : "flood ", periodUs, produced, got, reported, got + reported, torn, dup, order);
        CHECK(torn == 0 && dup == 0 && order == 0, "concurrent mode %d: torn %ld dup %ld order %ld", mode, torn, dup, order);
        CHECK(got + reported == produced, "concurrent mode %d: %ld delivered + %ld reported != %ld produced", mode, got, reported, produced);
        if (mode == 0) CHECK(reported == 0, "a fast consumer dropped %ld", reported);
    }

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
