# Overhead Controller Linux Port - Changelog

## HARDWARE PORTING CHECKLIST — Read Before Building for Real SandCat/VersaLogic Board

The VM build has several stubs and workarounds that **MUST be changed** for real hardware. Deploying the VM build to a real board without these changes will cause subtle, hard-to-diagnose failures.

### 1. I/O Port Stub: Return Value (CRITICAL — Ghost Bird Records)

**File:** `linux_port/common/platform.h` ~line 1198, `RtReadPortUchar()`

**VM behavior:** Returns `0xFF` (stub, no real I/O board)
**Real hardware:** Must use actual `inb()` calls to read the PCM-3724 I/O board

**Why this matters:** The PCM-3724 uses **active-low logic**. The controller inverts every read with `~`:
- `~0xFF = 0x00` → nothing active (correct for VM with no hardware)
- `~0x00 = 0xFF` → ALL inputs active → phantom syncs, pushbuttons, missed bird triggers → **ghost bird records**

If you forget this and deploy the `0xFF` stub to real hardware, the `inb()` reads from the real board will be overridden and the board won't function. Make sure the stub is replaced with actual port I/O.

### 2. I/O Port Stub: Write Function

**File:** `linux_port/common/platform.h` ~line 1190, `RtWritePortUchar()`

**VM behavior:** No-op stub
**Real hardware:** Must use actual `outb()` calls to write to the PCM-3724

### 3. Digital Load Cell Serial Timeout (TODO)

**File:** `linux_port/overhead/HBMLoadCell.cpp`, `HBMCheckSettings()`

**VM behavior:** HBM serial commands timeout forever because no load cell is attached. Currently the controller retries indefinitely (every 2500 loop iterations).
**Real hardware:** Will work normally with a connected HBM load cell. However, we still want to add graceful error handling — report the problem to the operator and stop retrying rather than restart the controller in a loop. This is a TODO for when hardware is available to test with.

### 4. setcap After Every Rebuild

**Command:** `sudo setcap cap_sys_nice+ep ~/dchservices/bin/overhead`

Applies to both VM and real hardware. The capability is stored on the file and gets wiped by every rebuild/copy.

## Oct 6, 2026 - 15.7.16 (hotfix line `hotfix/sszero-crosscheck`): single-sensor cross-sync count check

