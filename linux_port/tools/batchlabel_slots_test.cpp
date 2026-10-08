//--------------------------------------------------------------------------
//  batchlabel_slots_test.cpp - standalone test for overhead/BatchLabelSlots.h
//
//  Build + run on any host (no controller headers, no hardware):
//      g++ -std=c++98 -Wall -Wextra -O0 -I../overhead batchlabel_slots_test.cpp -o /tmp/bls_test && /tmp/bls_test
//
//  Models the controller's batch-label slot table and the host exchange
//  (310 -> 311, 320 -> 321, 319) with the SAME glue the controller uses in
//  overhead.cpp (GetLabelSlot / ReleaseCutShortLabel / NextBatchNumber), and
//  runs the defect scenarios from the 2026-09-25 rig soak against both the old
//  (15.7.13) behaviour and the fix.
//--------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include "BatchLabelSlots.h"

#define MAXBCHLABELS    64
#define MAXBCHLBLNUM    1000
#define MAXDROPS        32
#define LINE_SHIFT      20
#define BATCH_MASK      0xFFFFF
#define TEMP_BATCH_NUM  1

struct Slot   { int label_step; int pre_label_step; unsigned int seq_num; int drop; };
struct DropSt { unsigned int batch_number; int Batched; };

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, msg) do { if (cond) g_pass++; else { g_fail++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, msg); } } while (0)

//----- One controller line (glue mirrors overhead.cpp) ---------------------
struct Line
{
    int          lineid;
    bool         fixed;                 // false = 15.7.13 behaviour, true = 15.7.14
    Slot         slot[MAXBCHLABELS];
    unsigned int alloc_seq[MAXBCHLABELS];
    unsigned int alloc_ctr;
    DropSt       ds[MAXDROPS];
    unsigned int nxt;
    int          reclaims, noslot, sent310;

    void init(int id, bool f)
    {
        memset(this, 0, sizeof(*this));
        lineid = id; fixed = f; nxt = 1;
    }
    unsigned int mult() const { return (unsigned int) lineid << LINE_SHIFT; }
    void open_nums(unsigned int* o) const { for (int d = 0; d < MAXDROPS; d++) o[d] = ds[d].batch_number; }

    // APPLY_BATCH_NUMBER + LABEL_INFO
    int open(int drp)
    {
        if (ds[drp].batch_number != 0) return -2;
        if (!fixed)
        {
            ds[drp].batch_number = nxt + mult();
            int lp;
            for (lp = 0; lp < MAXBCHLABELS; lp++)
                if (slot[lp].seq_num == 0) break;
            if (lp < MAXBCHLABELS) fill(lp, drp); else { noslot++; lp = -1; }
            if (++nxt >= MAXBCHLBLNUM) nxt = 1;
            return lp;
        }
        unsigned int o[MAXDROPS]; int skipped;
        open_nums(o);
        ds[drp].batch_number = BLS_NextBatchNumber(&nxt, mult(), MAXBCHLBLNUM, o, MAXDROPS, slot, MAXBCHLABELS, &skipped);
        int lp = BLS_FindFree(slot, MAXBCHLABELS);
        if (lp < 0)
        {
            open_nums(o);
            lp = BLS_PickReclaim(slot, MAXBCHLABELS, alloc_seq, o, MAXDROPS);
            if (lp >= 0) { reclaims++; memset(&slot[lp], 0, sizeof(Slot)); }
            else         { noslot++; return -1; }
        }
        alloc_seq[lp] = ++alloc_ctr;
        fill(lp, drp);
        return lp;
    }
    void fill(int lp, int drp)
    {
        slot[lp].label_step = 0; slot[lp].drop = drp + 1; slot[lp].seq_num = ds[drp].batch_number;
    }
    // CLEAR_BCH_INFO
    void clear_if_done(int i)
    {
        if (slot[i].label_step == 1 && slot[i].pre_label_step == 2) memset(&slot[i], 0, sizeof(Slot));
    }
    // SendLabelInfo -> host -> 311 (the host answers the lowest pending slot, 1 per tick)
    bool tick_310()
    {
        for (int i = 0; i < MAXBCHLABELS; i++)
            if (slot[i].seq_num != 0 && slot[i].label_step == 0)
            { sent310++; slot[i].label_step = 1; clear_if_done(i); return true; }
        return false;
    }
    void ack_all_310() { while (tick_310()) {} }
    // last bird of the batch drops -> pre_label_step = 1 -> 320 -> 321
    void last_bird_drops(unsigned int seq)
    {
        int i = BLS_FindSlot(slot, MAXBCHLABELS, seq);
        if (i < 0) return;
        slot[i].pre_label_step = 1;
    }
    void ack_320(unsigned int seq)
    {
        int i = BLS_FindSlot(slot, MAXBCHLABELS, seq);
        if (i < 0) return;
        slot[i].pre_label_step = 2; clear_if_done(i);
    }
    // ReleaseCutShortLabel + CLEAR_DROP_BATCH
    void clear_drop_batch(int drp)
    {
        if (fixed)
        {
            unsigned int bch = ds[drp].batch_number;
            if (bch != 0 && bch != TEMP_BATCH_NUM && (int) (bch >> LINE_SHIFT) == lineid && !ds[drp].Batched)
            {
                int idx = BLS_FindSlot(slot, MAXBCHLABELS, bch);
                if (BLS_MarkCutShort(slot, idx) == BLS_FREE_NOW) memset(&slot[idx], 0, sizeof(Slot));
            }
        }
        ds[drp].batch_number = 0; ds[drp].Batched = 0;
    }
    void complete(int drp) { ds[drp].Batched = 1; }
    // CLEAR_BATCH_RECS (319)
    void cmd_319() { nxt = 1; memset(slot, 0, sizeof(slot)); }
    int used() const { int n = 0; for (int i = 0; i < MAXBCHLABELS; i++) if (slot[i].seq_num) n++; return n; }
};

