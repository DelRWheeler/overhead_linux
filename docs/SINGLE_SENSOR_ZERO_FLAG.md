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
