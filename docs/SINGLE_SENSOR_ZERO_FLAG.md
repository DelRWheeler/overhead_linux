# Single-Sensor Zero Flag (competitor-replacement feature)

Status: design + delivery plumbing done; detector + simulator + UI in progress.
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

## Delivery (the EPM-19-safe part)
Reuse the existing **`spare_int` field, shmID 81** (`NO_GROUP`, comment "nothing", verified unused
in both `linux_port` and `rtx_source` — only the declaration + shm_tbl entry reference it).
- In `linux_port` only, rename `spare_int` → `ZeroFlagMode` (same offset, same shmID 81). RTSS
  keeps `spare_int`.
- Go API pushes shmID 81 with the mode int32 on bootstrap + on Features save. Because 81 is an
  **existing** shmID present in the EPM-19 shm_tbl, an EPM-19 simply writes its ignored `spare_int`
  — no struct-size change, no unmapped write, **no gating needed**. (Contrast auto-shutdown's
  94/95, which are linux-only and ARE gated to `arch=="sandcat"`.)

## Detector algorithm (controller) — self-calibrating, no scope trace needed
Geometry: trolleys on **6" centers**, block ~1.1–1.25" wide. Normal = one count edge per trolley
interval `T`. Zero flag tab's rising edge lands at ~2.5" after the trolley = **~40% of `T`**.

In `ProcessSyncs()` / `GradeSyncs()`, when `ZeroFlagMode==1`:
1. Per sync, track `lastEdgeTick`; maintain a running `T` = ticks between consecutive count edges.
2. On a new count edge, `Δ = nowTick - lastEdgeTick`.
3. If `Δ < K·T` (K ≈ 0.6) → it's the **tab** → ZERO for that sync (reset shackle count); do **not**
   count the tab as a trolley. Else → normal trolley → count it; update the running `T` from this
   (non-tab) interval only.
4. The standard-mode `BITSET(switch_in, ZeroBit)` zero path is skipped in this mode.

Self-calibrates to line speed and trolley width. Needs a few normal trolleys at startup to seed
`T` before zero detection is reliable (same as today's startup-zeroing revolution).

### `SkipTrollies` (turkey lines)
The count sensor sees **every** trolley regardless of skip; `SkipTrollies` indexes shackles on top
(via `trolly_counters[]`). So the double-pulse detection runs on the **raw count edges**, upstream
of the skip logic, so the tab is never lost on a skipped trolley.

## Simulator (`apps/signal-generator`)
Matching `zeroFlagMode`: in single-sensor mode generate count pulses on the **even** outputs only,
inject the **tab** (second block) at the zero position with the right bandwidth/timing, and
**suppress the odd zero outputs**. Config field + web-UI toggle.

## Integration points
- `linux_port/common/overheadtypes.h`: `spare_int` → `ZeroFlagMode` (offset/shmID unchanged).
- `linux_port/overhead/overhead.cpp`: `InitShmTbl` entry 81 label; read mode; `ProcessSyncs()`
  (@~10136) + `GradeSyncs()` (@~5611) detector branch.
- `apps/api`: `SystemFeatures.ZeroFlagMode`, migration, `CZeroFlagMode=81`, push on bootstrap +
  Features save (ungated).
- `apps/web/src/pages/Features.tsx`: Standard/Single-Sensor selector.
- `apps/signal-generator`: `zeroFlagMode` generation + UI.

## Test plan (rig)
Single-sensor end-to-end: siggen drives the double-pulse, controller zeros on the tab, shackle
count tracks, production records correctly; verify on a turkey config (`SkipTrollies=1`). Then
regression-test Standard mode is unchanged.
