# Single-Sensor Zero Flag (competitor-replacement feature)

Status: **DONE + rig-verified (2026-06-27).** Detector, delivery, ms tab window, simulator,
host DB/API/UI all implemented and verified end-to-end on a 2-SandCat rig (10.5h soak clean,
both lines; standard-mode regression clean). Demo-simulator synthesis is the only deferred
piece. See `[[single_sensor_zero_flag_feature]]` memory for the full session record.
Scope: **SandCat / Linux (`linux_port`) only** — the RTSS / `rtx_source` is obsolete and is
NEVER modified. The Interface and the data structures sent to EPM-19 machines must stay
byte-for-byte unchanged (we still support EPM-19 plants).

## Why
Replacing a (now-unsupported) competitor's controllers in CA and Trinidad. Mostly plug-and-play;
two differences:
1. Analog load cell instead of HBM — handled by a hardware load-cell card that talks the HBM
   protocol, so **no software change**.
2. **Single sensor per sync** instead of our two (count + separate zero). The zero marker is a
   sheet-metal piece on one trolley that makes the count sensor read a **double pulse**:
   trolley block → ~1-trolley-wide gap → tab block. Seeing that double block = shackle zero.

## How our system differs today
Inputs are interleaved on the opto board: **even bits = count (sync) sensors**, **odd bits =
zero sensors**. `ProcessSyncs()` (drop syncs) and `GradeSyncs()` check, at each count edge,
whether the matching **zero bit** is set → if so, reset the shackle count.

## The feature
A **system-wide** setting on the Features screen: **Zero Flag Type = Standard | Single-Sensor**.
- Standard (default): today's behavior, byte-for-byte unchanged.
- Single-Sensor: **ignore the odd zero bits entirely**; derive zero from the double-pulse on the
  even count bit. No I/O remap — even bits (0,2,4,6,…) stay as the count sensors.

## Delivery — over shmID 96 (NOT 81), SandCat-gated
**The original "reuse spare_int shmID 81" plan DOES NOT WORK** and was abandoned. shmID **81 sits
inside the controller's read-only `ZERO_CTR(71)..GRD_SHKL(83)` status range**, so a host
`SHM_WRITE` to it is rejected by `ProcessMbxMsg`'s VALID_IDS gate (`SHM_WRITE Error id 81`).
Confirmed on the rig — every push to 81 bounced and `ZeroFlagMode` stayed 0.