Owner: "once the initial zero happens we should never allow the line to run a full revolution with the
wrong shackle count." Until 15.7.15 a sync's count could only be corrected at its own next zero flag. At
Pitman (single-sensor zero, SkipTrollies 1, 303 shackles = 606 trolleys, count syncs Scale 1 / Drop Sync 1
/ Drop Sync 2) the chain ROLLS BACK at line stops: a sensor that sat on a body rolls off it and counts it
again on restart, so drops fired one trolley off for up to a revolution (field 2026-10-05: DS2 "305
expected 303" 16:54:22 / 17:03:12, Scale 1 "trolley 1" 16:58:43, ...). Single-sensor mode
(`ZeroFlagMode == 1`) only; standard two-sensor mode is unchanged. Wire protocol and shm layout unchanged.
This is 15.7.15 (`b269cf8`, the Pitman build) + this change; `SS_BOOT_CONFIRM` stays 0.

### The check (`SingleSensorXC*` in `overhead.cpp`, constants + full rules in `overheadconst.h`, `SS_CROSSCHECK 1`)
- All count syncs sit on one chain: once zeroed, each sync's phase-accurate position since its zero
  (P = trolleys counted + time since the last edge / measured gap G) differs from another's by the sensor
  spacing D. A miscount moves that sync a whole trolley against the others; it is corrected
  (shackleno / trolly_counters / true_shackle_count / tab run moved back one trolley) a few trolleys after
  the restart, with "Count corrected: <sync> -1 trolley (cross-check with <others>)" (warning, not
  rate-limited), and the next zero finds the count exact.
- D is learned only from clean zeros (exact-count check passed on both syncs), confirmed by a second
  clean sample, then TRACKED: the Pitman captures show D varies with chain position by up to 0.42 trolley
  around the revolution (uneven pitch) but repeats to sd 0.02, so a constant D would leave almost no
  margin. Only the fractional residual is tracked; the integer part can never absorb a miscount.
- Only a sync one trolley AHEAD is corrected (a rollback can only add counts), never one that is behind,
  never one anchored by its own zero since the last disturbance, only with >= 3 syncs in the vote, only for
  a disagreement that arose in one step from all pairs at 0, after 2 consecutive edges, in steady running
  (gaps within 87..115% of the previous one - tighter than the 70..143% first proposed, see
  `overheadconst.h`). (P1) lone sync ahead of the others; (P2) after a real line stop, the lower of two
  levels is right - this covers two sensors re-counting at one stop (two of the six real events).
- No per-shackle pass runs twice or is skipped: the correction lands on a count whose drop / missed-bird
  pass already ran one trolley early, so that edge's pass is skipped; the scale re-weighs the shackle on
  it (SkipTrollies >= 1, the early weighment is still averaging) or skips its pass (SkipTrollies 0).
- "Count disagreement: <a> vs <b> by <k> trolley" (warning, once per revolution per pair) when a
  disagreement cannot be corrected: 2-sync lines, a sync behind, two syncs off at once in steady running.
- Not corrected (zeros realign them as before): a lost count; the overrun reset's -1/-2 after a tab
  rejected at a restart; every sync re-counting at the same stop (invisible to any cross-check); large
  offsets (false zeros). Arming from boot takes ~2.4 clean revolutions after the first zero.

### Tests (`linux_port/tools/`)
- `sszero_crosscheck_test.sh` (new): the real extracted ProcessSyncs / helpers replay the three Pitman
  Sensor Scope captures (`/home/del/data/pitman-scope-2026-10-05`) against hand-verified ground truth
  (flag-to-flag 606 trolleys everywhere). 15.7.15 reproduces every field zero mark scan for scan and the
  field alarm texts. 15.7.16 (offsets learned before the capture): 7 corrections, 7 right, 0 false; each
  real re-count fixed 6-8 counted edges after the restart (15.7.15: 192-583 edges, until the next zero);
  the literal 2-of-3 majority rule makes 10 false corrections on the same data. Cold / from boot: counter
  state identical to 15.7.15 on every scan. Synthetic: Pitman + chicken, 7-200 SPM, steps, ramps, stops
  with and without rollback, extra / missing edges, 2-sync, two syncs at once, near-coincident phases,
  4 syncs, a 9 h soak: 0 false corrections; clean lines identical to 15.7.15 scan for scan.
- `sszero_tab_dup_test.sh`, `sszero_window_test.sh`: extract the new functions; output byte-identical
  to 15.7.15 (353/353, 5775/0, `SS_BOOT_CONFIRM` 0 and 1); standard mode identical to 15.7.13.

`APP_VER3` 15 -> 16.

## Oct 5, 2026 - 15.7.15 (hotfix line `hotfix/sszero-measured-window`): single-sensor zero at any line speed

Pitman Farms (skip 1, 304 shackles, flag tab at 0.42 of the trolley pitch) lost its single-sensor zero
whenever the line ran slow: the host's `ZeroTabWindowMaxMs` = 375 ms rejected the genuine tab below
~33.6 SPM (tab = 0.42 x 30000/SPM ms), so the count drifted, drops fired on the wrong trolleys and
operators hand-unloaded birds. Single-sensor mode (`ZeroFlagMode == 1`) only; standard two-sensor mode
is unchanged (proven event-for-event against 15.7.13). Wire protocol and shared-memory layout unchanged.

**What this binary is:** `f9f2968` (15.7.13 + trickle-suspend guard = the `e1bf768c` build at Pitman,
Holmes, Claxton L1) + `02bf814` (15.7.14 single-sensor duplicate-pass guard, cherry-picked clean) + the
changes below. It does **NOT** contain `9da7827` (15.7.14 batch-label slots) - mainline 15.7.14 does.

### Tab detector (`SingleSensorIsZeroTab` -> `SingleSensorTabRule`, constants in `overheadconst.h`)
- Measured, not guessed: G = the last trolley-to-trolley gap, G2 = the one before (tabs never update
  them). TAB only if (a) `0.20*G <= delta <= 0.65*G`, (b) G within 70..143% of G2 (not accelerating),
  (c) `tabRun >= R - 8` trolleys since the last accepted tab, R = Shackles*(SkipTrollies+1).
- `ZeroTabWindowMinMs/MaxMs` and the 0.18..0.50 x EMA test are no longer consulted (fields + host push
  kept). EMA `interval`, stall / shrink re-seeds and timebase semantics unchanged; `tabRun` now counts
  every trolley-classified edge.
- Boot confirmation (`SS_BOOT_CONFIRM 1`): the first zero after a controller start needs a second
  tab-shaped edge R+1 +/- 8 trolleys after an earlier one (per-sync bit ring of 8192 positions).
  Costs up to one extra revolution before the first zero (~14 min at turkey speed, ~5 at chicken);
  set `SS_BOOT_CONFIRM 0` to take the first tab-shaped edge instead.

### Alarms (per sync, single-sensor only)
- Owner's rule replaces `SingleSensorWarnOk` (<= 1 per 30 s GLOBAL, which swallowed other syncs'
  alarms): no zero-flag alarm until the sync has counted Shackles+1 since boot, then at most one per
  Shackles+1 counts - for "NOT Detected", "Early", "Late" and the new "position mismatch". Counting
  on a missed zero is unchanged. "Initial" is not gated.
