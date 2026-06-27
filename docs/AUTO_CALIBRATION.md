# Auto-Calibration (continuous span verify against a permanent known weight)

Status: **design / spec, 2026-06-27.** Not started.
Scope: **SandCat / Linux only** (competitor-replacement feature). EPM-19 keeps working with the
interface and does NOT get this; the `rtx_source` is never modified. Builds on the **scale sync
offset** ([[scale_offset_feature]]) and the single-sensor work.

## Why
The competitor's plants rely on "auto calibration": a known weight rides the line permanently, the
system reads it every revolution and keeps the weigher trimmed all day, deleting the morning manual
Auto Span (~5 min/line, ~20 min on a 4-line plant). This is parity/table-stakes for the SandCat
replacement, and we can make ours *safer* than his (see "the key insight" below).

## Physical setup (agreed with Del)
- With **scale offset −2 live**, the trolley physically on the weigh deck when the system zeros is the
  zero trolley. The controller resets `trolly_counters→0` and `shackleno→1` at the flag
  (overhead.cpp:10516-10518) — so **trolley 0 == shackle-number 1**, off by one. We do NOT change this.
- **Trolley 0 stays empty** — it is the existing check-zero (`AutoZero`/`AutoBias`), untouched.
- A normal shackle is **welded to a known weight** and rides permanently on **trolley 1** (the next
  trolley over, normally empty, not a flag trolley), built so it never hangs or strikes anything.
