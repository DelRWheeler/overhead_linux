#pragma once

//--------------------------------------------------------------------------
//  BatchLabelSlots.h - batch-label slot bookkeeping (15.7.14)
//
//  Pure logic, no OS / hardware / shared-memory access, so it ports to any
//  controller platform unchanged and can be unit-tested on its own
//  (tools/batchlabel_slots_test.cpp). The overhead class wraps these with
//  its tracing and host error reporting (overhead.cpp, "Batch label slots").
//
//  The slot table is app->batch_label.info[MAXBCHLABELS] (print_info). A slot
//  is in use while seq_num != 0 and is freed by CLEAR_BCH_INFO once BOTH
//      label_step     == 1   the host answered PRN_BCH_REQ (310) with 311
//      pre_label_step == 2   the host answered PRN_BCH_PRE_REQ (320) with 321
//  pre_label_step goes 0 -> 1 only when the batch's LAST bird physically
//  drops (overhead.cpp, "Send prelabel message to host"), so a batch that is
//  cleared before it completes never reaches 1 and its slot was held until
//  CLEAR_BATCH_RECS (319) or a controller restart. At MAXBCHLABELS held
//  slots LABEL_INFO silently found no slot: the batch was numbered and its
//  birds stamped, but no 310 was ever sent.
//
//  Neither host acts on 320: Delphi only acknowledges it
//  (MessageInterface.pas PRN_BCH_PRE_REQ, the frmPrintLabels.Add call is
//  commented out) and the Go API only logs and acknowledges it
//  (client.go handleBatchPreLabel). So once 311 is in, a slot has no job
//  the host can observe except to produce that 320.
//
//  label_step / pre_label_step are NOT on the wire: PRN_BCH_REQ sends
//  print_info from seq_num onward (sizeof(print_info) - 2 ints).
//
//  Written as templates over the slot type so the test can supply its own
//  struct with the same field names; SLOT is print_info in the controller.
//--------------------------------------------------------------------------

typedef unsigned int bls_uint;

// Results of BLS_MarkCutShort
#define BLS_NOT_FOUND        0   // no slot holds this number (never queued, or 319/restart cleared it)
#define BLS_FREE_NOW         1   // host already answered 310 -> caller frees the slot now
#define BLS_FREE_ON_ACK      2   // 310 still pending -> slot frees itself when 311 arrives
#define BLS_PRELABEL_ACTIVE  3   // last bird already dropped, 320 exchange under way -> untouched

//----- index of the slot holding batch number seq, or -1
template <class SLOT>
static inline int BLS_FindSlot(const SLOT* info, int nslots, bls_uint seq)
{
    int i;
    if (seq == 0)
        return -1;
    for (i = 0; i < nslots; i++)
        if (info[i].seq_num == seq)
            return i;
    return -1;
}

//----- index of a free slot, or -1
template <class SLOT>
static inline int BLS_FindFree(const SLOT* info, int nslots)
{
    int i;
    for (i = 0; i < nslots; i++)
        if (info[i].seq_num == 0)
            return i;
    return -1;
}

//----- true if full batch number num is some drop's current batch number
static inline bool BLS_IsOpen(bls_uint num, const bls_uint* open_nums, int n_open)
{
    int i;
    for (i = 0; i < n_open; i++)
        if (open_nums[i] == num)
            return true;
    return false;
}

//----- The batch in slot idx was cleared before it completed (no last-in-batch
//      bird exists, so pre_label_step can never become 1 and no 320 will ever
//      be sent). Mark the pre-label exchange as finished (pre_label_step = 2)
//      so the ordinary CLEAR_BCH_INFO rule frees the slot:
//        - now, if the host has already answered the 310 (label_step == 1);
//        - otherwise when its 311 arrives. The 310 is still sent, so the host
//          still opens the label exactly as before (Delphi can then flush it
//          with Print Pending Batches); only the slot leak goes away.
//      The caller performs the free for BLS_FREE_NOW.
template <class SLOT>
static inline int BLS_MarkCutShort(SLOT* info, int idx)
{
    if (idx < 0)
        return BLS_NOT_FOUND;
    if (info[idx].pre_label_step != 0)
        return BLS_PRELABEL_ACTIVE;
    info[idx].pre_label_step = 2;
    return (info[idx].label_step == 1) ? BLS_FREE_NOW : BLS_FREE_ON_ACK;
}

//----- The table is full. Choose a slot that can be reused without any
//      difference the host can see:
//        - label_step == 1: the host has had its 310 and opened the label.
//          A slot still waiting for 311 is NEVER taken - that would lose the
//          open and with it the whole batch on the host.
//        - its batch is not any drop's current batch number: the batch is
//          finished or was cut short, so all that is lost is the 320 that
//          neither host acts on.
//      Oldest allocation first (alloc_seq, compared with wrap-safe signed
//      difference). Returns -1 if nothing qualifies.
template <class SLOT>
static inline int BLS_PickReclaim(const SLOT* info, int nslots, const bls_uint* alloc_seq,
                                  const bls_uint* open_nums, int n_open)
{
    int i;
    int victim = -1;
    for (i = 0; i < nslots; i++)
    {
        if (info[i].seq_num == 0)
            continue;
        if (info[i].label_step != 1)
            continue;
        if (BLS_IsOpen(info[i].seq_num, open_nums, n_open))
            continue;
        if ( (victim < 0) || ((int) (alloc_seq[i] - alloc_seq[victim]) < 0) )
            victim = i;
    }
    return victim;
}

//----- Next batch number. Same sequence as before (nxt, nxt+1, ... wrapping
//      from maxnum-1 back to 1), except that a number which is still some
//      drop's current batch number, or still held in a label slot, is skipped.
//      Before this, CLEAR_BATCH_RECS (319) / RESET_BCH_NUMS restarted *nxt at 1
//      while drops kept their open numbers, so a new batch could be given the
//      number of a batch still open on another drop and the host (keyed by
//      number, as Delphi is) merged the two. *skipped returns how many numbers
//      were passed over. If every number were in use (cannot happen: at most
//      MAXDROPS + MAXBCHLABELS of 999) the original candidate is used.
template <class SLOT>
static inline bls_uint BLS_NextBatchNumber(bls_uint* nxt, bls_uint line_mult, bls_uint maxnum,
                                           const bls_uint* open_nums, int n_open,
                                           const SLOT* info, int nslots, int* skipped)
{
    bls_uint first, cand, tries;

    cand = *nxt;
    if ( (cand == 0) || (cand >= maxnum) )
        cand = 1;
    first = cand;
    *skipped = 0;

    for (tries = 1; tries < maxnum; tries++)
    {
        if ( !BLS_IsOpen(cand + line_mult, open_nums, n_open) &&
             (BLS_FindSlot(info, nslots, cand + line_mult) < 0) )
            break;
        (*skipped)++;
        if (++cand >= maxnum)
            cand = 1;
        if (cand == first)
            break;
    }

    *nxt = cand + 1;
    if (*nxt >= maxnum)
        *nxt = 1;

    return cand + line_mult;
}
