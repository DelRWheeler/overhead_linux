# Auto-Calibration (permanent known weight: cal-time span + drift monitor)

## ★ REDESIGN 2026-07-02 — cal-time span ONLY + record/alarm (SUPERSEDES continuous-trim) ★

Del's rethink: the old manual flow made operators hang a known weight 6-8× per line and averaged
the first four to build the span bias. This feature keeps a known weight welded on the line 100% of
the time, but we **do NOT trim span continuously** (that risked drifting span all day). Instead:

**1. Span bias is created ONLY during Calibrate Shackle Tares** (the existing menu; the operator
   already picks the number of cycles there — most plants use **2**, sometimes 1, sometimes 3-4,
   depending on time before birds exit the chiller):
   - **N = 1** → span bias from the **first** reference-shackle reading.
   - **N ≥ 2** → **average** of the first N reference readings → span bias.
   - The N readings that fed the span are **marked as "used"** in the log; the reference shackle is
     still skipped for tare (it is not empty).
   - This is the existing controller **Part 1** (`AutoTare`) — it already averages the reference over
     the tare cycles and sets `SpanBias`. It stays. N == the existing tare cycle count (no new setting).

**2. Between calibrations the span is FROZEN.** The reference is read every revolution ONLY to be
   **recorded** (`auto_cal_log`) — never to adjust span. The old **Part 2 continuous trim
   (`AutoCalMonitor`: deadband / slow integral / clamp / alarm-and-HOLD span adjustment) is REMOVED**
   and replaced by **record + drift check**:
   - Once per revolution, when the reference crosses the scale, compare `measured` vs `known`.
   - If `|measured − known| / known > threshold%` → set a **drift alarm** flag on that reading; the
     host raises it to the web as a **timed red box** (auto-dismiss; re-fires once per revolution
     while still out — ~once per 5+ min on a ~1000-shackle line ≤190 SPM, so not spammy).
   - Span is **not** touched by the drift check.

**3. Threshold is operator-editable** — a **drift %** field on the **same screen as the known weight**
   (Line Setup), default **2%**. Reuse `AutoCalClampPpt` / `auto_cal_clamp_pct` as this threshold.

**4. List Weights:** the reference-shackle reading is shown **YELLOW** (it currently colors each
   shackle red if drop-assignment==0, green if assigned; the reference is the one shackle that is
   OMITTED from drops, so make it the single **yellow** row so operators can eyeball its drift).

**5. New Calibration Log screen** — view `auto_cal_log`: the calibration events (marked "used"
   readings) and the ongoing per-rev monitoring readings, per line/scale, with the drift-alarm ones
   highlighted. (This screen was already on the open list from the 6/30 session.)

### Code deltas vs the (implemented) continuous-trim version below
| Layer | Change |
|---|---|
| Controller `overhead.cpp` (SandCat) | Keep Part 1 (cal-time span; verify N=1 = first reading). **Gut `AutoCalMonitor`**: drop the integral/deadband/clamp span *write*; keep reading the reference + emitting `AUTO_CAL_REC` (cmd 334) every crossing; set the alarm flag purely from `|measured−known| > threshold` (no HOLD-on-span, no span change). |
| API + DB | `auto_cal_log` already fits (add/repurpose `adjusted`→"used in cal" + a drift-alarm flag value). Drift % is `auto_cal_clamp_pct` surfaced as an editable Line Setup field. Emit a drift event to the web when a logged reading is flagged. |
| Web | Rev-count already lives on Calibrate Shackle Tares. Add: editable drift-% next to known weight (Line Setup); timed red drift box; yellow reference on List Weights; the Calibration Log screen. |

Everything from here down describes the **prior continuous-trim implementation** — kept for reference;
the parts marked REMOVED above no longer apply.

---

# Auto-Calibration (continuous span verify against a permanent known weight) — PRIOR MODEL

Status: **ALL 3 PHASES implemented + RIG-PROVEN on the single-line SandCat (line 1, .11), 2026-06-28.**
End-to-end verified: the reference reading flows controller `AutoCalMonitor` -> `AUTO_CAL_REC`
(cmd 334) -> rebuilt interface relay -> host parse -> `auto_cal_log`; the reference reads on the
right shackle (`measured 5.04 lb, flag=0`) and is OMITTED from drops (0 phantom drops). Remaining:
deploy to line 2 (.12), the offset-vs-gain drift demonstration, and the full span-convergence soak.

## Reference-shackle mapping + drop omission (LEARNED ON THE RIG 2026-06-28 — read carefully)
- **The reference is shackle 2 = Trolley 1** (Trolley 0 = the empty zero/check trolley = shackle 1;
  the known weight rides the very next trolley = Trolley 1 = shackle 2). The zero TAB is on Trolley 3
  (= shackle 4, can't hold a shackle). Single-sensor: tab = a double-pulse (trolley pulse + a second
  blip one trolley-width later); standard: the zero flag spans at most TWO trolleys, never several.