- New "Zero Flag position mismatch. <sync> count n trolley t expected Shackles+1/0": an accepted tab
  must find the flag body's count (Shackles+1, skip parity 0); catches the misplaced zeros Late/Early
  cannot see (e.g. skip 1: missed tab then a false zero one trolley later).
- New informational "Zero Flag re-acquired. <sync>" on the first good zero after a raised
  "NOT Detected" / "position mismatch" (the rule usually swallows the "Late" that used to say it).
- While a boot candidate awaits confirmation the threshold is two revolutions, so a normal start-up
  raises no alarm.

### Tests (`linux_port/tools/`)
- `sszero_tab_dup_test.sh` (adapted): standard mode vs real 15.7.13 identical; single-sensor
  duplicate-pass guard vs the same tree without it. 353/353 with `SS_BOOT_CONFIRM` 1 and 0.
- `sszero_window_test.sh` (new): real ProcessSyncs/GradeSyncs over 5 ms scan streams of a moving
  chain - Pitman/chicken steady, steps, ramps, stops, jitter, noise, missing flags, any-plant grid.
  Steady and 5-15 s ramps: 0 missed / 0 false. Residuals (all alarmed): instant or 2 s speed-up 1-8
  trolleys before the flag; a stop with the flag body in front of the sensor.

`APP_VER3` 13 -> 15.

## Sep 26, 2026 - 15.7.14: batch label slots, batch-number reuse, single-sensor zero-tab double pass

> **Hotfix line note:** the 15.7.15 hotfix branch carries only the single-sensor part of this entry
> (`02bf814`); the batch-label slot fix (`9da7827`) below is mainline 15.7.14 only.

Found by the dual-scale distribution soak on the office rig (host write-up:
`overhead/docs/DUALSCALE_SOAK_AND_BATCH_FIXES_2026-09-26.md`). Wire protocol and host behaviour
(Delphi and the Go API) unchanged. Deployed to the rig controller `.11` only (sha256 `844daeb3...`).

### Batch label slots (`overhead/BatchLabelSlots.h`, `overheadmacros.h`, `overhead.cpp`, `InterSystems.cpp`)
- **Leak.** A label slot (MAXBCHLABELS = 64) was freed only after the host's 311 AND the 320/321
  pre-label exchange, and 320 is sent only when the batch's last bird drops. A batch cleared before
  completion (RESET_BATCH / "Start New", mode change, CLEAR_TOTALS) held its slot until a 319 or a
  restart. At 64 `LABEL_INFO` silently found no slot: batching continued but no 310 reached the host
  (no batch record, no label, no alarm).
