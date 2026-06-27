# Sensor Scope (oscilloscope / logic-analyzer for sensor pulses)

Status: **MVP done + rig-verified (2026-06-27).** Demo-simulator synthesis deferred.
Scope: **SandCat / Linux only.** EPM-19 keeps working with the interface but does NOT get
this feature (it is host arch-gated and the EPM-19 `rtx_source` is never modified).

## Why
A logic-analyzer view of the controller's raw input pulses so operators can SEE the zero flag —
especially the single-sensor double-pulse (see `SINGLE_SENSOR_ZERO_FLAG.md`) — and tune the tab
window visually. The single-sensor bring-up was blind to pulse timing (we had to bolt temporary
`SSDBG` logging into the controller); this makes that timing a glance instead of a log-grep. It is
also useful in standard mode (count + zero channels, debounce/flaky-sensor diagnosis, training).

## EPM-19 safety (the design driver)
1. **No `SHARE_MEMORY` struct change** — the capture ring buffer lives in the `overhead` process
   memory, NOT in `pShm`. The interface marshals the same struct, so the wire layout is unchanged.
2. **New IPC ids appended at the END of `common/ipc.h`** (`SET_SYNC_CAPTURE=332`,
   `SYNC_CAPTURE_INFO=333`, before `LAST_APP_MSG`) so existing ids 300–331 do not shift. EPM-19's
   own `rtx_source/ipc.h` is never edited and never sees 332/333.
3. **Host arch gate** — the API only sends `SET_SYNC_CAPTURE` to lines where
   `GetControllerArch(lineID)=="sandcat"`. An EPM-19 never receives it and never emits 333. The web
   hides the screen for non-SandCat lines.

## IPC (mirrors the LC-capture pattern 326/327)
- **`SET_SYNC_CAPTURE` (332, host→controller)** — `{mode, triggerSync, pre, post}` as four int32s.
  `mode`: 0 off, 1 live (continuous), 2 trigger-on-zero. Handled in `ProcessMbxMsg` like
  `SET_LC_CAPTURE`.
- **`SYNC_CAPTURE_INFO` (333, controller→host)** — 16-byte header `{numSamples, samplePeriodMs=5,
  triggerIndex, numChannels=4}` + `numSamples*4` bytes. Sent via `SendHostMsg`.

**Sample = 4 bytes per 5 ms scan:** `{sync_in[0], sync_zero[0], switch_in[0], eventFlags}`.
- `sync_in[0]` = count sensors (bit per sync 0–7), `sync_zero[0]` = zero sensors, `switch_in[0]` =
  grade/missed-bird/switch bits.
- `eventFlags` = what the detector decided this scan: `0` none; else `0x40`=a zero fired,
  `|0x80` if it was a single-sensor TAB (else a standard zero bit), `|0x20` if a grade sync; low 3
  bits = sync/grade index. This is what the web overlays as ZERO/TAB markers.

~4 bytes × 200 samples/s = ~800 B/s — trivial next to drop-record traffic.

## Controller (`linux_port/overhead` only)
- `overhead.h`: capture state + ring buffer (`syncCapBuf[SYNC_CAP_MAXSAMPLES=1500][4]`, mode,
  trigger, pre/post, head/count, `syncCapEventAccum`). Methods `SyncCaptureScan/Send/SetSyncCapture`.
- `App_Timer_Main`: clear `syncCapEventAccum` before the sync processing, append the sample after
  `ProcessSyncs()` (`SyncCaptureScan()`).
- The detector zero paths in `ProcessSyncs`/`GradeSyncs` set `syncCapEventAccum`.
- Live mode flushes ~1 s windows; trigger-on-zero frames pre+post samples around the selected sync's
  zero. Capture-N (manual freeze) is web-side over live mode.

## Host (`apps/api`) and web (`apps/web`)
- `client.go`: `CmdSetSyncCapture/CmdSyncCaptureInfo=332/333`; `buildSetSyncCaptureMsg`;
  `handleSyncCaptureInfo` decodes 333 → WS `SYNC_CAPTURE` (count/zero/switch/event arrays).
- `hub.go`: `SET_SYNC_CAPTURE` added to the web→controller command whitelist.
- `main.go OnCommand`: arch-gate (drop `SET_SYNC_CAPTURE` for non-sandcat lines).
- web `SensorScope.tsx` (route `/calibration/sensor-scope`, System menu below Capture Load Cell,
  `calibrate` permission, hidden for non-sandcat lines): canvas timing diagram; Off / Live /
  Trigger-on-Zero / Capture-5s; per-sync or all-syncs; single-sensor tab-window shading + ZERO/TAB
  markers + measured-gap readout. Store: `syncCapture` keyed by lineId.

## Adding a new IPC message — the FOUR places that each silently drop it (learned 2026-06-27)
1. `common/ipc.h` enum (append at end).
2. **Rebuild + redeploy the linux `interface` binary** — `interface/tcpserver.cpp` relays only
   `cmd < LAST_APP_MSG` (inbound); a stale interface drops new ids. (EPM-19's Windows Interface.exe
   is separate and untouched — host gating means it never sees these ids.)
3. **`SendHostMsg` switch case (`overhead.cpp`)** — the `default:` REJECTS unknown cmds. Add a case
   (copy to `gen.data`, set `gen.hdr.len`). The controller's `SendHostMsg` also bounds
   `cmd < LAST_APP_MSG`, so the controller must be rebuilt against the new `ipc.h`.
4. **API hub command whitelist (`realtime/hub.go`)** — web→controller command switch.

## Deploy note
The **linux `interface` binary must be redeployed** to any SandCat site that gets this feature
(source unchanged, but it must be rebuilt against the new `ipc.h` so `LAST_APP_MSG` covers 332/333).

## Test status — PASSED 2026-06-27
Drove live + trigger captures end-to-end on the 2-SandCat rig: real count+zero pulse data on the
active syncs; detector event bytes decode correctly (ZERO on scale/drop syncs, grade ZERO; the TAB
bit sets only in single-sensor mode). Remaining: demo-simulator `SYNC_CAPTURE_INFO` synthesis for
the off-rig DO demo.