- **`AutoCalRefShackle` is host-pushed (shmID 103), default 2, pinned by OBSERVATION** (List Weights).
  The controller monitors it for span AND omits it from drop assignment. NOT derived on paper.
- **Omission is at the single chokepoint `AssignDrop()`** (overhead.cpp): every path — local
  `FindDrops`, InterSystems (drop manager / `CHECK_DROP_LOCAL`), and gib/grade drops — funnels
  through `AssignDrop`, so one guard there guarantees the reference is never assigned a drop,
  `AddBird`-counted, batched, or dropped downstream. Off by default = byte-identical legacy.
- **Simulator alignment (siggen, test-tool only):** near the zero flag the siggen pins its sync
  counter across a ~2-shackle window, so the known weight injected on `refCounter` can bleed onto
  two shackles. Fixed with a single-shackle latch + a configurable `refSubShackle` (which shackle of
  the window). On this rig: `refCounter=0, refSubShackle=1` lands the weight on shackle 2.
  (A leftover `if refCounter==0 {refCounter=1}` in the siggen API was forcing it off shackle 2 — removed.)

(Original status / implementation detail below.)

Status (earlier): **Phase 1 (controller) CORE implemented + compiles (x86_64), 2026-06-28.** NOT deployed,
NOT committed. Safe to deploy anytime (AutoCalEnable zero-inits to 0 = byte-identical legacy until
turned on), but pointless until host (enable/known-weight/clamp push) + siggen (inject known weight
on the reference shackle) land. Remaining in Phase 1: the per-crossing reading-record IPC + pushing
`AutoCalAlarm` back to the host (overlaps Phase 2's 4-places rule).

## Implementation status (2026-06-28)
**Controller, `overhead_controller/linux_port` (Phase 1 core, compiles):**
- Data model: `SHARE_MEMORY` gained `AutoCalEnable` / `AutoCalKnownWeight (__int64)` /
  `AutoCalClampPpt` (host-pushed) + `AutoCalSpanBaseline[MAXSCALES]` / `AutoCalAlarm[MAXSCALES]`
  (controller-owned), appended at struct end (no offset shift). New spare shmIDs **100/101/102**
  (`AUTOCAL_ENABLE/KNOWN_WT/CLAMP`), `ALL_SHM_IDS` `+1`→`+4`, shm_tbl rows (NO_GROUP, not saved —
  host re-pushes on connect). `AUTOCAL_REF_SHACKLE 2` constant. Class members
  `autocal_ref_accum/cnt[MAXSCALES]`.
- **Part 1** (`AutoTare`, overhead.cpp ~6677): when `AutoCalEnable` and the weigh shackle is the
  reference (shackleno 2), skip its tare (=0) and average its `(raw-AutoBias)` reading; at the
  averaging-complete step set `SpanBias[s] = 1000*(known/avg_ref - 1)` and capture
  `AutoCalSpanBaseline[s]` (clamp anchor). Persists SCL group + shmID 32 update.
- **Part 2** (`AutoCalMonitor`, new; called from `ProcessWeight` ModeRun right after span is
  applied): `final_ref = (raw-AutoBias)*(1+SpanBias/1000)` for the reference shackle; gates (zero
  active `WeighZero[s]`; weight present > `MISSING_PCT`); outlier reject (>`OUTLIER_PCT`); deadband
  (`DEADBAND_PPT`); slow integral toward `target = 1000*(known/pre - 1)` at `1/INTEGRAL_DIV` per
  accepted sample; hard clamp to `baseline ± AutoCalClampPpt` with **alarm-and-HOLD** beyond; writes
  + persists SpanBias; sets `AutoCalAlarm[s]` (0 ok / 1 held / 2 zero-off / 3 weight-missing). Trace
  under `_AUTOZ_`. Tunables: DEADBAND_PPT=5, OUTLIER_PCT=10, MISSING_PCT=50, INTEGRAL_DIV=16.

**UNITS CONTRACT (critical for Phase 2 host push):** `AutoCalKnownWeight` (shmID 101) is in the
controller's INTERNAL weight units = raw ADC counts after AutoBias (same units as
`ShackleStatus.weight`), NOT lbs. The host MUST convert the operator's lbs entry:
`known_counts = lbs * counts_per_pound` (from `loadcell_config`, per line/scale) before pushing.
Same scaling the rest of the weight pipeline uses (host divides raw by counts-per-pound for lbs).

**Reference-shackle reads 0 in simulation:** `ProcessWeight`/`AutoTare` zero the weight when
`weight_simulation_mode` is set (the empty-deck sim) — so on the siggen the reference shackle will
read 0 and Part 1/2 can't see the known weight. Phase 3 (siggen) MUST inject a steady known weight
on shackleno 2 every rev and the sim path must not zero it. On a real rig with a welded weight this
is moot.

---

(Original spec below.)

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