What we actually do:
- `linux_port` only: `spare_int` is renamed `ZeroFlagMode` (the field is fine where it is; its own
  shmID-81 slot just isn't host-writable). RTSS keeps `spare_int`.
- Deliver over a **new shmID 96** — a spare `ALL_SHM_IDS` slot (`MAXIDS=93`, `MAX_GROUPS=5` →
  94–98 valid, like auto-shutdown's 94/95). An added `shm_tbl` entry maps 96 → `&ZeroFlagMode`
  (an alias). 96 passes VALID_IDS and is host-writable.
- The tab window (below) ships as **shmIDs 97/98** the same way.
- The Go API pushes 96/97/98 **gated to `arch=="sandcat"`** (96–98 don't exist on EPM-19, so an
  EPM-19 would reject them). This loses nothing — single-sensor is a SandCat-only feature. The
  earlier "ungated / EPM-19-safe" claim is void.
- **No `SHARE_MEMORY` struct change** for the mode (reuses spare_int's slot); the window adds two
  ints appended at the **end** of the struct (after the auto-shutdown fields) so no existing
  offset shifts → interface wire format unchanged → EPM-19 unaffected.

## Detector algorithm (controller) — configurable ms window + learned `T` as a sanity bound

> ⚠️ **SUPERSEDED by 15.7.15 (2026-10-05).** The fixed ms window below failed in the field: at Pitman it rejected
> every zero below ~34 SPM. See **15.7.15 / 15.7.16** at the end of this document. Kept for history.
Geometry: trolleys on **6" centers**, block ~1.1–1.25" wide. Normal = one count edge per trolley
interval `T`. Zero flag tab's rising edge lands at ~2.5" after the trolley = **~40% of `T`**.
Tick unit = `App_Timer_Main` scans (**5 ms**); `ss_scan_tick` is a monotonic scan counter.

`SingleSensorIsZeroTab()` runs per confirmed count edge when `ZeroFlagMode==1`, per sync (and per
grade sync). It keeps a per-sync `last_trolley_tick` + EMA `interval` (`T`):
1. `Δ = ss_scan_tick - last_trolley_tick`.
2. Seed timebase on the 1st edge; seed `interval` on the 2nd.
3. `Δ > 3·T` → line was idle/stopped: resync timebase, keep `T`, treat as trolley.
4. **TAB** if `Δ` is inside the **host-configured ms window** `[ZeroTabWindowMinMs, ZeroTabWindowMaxMs]`
   (converted to ticks via /5) **AND** `2·Δ < T` (the learned `T` is the *backup sanity bound* so the
   window can never misfire on a real trolley). On a tab: ZERO that sync, do **not** advance the
   trolley timebase (so the trolley after the tab still measures a full `T`).
5. Else trolley: count it, and fold `Δ` into the EMA **only if `Δ ≥ 0.7·T`** (a plausible full
   trolley) — this guard stops a misclassified tab from poisoning `T` and death-spiralling.
6. The standard-mode `BITSET(…ZeroBit)` path is taken byte-for-byte when `ZeroFlagMode==0`.

**Why a ms window (not pure self-calibration):** the original `Δ < 0.6·T` alone was fragile at the
edges (a single misclassification poisoned `T`). Operator-set min/max ms (default 30–250) is
deterministic and tunable per line; defaults apply until the host pushes 97/98.

### Single-sensor off-by-one (expected, handled)
The marked trolley's *body* pulse counts (`shackleno → Shackles+1`) **before** its tab pulse resets
it. So the "too many shackles" guard tolerates `Shackles+1` in single-sensor mode, and the
"Zero Flag NOT Detected" warning is **throttled to ≤1/30 s** (`SingleSensorWarnOk()`) so imperfect
tab timing during tuning can't flood the host (an unthrottled flood crashed the operator browser
during bring-up). There is always an empty zero region (no bird on trolley 0 or the 2 ahead of it),
so tares/auto-zero are **unchanged** from standard.

### `SkipTrollies` (turkey lines)
The count sensor sees **every** trolley regardless of skip; `SkipTrollies` indexes shackles on top
(via `trolly_counters[]`). So the double-pulse detection runs on the **raw count edges**, upstream
of the skip logic, so the tab is never lost on a skipped trolley.

## Simulator (`apps/signal-generator`)
Matching `zeroFlagMode`: in single-sensor mode generate count pulses on the **even** outputs only,
inject the **tab** (second block) at the zero position with the right bandwidth/timing, and
**suppress the odd zero outputs**. Config field + web-UI toggle.

## Integration points (as built)
- `linux_port/common/overheadtypes.h`: `spare_int` → `ZeroFlagMode`; `ZeroTabWindowMinMs` +
  `ZeroTabWindowMaxMs` appended at struct end.
- `linux_port/overhead/overhead.cpp`: `shm_tbl` aliases **96**→`ZeroFlagMode`, **97/98**→window;
  `ss_scan_tick++` in `App_Timer_Main`; `SingleSensorIsZeroTab()` + `SingleSensorWarnOk()`;
  detector branch substituted in `ProcessSyncs()` + `GradeSyncs()` (gated on `ZeroFlagMode==1`).
- `apps/api`: `SystemFeatures.{ZeroFlagMode,ZeroTabWindowMinMs,ZeroTabWindowMaxMs}`, migrations
  029/030, `CZeroFlagMode=96` / `CZeroTabWindowMin=97` / `CZeroTabWindowMax=98`, `sendZeroFlagMode()`
  pushed on bootstrap + Features save, **gated to `arch=="sandcat"`**.
- `apps/web/src/pages/Features.tsx`: Standard/Single-Sensor selector + min/max ms inputs.
- `apps/signal-generator`: `single_sensor` config; suppress odd zero outputs, inject the tab as a
  2nd count pulse at `ZeroTabOffset=2.5"` (~42% of cycle); web-UI toggle.

## Test status (rig) — PASSED 2026-06-27
2-SandCat rig (L1 .11, L2 .12, siggen .21, Pi .103): siggen drove the double-pulse, controller
zeroed on the tab (`Δ≈28` ticks in the 30–250 ms window), shackle count tracked + wrapped, weights
per bird, production records grew, **10.5h soak clean** (0 disconnects, 0 floods, steady
production), and **standard-mode regression clean** (two-sensor zeroing unchanged).
Remaining: turkey (`SkipTrollies=1`) spot-check; demo-simulator synthesis for off-rig demo.


## 15.7.15 / 15.7.16 (2026-10) — measured-gap tab rule, alarm rule, cross-sync check, message queue

**Field failure that forced this (Pitman Farms, 2026-10-05).** Single-sensor, `SkipTrollies` 1, 303 shackles.
- **What failed:** the measured tab sits at **0.41·T** (Sensor Scope: 0.403/0.411/0.418 on three heads). With the
  `ZeroTabWindowMaxMs` 375 rail, every zero was rejected below 12600/375 = 33.6 SPM, and the plant runs turkeys at
  21–33 SPM.
- **Why no fixed ms window can work:** the 21 SPM tab (~600 ms) is longer than a 72 SPM trolley gap (417 ms).
- **Owner requirement:** single-sensor zero must work at ANY plant (any skip, length, speed range, flag geometry)
  with no per-plant tuning.

**15.7.15** (`hotfix/sszero-measured-window`, b269cf8; base f9f2968 + 02bf814; constants in overheadconst.h):
- **`SingleSensorTabRule`:** a candidate edge is the tab only if all three hold:
  - **band:** `200·G <= 1000·delta <= 650·G`, where G = the last trolley-classified gap (accepted tabs never
    update G or G2);
  - **steadiness:** `70·G2 <= 100·G <= 143·G2`;
  - **revolution gate:** `tabRun >= Shackles·(SkipTrollies+1) - 8`.

  `ZeroTabWindowMin/MaxMs` and the `0.18–0.50·interval` test are no longer consulted (the shm fields and host
  push are unchanged). Stall/shrink/EMA are unchanged. Clean in simulation for every skip/geometry combination
  (tab 0.25–0.50) at 0–5 % jitter, 10–220 SPM.
- **Boot confirmation** (two candidates R±8 apart before the first zero): built, `SS_BOOT_CONFIRM 0`. The owner
  requires a restart to zero on the first flag pass, as before.
- **Exact-count check:** an accepted tab must find `shackleno == Shackles+1 && trolly_counters == 0`, else
  "Zero Flag position mismatch". It catches the skip-1 half-shackle slip that Early/Late cannot see. At Pitman it
  exposed a 303-shackle chain configured as 304.
- **Alarm rule (owner):** per sync, no zero-flag alarm before the count first reaches the preset; then at most
  one per revolution (NOT Detected / Early / Late / mismatch). It replaces `SingleSensorWarnOk` (one per 30 s
  across ALL syncs, which swallowed alarms). An informational "Zero Flag re-acquired" marks the first good zero
  after an alarm.

**15.7.16** (`hotfix/sszero-crosscheck`, 0750225; `SS_CROSSCHECK 1`):
- **Cross-sync count check.** All count syncs share one chain, so their phase-accurate trolley offset is a
  constant once zeroed. It varies ~0.4 trolley around the chain but repeats within sd 0.02, so it is tracked.
  - **Learning:** from clean zeros, confirmed twice; afterwards only the fraction is tracked.
  - **Correction:** only a sync **one trolley ahead**, by −1. A stop only adds counts (chain rollback re-counts);
    a lost count is alarm-only.
    - **P1:** one sync ahead while the others agree.
    - **P2:** after a real stop, the lower of two levels wins. Two sensors re-counting at one stop is common; a
      symmetric majority made false corrections on real data.
  - **Guards:**
    - ≥ 3 syncs;
    - steady gate 87–115 %;
    - 2 consecutive agreeing edges;
    - a disagreement that appears in one step;
    - never a sync anchored by its own zero.
  - **Arming:** ~2.4 clean revolutions after the first zero; until then, behaviour is exactly 15.7.15.
  - **Messages:** "Count corrected: <sync> -1 trolley (cross-check with …)" (never rate-limited);
    "Count disagreement: <a> vs <b> by <k> trolley" (once per revolution per pair).
  - **Proof:** replay of real Pitman captures 7/7 corrected, 0 false; rig fault injection, all cases.
  - **Field:** first correction 2026-10-07 00:52:48 PDT, verified by the next zero.
- **GenError message queue.** `GenError` used a single `send_error` slot, so simultaneous messages reached the
  host as the last one only. This is inherited and present in every version, standard mode included. It is now a
  64-entry lock-free FIFO of copied text, drained in order by SendError:
  - on overflow the newest overwrites the oldest, followed by an "N controller messages dropped" summary;
  - dchserver.log is unchanged.

  Tests: `tools/errqueue_test.sh` (38/0, ASan/UBSan clean).

Tests for all of the above: `tools/sszero_tab_dup_test.sh`, `tools/sszero_window_test.sh`,
`tools/sszero_crosscheck_test.sh`, `tools/errqueue_test.sh`. Full field write-up: overhead repo
`docs/PITMAN_SINGLE_SENSOR_ZERO_FIX_2026-10-05.md`.