//----- Scenarios ------------------------------------------------------------

// 1. Operator resets a batch mid-way, over and over (the rig's leak).
static void t_cut_short_leak(bool fixed, int* noslot_out, int* used_out)
{
    Line L; L.init(1, fixed);
    for (int i = 0; i < 200; i++)
    {
        L.open(4);          // drop 5 opens a batch
        L.ack_all_310();    // host opens the label
        L.clear_drop_batch(4);  // RESET_BATCH before the batch completes
    }
    *noslot_out = L.noslot; *used_out = L.used();
}

// 2. Cut short BEFORE the host answered the 310: the 310 must still go out.
static void t_cut_short_before_ack()
{
    Line L; L.init(1, true);
    L.open(2);
    unsigned int seq = L.ds[2].batch_number;
    L.clear_drop_batch(2);                          // reset before any 311
    int i = BLS_FindSlot(L.slot, MAXBCHLABELS, seq);
    CHECK(i >= 0, "cut-short slot kept until the host has had its 310");
    CHECK(i >= 0 && L.slot[i].label_step == 0 && L.slot[i].pre_label_step == 2, "marked: 310 pending, no 320 expected");
    CHECK(L.tick_310(), "310 still sent after the reset");
    CHECK(L.sent310 == 1, "exactly one 310 for the batch");
    CHECK(BLS_FindSlot(L.slot, MAXBCHLABELS, seq) < 0, "slot freed by the 311");
}

// 3. A COMPLETED batch reset before its last bird physically drops keeps its
//    slot and its 320, exactly as before.
static void t_completed_keeps_prelabel()
{
    Line L; L.init(1, true);
    L.open(7); L.ack_all_310();
    unsigned int seq = L.ds[7].batch_number;
    L.complete(7);                                  // last bird assigned (Batched = 1)
    L.clear_drop_batch(7);                          // operator resets the full drop
    CHECK(BLS_FindSlot(L.slot, MAXBCHLABELS, seq) >= 0, "completed batch keeps its slot for the 320");
    L.last_bird_drops(seq);
    int i = BLS_FindSlot(L.slot, MAXBCHLABELS, seq);
    CHECK(i >= 0 && L.slot[i].pre_label_step == 1, "320 is sent when the last bird drops");
    L.ack_320(seq);
    CHECK(BLS_FindSlot(L.slot, MAXBCHLABELS, seq) < 0, "freed by the 321, as before");
}