- **Fix (a):** `ReleaseCutShortLabel(drp)` at the head of `CLEAR_DROP_BATCH` and before the CLEAR_TOTALS
  wipe frees the slot of a batch that never completed (`Batched==0`, this line's number): at once if the
  host already answered the 310, otherwise on its 311. Completed batches still send their 320.
- **Fix (b):** `GetLabelSlot()` - when full, reuse the oldest slot the host has acked (311) whose batch
  is no drop's current batch, and raise ERROR_MSG (<=1/min) "Batch label table full..." /
  "Batch N (drop D) NOT sent to host...". Never silent again.
- **Fix (c):** `NextBatchNumber()` follows the old sequence but skips numbers that are still a drop's
  current batch or held in a slot - no reuse after CLEAR_BATCH_RECS (319) or the 999->1 wrap.
- `LABEL_INFO` now also resets `pre_label_step` when it takes a slot. Traces under `_LABELS_`
  (`Lbl cutshort|reclaim|noslot|skip`).
- Test: `tools/batchlabel_slots_test.cpp` (62/62; 200 mid-batch resets: 15.7.13 holds all 64 slots and
  136 batches never reach the host, 15.7.14 holds 0). Rig: 100 mid-batch resets -> 100 batch opens.
- **RTX:** `rtx_source/Overhead` has byte-identical macros - needs the same change (VC6) for EPM-19 sites.

### Single-sensor zero flag: shackle processed twice per revolution (`overhead.cpp`, `overhead.h`)
- In `zero_flag_mode=1` the flag trolley's body pulse counts 1189 -> 1190, then the tab (~140 ms later)
  resets to 1; `RingSub` maps both to the same shackle (1172 on the rig), so the whole per-edge block ran
  twice: every drop on a tab-detecting sync **fired its kicker a second time**, the missed-bird check and
  `AddDropRecord` ran twice (phantom row graded area 0's letter), `GradeProcess` re-read the eye, the
  InterSystems shackles/sec counted +1.
- **Fix:** per-sync flags `ss_ovr_pass` / `ss_grade_ovr_pass`; on an accepted single-sensor tab after a
  pass at Shackles+1, still reset the counter and re-label the weighment, but skip the duplicate
  drop/missed-bird pass and GradeProcess and don't count the tab for the IS rate. Standard (two-sensor)
  mode, missed/late/early tabs and the first zero after restart behave exactly as before.
- Test: `tools/sszero_tab_dup_test.sh` runs the real 15.7.13 and 15.7.14 `ProcessSyncs`/`GradeSyncs`
  side by side (15 scenarios, 290/290; counters/zeroing identical on every scan; 5 mutants caught).
  Rig: 21-min capture - 0 repeated shackles, 1172 once per revolution.

`APP_VER3` 13 -> 14.

## Aug 15, 2026 - 10" Kiosk Window Position (`DCH_GUI_X` / `DCH_GUI_Y`)

Added `DCH_GUI_X` / `DCH_GUI_Y` origin-offset env vars to `linux_port/interface/dch-server-gui.py`
so the kiosk window can be nudged right/down off the top-left, compensating for 10" panels whose
EDID over-reports the usable area (a full-frame window at `0,0` clipped off the left/top). Default
`0,0` = unchanged. Approved standard values for the 10" panel, tuned live on both Holmes Foods
controllers (`.11`, `.12`): `X=300 Y=50 WIDTH=1600 HEIGHT=1100`.

These values (and the pre-existing `DCH_GUI_WIDTH`/`HEIGHT`) now ship in a **version-controlled**
launcher at `linux_port/interface/start-kiosk.sh` (0755) — previously hand-edited on each box only,
and fixed by hand three times. See `docs/SANDCAT_ZEROBIAS_ALARM_AND_KIOSK_DISPLAY.md`.

**Open item:** new SandCats are produced by **cloning** an existing image, not deployed from this
repo — so the clone-master image must be updated with the new `start-kiosk.sh` and `dch-server-gui.py`
for future controllers to inherit this automatically.

## Feb 23, 2026 - CPU Starvation Crash Fix & Diagnostics

### Critical Bug: Controller crashed every 3-6 minutes

**Symptom:** `CheckHeartbeats: GP_TIMER not responding` — GP_TIMER thread starved on Intel Atom E3825 (2 cores, 1.33GHz). Load average: 17.05, overhead CPU: 75.4%.

**Root causes identified and fixed:**

#### 1. WaitForMultipleObjects: busy-polling replaced with eventfd + poll() (CRITICAL)

The Linux port's `WaitForMultipleObjects()` was a `while(1)` loop with `usleep(10000)` — 10ms polling that never returned `WAIT_TIMEOUT`. On XP/RTX this was a proper kernel wait (zero CPU).

**Fix:** Added `notify_fd` (eventfd) to event struct. `RtSetEvent()` writes to eventfd, `WaitForMultipleObjects()` uses `poll()` on eventfds with proper timeout. Zero CPU when idle.

#### 2. Auto-reset events not consumed by WaitForMultipleObjects (CRITICAL)

The `process_event[]` events (EVT_SHM_READ, etc.) are auto-reset (`manualReset=false`). On Windows, `WaitForMultipleObjects` automatically clears auto-reset events when consumed. The initial poll()-based fix didn't do this — events stayed signaled forever, causing `MbxEventHandler` to spin at ~3500 iterations/sec.

**Fix:** `WaitForMultipleObjects` now clears `signaled=0` and drains eventfd for auto-reset events in both fast-path and poll-path. Also fixed `RtWaitForSingleObject` and `RtResetEvent` to drain eventfd.

#### 3. GpSendThread: 5ms polling loop

`GpSendThread` had `Sleep(5)` — 200 wakeups/sec polling flags set every 500ms.

**Fix:** Changed to `Sleep(50)` — still catches flags within 50ms.

#### 4. Timer threads: no real-time priority

GP_TIMER ran at normal priority, easily starved by other threads.

**Fix:** `_timer_thread_func()` sets `SCHED_RR` priority. Requires `CAP_SYS_NICE` (granted via systemd `AmbientCapabilities`).

#### 5. ExitWindowsEx: called `system("sudo reboot")`

On XP this rebooted the PC. On Linux, rebooting the whole system is wrong — just restart the service.

**Fix:** Changed to `_exit(1)` and let systemd `Restart=on-failure` handle restart.

### Results

| Metric | Before | After |
|--------|--------|-------|
| Load average | 17.05 | 0.62 |
| Overhead CPU | 75.4% | 7.6% |
| MbxEventHandler | Busy-spin (3500/sec) | 50ms poll blocking (20/sec) |
| All thread states | R (running) | S (sleeping) |
| Crash frequency | Every 3-6 min | None |

### Diagnostic infrastructure added

- `logAppFlagsChange()` — logs every `AppFlags=0` with timestamp, PID, TID, backtrace
- `logInterfaceAppFlags()` — logs Interface shutdown decisions
- GP_TIMER checkpoint tracking (`g_gptimer_checkpoint` 1-99) for hang diagnosis
- Heartbeat health log (`/tmp/overhead_heartbeat.log`) every ~5 seconds
- Timer thread exit logging

### PCM-3724 Simulator removed

Deleted `pcm3724_sim.cpp` and `pcm3724_sim.h`. The signal generator on SandCat A provides real PCM-3724 digital I/O signals — the built-in simulator is not needed and must stay disabled.

### Systemd service (`dch-controller.service`)

- Added `Nice=-5` and `AmbientCapabilities=CAP_SYS_NICE` for RT scheduling
- `ExecStopPost` kills both `overhead` and `interface` (was only killing overhead)
- `ExecStartPre` cleans orphan overhead processes
- Disabled conflicting `dch-server-gui.service` (was grabbing port 5000)

### Modified files

- `linux_port/common/platform.h` — eventfd + poll() WaitForMultipleObjects with auto-reset, SCHED_RR timer threads, eventfd drain in RtResetEvent/RtWaitForSingleObject, ExitWindowsEx fix
- `linux_port/overhead/overhead.cpp` — GP_TIMER checkpoints, heartbeat log, GpSendThread Sleep(50), GenError logging
- `linux_port/overhead/Main.cpp` — GP_TIMER checkpoint globals, exit logging, SIGABRT/SIGTERM handlers
- `linux_port/overhead/overheadext.h` — logAppFlagsChange() with backtrace, checkpoint externs
- `linux_port/overhead/overheadmacros.h` — logAppFlagsChange in EXCEPTION_SHUTDOWN macro
- `linux_port/interface/main.cpp` — logInterfaceAppFlags() diagnostic logging
- `linux_port/overhead/pcm3724_sim.cpp` — DELETED
- `linux_port/overhead/pcm3724_sim.h` — DELETED

---

## Feb 11, 2026 - Session 3: HBM Load Cell Simulator & EPIPE Fix

### HBM FIT7 Digital Load Cell Simulator (NEW)

Created a virtual HBM FIT7 digital load cell simulator that exercises the full HBM serial code path (init_adc, HBMCheckSettings, continuous measurement stream). This replaces `_WEIGHT_SIMULATION_MODE_` which bypassed the entire serial path.

**New files:**
- `linux_port/overhead/hbm_sim.h` — Simulator state struct, FIFO ring buffer, API declarations
- `linux_port/overhead/hbm_sim.cpp` — Full HBM FIT7 protocol implementation

**Features:**
- 4096-byte FIFO ring buffer (power of 2 for fast masking)
- Complete command parser for all 21 settings (ASF, FMD, ICR, CWT, LDW, LWT, NOV, RSN, MTD, LIC0-3, ZTR, ZSE, TRC1-5) plus protocol commands (IDN, BDR, COF, CSM, STR, MSV, STP, SPW, TDD1, MAV, RES)
- Background measurement thread generating 4-byte binary packets at ~600 Hz
- Weight values cycle through sim_weight table (2-4 lb birds), converted to ADC counts using CNTS (116500)
- FIFO flush on each new command to prevent cascading misalignment from unconsumed responses

**Enabled via:** `#define _HBM_SIM_` in overheadconst.h

**Modified files:**
- `linux_port/overhead/Serial.cpp` — Routes RtReadComPort/RtWriteComPort/RtGetComBufferCount through HBM simulator when `_HBM_SIM_` defined
- `linux_port/overhead/CMakeLists.txt` — Added hbm_sim.cpp to build
- `linux_port/common/overheadconst.h` — Added `_HBM_SIM_` define, disabled `_WEIGHT_SIMULATION_MODE_`

### Fixed: init_adc "Error in HBMLoadCell Configuration"

**Root cause:** `SendCommand()` initialized `ResponseCnt = 1000` (changed by DRW from original 10). The BDR check in `init_adc()` uses `response_time=500`. The wait loop condition `while (1000 < 500)` was always false, so the BDR response was **never read**. This caused:
1. BDR check failed (response not consumed)
2. Stale BDR response sat in the FIFO
3. All subsequent commands (COF, CSM) read the wrong (stale) response
4. COF and CSM checks also failed → `Config=false` → init_adc error

**Fix (two parts):**
- `HBMLoadCell.cpp` line 1355: Changed `ResponseCnt = 0` (was 1000). Now the wait loop executes for all commands.
- `hbm_sim.cpp` process_command(): Added FIFO flush at start to clear any stale unconsumed responses.

### Fixed: "Attempted to change ASF from 5 to 0"

**Root cause:** Same cascading FIFO misalignment from the init_adc bug. With the init_adc fix applied, ASF reads correctly and the error no longer occurs.

### Fixed: DefaultDigLCSet.ICR (overhead.cpp)

Changed `DefaultDigLCSet[i].ICR` from 0 to 4. The real HBM FIT7 defaults to ICR=4. With ICR=0, the controller logged "LC 1 ICR changed from 4 to 0" on every startup.

### Fixed: Send Failed 32 (EPIPE) — tcpclient.cpp

**Root cause:** Writing to a TCP socket after the remote end (API) closes the connection sends SIGPIPE, which by default terminates the process. The error "send failed 32, file tcpclient.cpp line 226" was EPIPE (broken pipe).

**Fix:**
- `tcpclient.cpp` SendTcpMsg(): Changed `send()` flag from `0` to `MSG_NOSIGNAL`. Suppresses SIGPIPE for that specific send, returning -1/EPIPE instead.
- `main.cpp` main(): Added `signal(SIGPIPE, SIG_IGN)` at startup as defense-in-depth.
- Improved error logging to include `strerror(errno)` and connection index.

### Database Changes (overhead project, not controller)

Updated `dig_lc_settings` table with correct HBM FIT7 defaults:
- ASF=5, FMD=1, ICR=4, CWT=500000, LWT=500000, RSN=1, LIC1=500000
- Updated `loadcell_config.load_cell_type = 1` (Digital/HBM) for all lines

### Simulation Defines Summary

```cpp
//#define _SIMULATION_MODE_           // DISABLED - bypasses I/O entirely
//#define _WEIGHT_SIMULATION_MODE_    // DISABLED - bypasses HBM serial path
#define _PCM3724_SIM_                 // Virtual PCM-3724 I/O board (from Session 2.5)
#define _HBM_SIM_                     // Virtual HBM FIT7 load cell (NEW)
```

---

## Feb 10, 2026 - Session 2: Bug Fixes & Stabilization

### Changes Made

#### 1. Fixed MAXSYNCS Issue (overhead.cpp, line 9827)
**Problem:** `ProcessSyncs()` iterated over all 16 syncs (`MAXSYNCS = 16`) regardless of configuration. The real system only has max 2 scale syncs + 6 drop syncs = 8. This caused false "Initial Zero Flag Detected" messages on unconfigured syncs (up to sync 14).

**Fix:** Changed `NumSyncs` from `MAXSYNCS` to `pShm->scl_set.NumScales + pShm->sys_set.NumDrops`, with a safety cap at `MAXSYNCS`. Boaz mode override still applies.

```cpp
// BEFORE:
int NumSyncs = MAXSYNCS;

// AFTER:
int NumSyncs = pShm->scl_set.NumScales + pShm->sys_set.NumDrops;
if (NumSyncs > MAXSYNCS) NumSyncs = MAXSYNCS;
```

**Sync layout (sync_desc array):**
- Index 0: Scale 1
- Index 1: Scale 2
- Index 2-7: Drop Sync 1-6 (max 6 drop syncs, NEVER 14)

#### 2. Fixed Uninitialized w_avg Array (overhead.cpp, ~line 1739)
**Problem:** `w_avg[]` array was uninitialized, causing garbage weight values (19 billion lbs) to be read from shared memory on startup.

**Fix:** Added `memset(&w_avg, 0, sizeof(w_avg))` in `InitLocals()`.

#### 3. Removed Debug Print Spam (overhead.cpp)
**Problem:** Debug prints added during development caused excessive console output.

**Removed:**
- File write to `/tmp/gp_timer_debug.log` (was in `Gp_Timer_Main`)
- `gp_timer_count` variable and "GP Timer fired" print
- "OVH Heartbeat: AppHB=%d pShm=%p" print
- "GP_Timer complete tick=%d hb=%d" print

#### 4. Created totals.dat (~/dchservices/data/totals.dat)
**Problem:** Controller could not read totals file on startup: "Error reading totals"

**Fix:** Created zero-filled file at 6260 bytes (matching `sizeof(ftot_info)` with alignment padding). Initial attempt at 6256 bytes was wrong - the struct has 4 bytes of alignment padding.

#### 5. Set RT Capabilities on Binaries
**Problem:** "RT scheduling not available for thread (need root or RT privileges)" warnings.

**Fix:**
- Created `/etc/security/limits.d/rt-scheduling.conf` (needs new login session)
- Set capabilities: `sudo setcap cap_sys_nice+ep` on both interface and overhead binaries
- NOTE: Must re-apply setcap after every rebuild (capability is stored on the file)

#### 6. Created droprecs Directory
**Path:** `~/dchservices/data/droprecs/`
Required for drop record file storage.

### API Changes (ALL REVERTED)

The following API changes were made but **caused the controller to crash at ~30 seconds** and were fully reverted:
- Proactive settings push (sending all 4 setting groups on connect) - **MAIN CULPRIT**
- Fixed struct sizes for shmIDs 17, 18, 19 in shm_read_handler.go
- Tightened weight validation (200 lbs cap)
- Added CmdBlkDrecReq (cmd 304) handler

**Revert command used:**
```bash
git checkout HEAD -- apps/api/cmd/server/main.go apps/api/cmd/server/shm_read_handler.go apps/api/internal/controller/client.go
```

These changes need to be re-applied carefully, one at a time, with testing between each.

### Known Issues (Unresolved)

1. **No shmID 52/88 data flowing to API**: Controller reaches ModeRun (OpMode=6) but no DispShackle (shmID 52) or SystemOutRecords (shmID 88) data appears at the API. Web UI status bar shows no shackle counts or weights. Need to investigate why `GpSendThread` / `SendHostMbxServer` isn't forwarding SHM_WRITE messages.

2. **Sync count may still need tuning**: User reported "I think we still have issues with the number of syncs." The NumScales + NumDrops calculation may not exactly match what the Delphi app expected. Need to verify with user what the correct active sync count should be for the current configuration.

3. **RT scheduling warnings**: `setcap` must be re-applied after every rebuild. The limits.d config requires a fresh login session to take effect.

4. **Stale POSIX IPC**: Shared memory (`/dev/shm/SharedMemory`) and semaphores persist across process restarts. Must clean `/dev/shm/` between restarts or stale data causes garbage weights and semaphore failures. Consider adding cleanup to a startup script.

5. **drprec1.dat write error**: Occurred once during session. May be transient. The droprecs directory exists with proper permissions.

6. **RtReleaseSemaphore failed**: Occurred once, likely due to stale POSIX semaphores from previous crashed run. Cleaning /dev/shm/ before restart resolved it.

### Operational Notes

**Startup procedure:**
```bash
# 1. Clean stale IPC (REQUIRED between restarts)
rm -f /dev/shm/App_Msg_Shm /dev/shm/DM_Msg_Shm /dev/shm/HstTx_Shm \
  /dev/shm/Isys_Msg_Shm /dev/shm/Rx_Shm /dev/shm/Tx_Shm \
  /dev/shm/SharedMemory /dev/shm/TelnetTermSMem /dev/shm/TraceMemory
rm -f /dev/shm/sem.*

# 2. Set RT capabilities (after rebuild)
sudo setcap cap_sys_nice+ep ~/dchservices/bin/overhead
sudo setcap cap_sys_nice+ep ~/dchservices/bin/interface

# 3. Start via GUI
python3 /home/del/Documents/projects/overhead_controller/linux_port/interface/dch-server-gui.py
```

**Build procedure:**
```bash
cd /home/del/Documents/projects/overhead_controller/linux_port/overhead/build
make -j$(nproc)
cp overhead ~/dchservices/bin/overhead
sudo setcap cap_sys_nice+ep ~/dchservices/bin/overhead
```

**Key paths:**
- Controller binary: `~/dchservices/bin/overhead`
- Interface binary: `~/dchservices/bin/interface`
- Settings files: `~/dchservices/data/settings/*.bin`
- Totals file: `~/dchservices/data/totals.dat` (6260 bytes)
- Drop records: `~/dchservices/data/droprecs/`
- Source code: `/home/del/Documents/projects/overhead_controller/linux_port/overhead/overhead.cpp`
- Build output: `/home/del/Documents/projects/overhead_controller/linux_port/overhead/build/overhead`

**Timing:**
- ModeStart → ModeRun transition takes ~50 seconds (comm_state machine: states 0→1→2→3→0)
- "System started, configuration OK" appears ~5 seconds after startup
- config_Ok set when all 4 settings groups loaded from files

---

## Feb 9-10, 2026 - Session 1: Initial Linux Port

### Platform Abstraction (platform.h)
- POSIX shared memory via `shm_open()` / `ftruncate()` / `mmap()`
- Named semaphores via `sem_open()` / `sem_post()` / `sem_wait()`
- Mutexes via named semaphores (binary semaphore pattern)
- Windows Events via `pthread_cond_t`
- Thread creation via `pthread_create()` with SCHED_FIFO
- Timer via `timer_create()` with `SIGRTMIN`
- I/O port stubs (no real hardware on VM)
- Sleep/timing via `clock_nanosleep()`

### TCP Communication (interface)
- TCP server on port 5000 (inbound from API)
- TCP client connecting to port 5001 (outbound to API)
- Replaces Windows named pipes
- Binary protocol preserved byte-identical

### Build System
- CMake-based build in `linux_port/overhead/build/`
- Compiles with g++ and `-fpermissive` (lots of legacy C++ issues)
- Single binary output

### GUI Launcher (dch-server-gui.py)
- Python GTK3 + VTE terminal
- Spawns interface process in embedded terminal
- File → Save Log, Edit → Clear Screen