- **The reference shackle is FIXED: trolley 1 == shackle-number 2** — it never changes (trolley 0 ==
  shackleno 1 from the reset). The controller reads `shackleno 2`. (Confirm it actually reads the
  known weight at bring-up, but it's a hardcoded constant, not auto-pinned.)

## The two-point principle + the key insight
Zero and span have different jobs and different drift signatures:
- **Zero bias** (`AutoBias[scale]`, `AutoZero()` overhead.cpp:9114) = the offset/baseline; it moves
  regularly (load-cell zero drift) and is already corrected from the empty zero trolley. **Untouched.**
- **Span bias** (`scl_set.SpanBias[scale]`, shmID 32) = the gain; physically stable, normally a
  one-time setting.
- **Offset drift moves the empty AND the known weight by the same amount; gain drift moves only the
  known weight.** So a naive "known weight drifted → move span" loop (likely the competitor's) wrongly
  attributes load-cell *offset* drift to *gain* and corrupts span all day.
- **We avoid that for free:** the per-shackle stored weight (`ShackleStatus[shk].weight[scale]`) is
  already **AutoBias-subtracted before span bias** (overhead.cpp:6181 then 6238). So reading the
  reference shackle's stored weight already has the offset removed — the span residual we see is
  **only genuine gain error.** (Requires zero bias active; if off, fall back to subtracting the empty
  trolley reading explicitly. Flag/alarm if zero bias is off.)

## Weight math (overhead.cpp ProcessWeight, ModeRun)
```
w  = raw_counts
w -= AutoBias[scale]                       // 6181  zero/offset (from empty trolley)
w -= TARE_SHACKLE(scale)                    // 6238  per-shackle tare  (== 0 for the reference shackle)
w += (w * SpanBias[scale]) / 1000           // 6238-6242  gain
```
For the reference shackle (no tare): `final_ref = (raw − AutoBias) * (1 + SpanBias/1000)`.
We want `final_ref == known_weight`, so the target span is:
```
SpanBias_target = 1000 * ( known_weight / (raw_ref − AutoBias) − 1 )
```

## The feature — two parts (Del's framing)

### Part 1 — at Calibrate Shackle Tares (modified `AutoTare()`, overhead.cpp:6636)
Run the manual tare calibration exactly as before, but:
- **Do NOT write a tare for the reference shackle** (skip `TARE_SHACKLE(s)=…` when
  `WeighShackle[s] == reference shackle`; leave its tare 0).
- Instead **capture the reference shackle's averaged reading** over the TareTimes passes, and at
  completion **set/verify SpanBias** from it vs the entered known weight (the formula above).
- This replaces hanging the weight 4× on the Auto Span screen — the morning step goes away.

### Part 2 — ongoing every revolution (new, near ProcessWeight/AutoZero)
- Each time the reference shackle is weighed, compute
  `span_error = final_ref − known_weight` (final_ref already offset-cancelled via AutoBias).
- **Monitor, only correct on a true residual** — never a constant chase:
  - **Deadband:** ignore |span_error| below X% (e.g. 0.5–1%).
  - **Slow integral:** move SpanBias a small fraction of the error per accepted sample (minutes to
    converge), so it tracks slow gain drift and is deaf to per-revolution noise.
  - **Outlier rejection:** discard a reading outside a tight band of the known weight (weight swung,
    transient, mis-zero).
  - **Hard clamp:** limit total SpanBias deviation from the morning baseline (e.g. ±2–3%). If it wants
    to exceed → **alarm and HOLD** at the last good span (weight shifted/fell, cell dying) — do not chase.
  - **Sanity gates:** only act while running steadily; require zero bias active; require the reference
    reading present (near-zero reference = weight gone → alarm).

## Data + delivery
- **New host setting — reference known weight** (shackle + welded weight together), per line, entered
  on **Line Setup**, stored consistent with the Auto Span known-weight representation. Pushed to the
  controller over a **new spare shmID** (like the scale offset; relays through the interface with no
  rebuild). Also likely: a reference-shackle number (or "auto-pin from observation") + enable flag.
- **Span bias** is written by the controller directly (`scl_set.SpanBias[scale]`, shmID 32, SCL group
  — persists to disk) — this is new (today the host computes span during Auto Span and pushes it).
- **Status/alarm back to host** (drift, alarm-and-hold, zero-bias-off, weight-missing). If a NEW IPC
  message → the 4-places rule (ipc.h + interface rebuild + SendHostMsg case + API whitelist). Prefer
  riding an existing channel (error/status) if it fits.

## Phasing (Del)
1. **Controller** — the AutoTare mod (Part 1), the per-rev monitor/trim (Part 2), the new shmID(s),
   the guards, status. SandCat-gated.
2. **Interface** — relay the known-weight shmID (automatic) + any new status/alarm IPC id.
   Plus the host/web/DB slice: Line Setup reference-weight field, enable, alarm display, DB column.
3. **Simulator (siggen)** — inject a **steady known weight on the reference shackle every revolution**
   (the permanent welded weight), on the **same shackle the controller reads** (pin by observation).
   Plus a **drift knob** to prove the logic: *offset drift* = shift empty(0)+known(1) together →
   controller corrects ZERO, leaves span; *gain drift* = scale known(1) alone → controller moves SPAN.

## UI gating + history (Del, 2026-06-27)
- **Auto Span ↔ auto-cal are mutually exclusive (a toggle).** Enabling auto-cal **grays out / disables
  the existing Auto Span screen**; disabling auto-cal restores normal Auto Span. (This resolves the
  "Part 1 replace-vs-coexist" question — they never run at once; the operator picks one.)
- **Readings log table — new DB table** (e.g. `auto_cal_log`): a time series of the reference
  shackle's readings so we (and the plant) can report on how it's actually doing. Columns:
  `line_id, scale_number, recorded_at, measured_weight, known_weight, span_error, span_bias`
  (current), `zero_bias` (AutoBias, for context), `adjusted` (bool — did it nudge span this reading),
  `note/flag` (ok / held / zero-bias-off / weight-missing). **One record per crossing:** the
  reference shackle is a single shackle, so it crosses the scale **once per revolution** (~6.6 min at
  1189 shackles, faster at lower counts → ~150/day/line). The controller emits a reading record on
  each crossing — like a drop record — and the host stores it. Event-driven, not timer-sampled; the
  rate is inherently modest. **Truncatable on the Database Management screen** (same pattern as the
  lot_history / production_data trims); optional auto-retention (N days) like `lot_history_days`.

## Decisions (Del, 2026-06-27 cont.)
- **Known weight = the existing Auto Span value, REUSED** (same stored value/units). Because enabling
  auto-cal grays out the Auto Span screen, put a **duplicate known-weight input on the auto-cal
  screen** — both fields edit the same underlying value; you enter it on whichever screen is active.
- **Reference shackle FIXED = trolley 1 = shackle-number 2** (never changes). No auto-pin — read
  shackleno 2.
- **Clamp = editable field, default 2%**, on the same screen as the known weight (tunable per plant —
  "see how it does in a real plant"). Deadband / gain / retention stay conservative internal defaults.
- **Screen:** auto-cal **enable toggle + known-weight (duplicate) + clamp %** live together, per line.
  Proposed home: a small **"Auto Calibration" panel under the calibration menu** (`calibrate`
  permission), since it gates the Auto Span calibration screen — confirm vs putting it on Line Setup.

## Open items to confirm
- Reading record: a dedicated IPC record (like DREC, emitted on each reference crossing, host inserts
  a row), with alarm/hold as a flag field on it. (New IPC id → the 4-places rule.)
- Optional auto-retention days for the log table (default: keep, manual truncate only).
- Final screen home for the auto-cal controls (Auto-Cal panel vs Line Setup).