// 4. Normal complete lifecycle is unchanged (fixed == old).
static void t_normal_unchanged()
{
    for (int f = 0; f < 2; f++)
    {
        Line L; L.init(1, f != 0);
        for (int n = 0; n < 500; n++)
        {
            int d = n % 20;
            L.open(d); L.ack_all_310();
            unsigned int seq = L.ds[d].batch_number;
            L.complete(d); L.clear_drop_batch(d);   // loop clear / reset after completion
            L.last_bird_drops(seq); L.ack_320(seq);
        }
        CHECK(L.used() == 0 && L.noslot == 0 && L.reclaims == 0, "complete batches free normally, no reclaim");
        CHECK(L.sent310 == 500, "one 310 per batch");
    }
}

// 5. Table full of finished-but-in-flight slots (the rig's scale-2 loops):
//    reuse the oldest acknowledged one; never a 310-pending or open one.
static void t_reclaim()
{
    Line L; L.init(1, true);
    for (int n = 0; n < MAXBCHLABELS; n++)      // 64 completed batches, last birds still travelling
    {
        int d = n % 30;
        L.open(d); L.ack_all_310(); L.complete(d); L.clear_drop_batch(d);
    }
    CHECK(L.used() == MAXBCHLABELS, "all 64 slots held by in-flight completed batches");
    unsigned int oldest = L.slot[0].seq_num;
    L.open(31);                                  // before 15.7.14: silently no 310
    CHECK(L.noslot == 0 && L.reclaims == 1, "new batch gets a reclaimed slot");
    CHECK(BLS_FindSlot(L.slot, MAXBCHLABELS, oldest) < 0, "oldest slot was the one reused");
    CHECK(BLS_FindSlot(L.slot, MAXBCHLABELS, L.ds[31].batch_number) >= 0, "new batch is queued for its 310");

    // A slot whose 310 is not yet answered, or whose batch is still open, is never reused.
    Line M; M.init(1, true);
    for (int n = 0; n < MAXBCHLABELS; n++)
    {
        int d = n % 32;
        if (M.ds[d].batch_number) M.clear_drop_batch(d);  // cut short, 310 unanswered -> kept (frees on 311)
        M.open(d);
    }
    CHECK(M.used() == MAXBCHLABELS, "64 slots, every 310 unanswered");
    M.clear_drop_batch(0);
    int r = M.open(0);
    CHECK(r == -1 && M.noslot == 1 && M.reclaims == 0, "no slot taken from a 310 the host has not answered");
    M.ack_all_310();
    CHECK(M.used() <= MAXDROPS, "answered cut-short slots free on the 311; only open batches remain");
    int before = M.reclaims;
    // open batches (current on a drop) are not reclaim candidates
    unsigned int o[MAXDROPS]; M.open_nums(o);
    for (int i = 0; i < MAXBCHLABELS; i++)
        if (M.slot[i].seq_num) CHECK(BLS_IsOpen(M.slot[i].seq_num, o, MAXDROPS), "remaining slots are open batches");
    CHECK(M.reclaims == before, "no reclaim needed");
}

// 6. 319 / RESET_BCH_NUMS must not hand out a number still open on a drop.
static void t_renumber_after_319(bool fixed, int* collisions)
{
    Line L; L.init(1, fixed);
    L.nxt = 40;
    L.open(0); L.open(1); L.open(2);            // drops 1-3 hold 40, 41, 42 (open, mid-batch)
    L.ack_all_310();
    L.cmd_319();                                // Print Pending Batches
    L.nxt = 40;                                 // worst case: restart lands on the open numbers
    *collisions = 0;
    for (int d = 3; d < 10; d++)
    {
        L.open(d);
        for (int k = 0; k < 3; k++)
            if (L.ds[d].batch_number == L.ds[k].batch_number) (*collisions)++;
    }
}

// 7. With nothing in use the numbering is exactly the old sequence, wrap included.
static void t_sequence_identical()
{
    unsigned int nxt_old = 995, nxt_new = 995;
    unsigned int o[MAXDROPS]; memset(o, 0, sizeof(o));
    Slot s[MAXBCHLABELS]; memset(s, 0, sizeof(s));
    bool same = true;
    for (int n = 0; n < 3000; n++)
    {
        unsigned int old_num = nxt_old + (2u << LINE_SHIFT);
        if (++nxt_old >= MAXBCHLBLNUM) nxt_old = 1;
        int sk;
        unsigned int new_num = BLS_NextBatchNumber(&nxt_new, 2u << LINE_SHIFT, MAXBCHLBLNUM, o, MAXDROPS, s, MAXBCHLABELS, &sk);
        if (old_num != new_num || nxt_old != nxt_new || sk != 0) same = false;
    }
    CHECK(same, "same numbers as 15.7.13 when nothing is in use (incl. 999 -> 1 wrap)");
}

