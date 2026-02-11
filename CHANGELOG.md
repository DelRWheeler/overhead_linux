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
