//--------------------------------------------------------------------------
//  errq_harness.h - route a test harness's controller messages through the REAL 15.7.16
//  GenError -> queue -> SendError path (the harness's .sh cuts those functions into errq.inc and
//  defines XC_REAL_ERRQ when the source has the queue).
//
//  Produced  = every "GenError: " / "GenStatus:  " line GenError writes to the log (RtPrintf).
//  Delivered = every SendHostMsg(ERROR_MSG, ...) SendError makes - handed to the harness's own
//              message list through errq_deliver, so its analysis now sees what the HOST sees.
//  errq_drain() is GpSendThread's pass (HOST_OK && pending -> SendError), run after every scan;
//  errq_end() checks that the host got every produced message, in order, with the same text.
//  Include after the controller headers, in place of the harness's GenError / RtPrintf stubs.
//--------------------------------------------------------------------------
#pragma once
#include <string>
#include <vector>

volatile int   g_sentinel_magic = 0;          // isPShmValid(): early-init path
volatile void* g_sentinel_pShm  = 0;

static std::vector<std::string> g_errq_prod, g_errq_sent;
static void (*errq_deliver)(const char* txt) = 0;
static long g_errq_runs = 0, g_errq_msgs = 0, g_errq_bad = 0;

int RtPrintf(const char* f, ...)
{
    char b[2048]; va_list ap; va_start(ap, f); vsnprintf(b, sizeof b, f, ap); va_end(ap);
    if (strncmp(b, "GenError: ", 10) == 0)        g_errq_prod.push_back(b + 10);
    else if (strncmp(b, "GenStatus:  ", 12) == 0) g_errq_prod.push_back(b + 12);
    return 0;
}
int overhead::SendHostMsg(int cmd, int, BYTE* data, int len)
{
    std::string t((char*) data, len);
    g_errq_sent.push_back(t);
    if (cmd == ERROR_MSG && errq_deliver) errq_deliver(t.c_str());
    return 0;
}

#include "errq.inc"

static void errq_begin(overhead* o)
{
    o->pShm->IsysLineStatus.connected[HOST_INDEX] = 1;          // a host is connected
    static HANDLE m = 0;                                          // run() clears trc[]: reinstall one mutex
    if (!m) m = RtCreateMutex(NULL, FALSE, "errq_harness_gp");
    trc[GPBUFID].mutex = m;
    memset(o->errq, 0, sizeof(o->errq));                          // InitLocals' part
    o->errq_head = o->errq_tail = 0; o->errq_dropped = 0; o->send_error.send = false;
    g_errq_prod.clear(); g_errq_sent.clear();
}
static inline void errq_drain(overhead* o)
{
    overhead* keep = app; app = o;                                // HOST_OK reads app->pShm
    if (HOST_OK && app->ErrQueuePending()) app->SendError();
    app = keep;
}
static bool errq_end(overhead* o)
{
    errq_drain(o);
    g_errq_runs++; g_errq_msgs += (long) g_errq_prod.size();
    bool ok = g_errq_prod == g_errq_sent;
    if (!ok) g_errq_bad++;
    return ok;
}
static void errq_report()
{
    printf("controller messages through the real GenError queue: %ld runs, %ld produced, every one delivered in order: %s\n",
           g_errq_runs, g_errq_msgs, g_errq_bad ? "NO" : "yes");
}