// 8. InterSystems: another line's number, or the slave's TEMP number, is never
//    released here (the slot lives on the master); other lines' numbers never block.
static void t_isys()
{
    Line L; L.init(2, true);                     // this is line 2
    L.ds[5].batch_number = (1u << LINE_SHIFT) + 17;   // slave copy of line-1 master's batch 17
    L.clear_drop_batch(5);
    CHECK(L.used() == 0, "slave clear touches no slot");
    L.ds[6].batch_number = TEMP_BATCH_NUM;
    L.clear_drop_batch(6);
    CHECK(L.used() == 0, "TEMP number ignored");
    unsigned int o[MAXDROPS]; memset(o, 0, sizeof(o));
    o[0] = (1u << LINE_SHIFT) + 5;               // line 1's batch 5 open here as a slave copy
    Slot s[MAXBCHLABELS]; memset(s, 0, sizeof(s));
    unsigned int nxt = 5; int sk;
    unsigned int n = BLS_NextBatchNumber(&nxt, 2u << LINE_SHIFT, MAXBCHLBLNUM, o, MAXDROPS, s, MAXBCHLABELS, &sk);
    CHECK(n == (2u << LINE_SHIFT) + 5 && sk == 0, "another line's number does not block this line's");
}

// 9. pre-label already under way is left alone.
static void t_prelabel_active()
{
    Slot s[1]; memset(s, 0, sizeof(s));
    s[0].seq_num = 9; s[0].label_step = 1; s[0].pre_label_step = 1;
    CHECK(BLS_MarkCutShort(s, 0) == BLS_PRELABEL_ACTIVE && s[0].pre_label_step == 1, "pre_label_step 1 untouched");
    CHECK(BLS_MarkCutShort(s, -1) == BLS_NOT_FOUND, "missing slot is a no-op");
}

// 10. Clear totals / mode change cutting many batches short at once.
static void t_clear_totals()
{
    Line L; L.init(1, true);
    for (int d = 0; d < 20; d++) L.open(d);
    L.ack_all_310();
    L.complete(3);                               // one is full, last bird in flight
    unsigned int full = L.ds[3].batch_number;
    for (int d = 0; d < MAXDROPS; d++) L.clear_drop_batch(d);   // CLEAR_TOTALS loop
    CHECK(L.used() == 1 && BLS_FindSlot(L.slot, MAXBCHLABELS, full) >= 0, "only the completed batch keeps its slot");
}

int main()
{
    int ns_old, used_old, ns_new, used_new, col_old, col_new;

    printf("batch label slot tests (BatchLabelSlots.h)\n");

    t_cut_short_leak(false, &ns_old, &used_old);
    t_cut_short_leak(true,  &ns_new, &used_new);
    printf("  200 mid-batch resets:   15.7.13 slots used %d, batches with NO 310 %d | 15.7.14 slots used %d, no-310 %d\n",
           used_old, ns_old, used_new, ns_new);
    CHECK(used_old == MAXBCHLABELS && ns_old == 200 - MAXBCHLABELS, "old code reproduces the leak (64 held, rest silently unsent)");
    CHECK(used_new == 0 && ns_new == 0, "fix: every cut-short slot freed, every batch sent");

    t_renumber_after_319(false, &col_old);
    t_renumber_after_319(true,  &col_new);
    printf("  319 renumber vs open batches: 15.7.13 collisions %d | 15.7.14 collisions %d\n", col_old, col_new);
    CHECK(col_old > 0, "old code reproduces the 319 number reuse");
    CHECK(col_new == 0, "fix: no number reused while open");

    t_cut_short_before_ack();
    t_completed_keeps_prelabel();
    t_normal_unchanged();
    t_reclaim();
    t_sequence_identical();
    t_isys();
    t_prelabel_active();
    t_clear_totals();

    printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
