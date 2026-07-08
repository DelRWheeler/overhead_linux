# Span Bias Authority & Persistence — the LAW

## The LAW (design intent)

There is exactly **one authority** for a line's span bias at any time, selected by the
`AutoSpanEnable` (Auto Calculate Span) flag, and its value **must persist across
controller restarts and power cycles** until that same authority changes it.

1. **Auto Calculate Span ENABLED (`AutoSpanEnable = 1`)** — *controller is authority*
   - Span is set **only** by the welded-reference **tare cal**, on the controller.
   - The per-rev monitor stays **read-only** (records + drift alarm; never trims).
   - The **host never writes span** (manual Auto Span disabled; no bootstrap re-push).
   - Persists until the **next tare cal**.

2. **Auto Calculate Span DISABLED (`AutoSpanEnable = 0`)** — *host is authority*
   - Span is set **only** by **manual Auto Span** (operator hangs a known weight →
     host computes → `SET_SPAN_BIAS` → controller + DB).
   - The tare cal **never** writes span (already gated this way).
   - Persists until the **next manual Auto Span** (host re-pushes the stored value on
     every reconnect, so it survives controller restarts).

## Root cause of the observed failure (CORRECTED)

On the rig, line 1's span reverted from the tare-cal value (−9) to a stale 42 on a
controller restart. **The controller persistence is NOT broken.** A subagent trace of
the full path (controller `fsave` + Go API) found the real cause: **host ↔ DB
divergence**, entirely on the host side.

1. **The controller does persist correctly.** Tare cal writes `scl_set.SpanBias`,
   marks group `SCL_IN_GROUP` (index 2 → `scale.bin`) changed, and `CheckConfig()`
   (runs every ~5 s, not mode-gated) flushes it to disk. The −9 *did* reach
   `scale.bin`.

2. **The API drops the controller's announcement.** After tare cal the controller
   sends its new span to the host as `SHM_WRITE` shmID 32 (`CSpanBias`). But
   `handleShmWrite` (`apps/api/internal/controller/client.go:1517`) has **no
   `case CSpanBias`** → it hits `default:` and is logged "unhandled" and discarded.
   **The DB (`loadcell_config.span_bias`) never learns the −9** and still holds the
   old manual 42.

3. **The bootstrap re-pushes the stale DB value.** On every reconnect the API sends
   the persisted DB span back to the controller
   (`apps/api/cmd/server/shm_read_handler.go:1167`, Delphi-parity re-push). In this
   environment reconnects are frequent (API restart, "Refresh Settings", ASF flips).
   That push overwrites the controller's −9 in RAM (`overhead.cpp:4492`), marks the
   group changed, and `CheckConfig()` writes **42 back into `scale.bin`**. The next
   restart loads 42.

So the manual 42 "persisted" and the tare-cal −9 "didn't" **not because of any
controller flush difference** — both write the same field/group — but because the host
kept re-asserting a stale DB value the controller's own calibration never fed back into.

## Changes to make it LAW

**The core fix is entirely host-side (Go API + web) — no controller firmware change,
so no controller restart risk.** A controller-side defensive gate is an optional
phase 2.

### A. Go API — the core fix

1. **Persist controller-reported span (close the divergence).**
   Add `case CSpanBias:` to `handleShmWrite` (`client.go:1517`): parse the int64[2]
   and write it to `loadcell_config.span_bias` (reuse `UpdateSpanBias`,
   `settings.go:680`). Now the DB always mirrors the controller's authoritative span,
   so a later bootstrap re-push carries the *correct* value, never a stale one.

2. **Gate the bootstrap re-push on authority.**
   At `shm_read_handler.go:1167`, only re-push `CSpanBias` when
   `ls.AutoSpanEnable == false` (host is authority). When `AutoSpanEnable == true`
   the controller owns span on its own disk — the host must **never** push it.
   (`ls.AutoSpanEnable` is already in scope here — it's used at line 258.)

3. **Refuse the manual span push when the controller is authority.**
   In the `SET_SPAN_BIAS` handler path (`buildSetSpanBiasMsg`, `client.go:3698`),
   reject/no-op when `AutoSpanEnable == true` and surface a clear message, so a stray
   manual push can never reach a controller-authority line.

### B. Web

4. **Gate the manual Auto Span screen on `AutoSpanEnable`.**
   When `AutoSpanEnable = 1`, disable **Calibration → Auto Span** with
   "Span is managed by Auto Calculate Span." Enable it only when `AutoSpanEnable = 0`.
   (UI enforcement of A.3.)

### C. Controller — OPTIONAL phase 2 (defensive, needs go-ahead + rig validation)

5. **Reject host span writes when `AutoSpanEnable == 1`.**
   In the host `SHM_WRITE` handler (`overhead.cpp:4492`), if the target is `SpanBias`
   and `AutoSpanEnable == 1`, ignore it. Belt-and-suspenders so *no* source — buggy
   host, old client, manual poke — can corrupt a controller-authority calibration.
   Not required for the LAW (A.2 + B.4 already stop the host from pushing), but makes
   it bulletproof.

## Authority switch (`AutoSpanEnable` toggled)

No special handling needed and no "needs recal" alarm: the span in effect is always a
real, valid calibration; only *who may change it next* changes. 1→0: the controller's
last calibration stays in effect until the operator runs manual Auto Span. 0→1: the
host stops pushing (A.2); the controller keeps its disk value until the next tare cal.

## One-time cleanup

Line 1's DB currently holds the stale 42. After A.1/A.2 deploy it self-heals on the
next tare cal (which now feeds −9 back to the DB). To clear it immediately, run one
tare cal on line 1 after deploy.

## Validation (the LAW is proven, not assumed)

1. `AutoSpanEnable = 1`: tare cal → note span → confirm DB `span_bias` now matches
   (A.1 working) → **restart controller** → span **persists** (bootstrap did NOT push,
   A.2). Attempt manual `SET_SPAN_BIAS` → **rejected** (A.3/B.4).
2. `AutoSpanEnable = 0`: manual Auto Span → span set in DB + controller → **restart** →
   host re-pushes stored value → span **persists**. Confirm tare cal writes no span.
3. Toggle `AutoSpanEnable` both ways → confirm the retired authority's value never gets
   clobbered by the other path, and survives a restart.

## Priority

Correctness — a power cycle reverting span to a stale value is a silent
wrong-weight/mis-distribution risk. Core host-side fix (A+B) is low-risk (no controller
firmware change). Target: fixed and validated **before the Pitman Farms install
(July 25)**.
