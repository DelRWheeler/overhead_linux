# Overhead Controller Linux Port - Implementation Plan

## Context

The GS-1000 RTX controller application currently runs on Windows XP Embedded with IntervalZero RTX on VersaLogic EPM-19 boards (Vortex86DX2 CPU). The Vortex86DX2 is too old for any modern Linux distro. Two VersaLogic SandCat EPM-39 boards (Intel Atom E3825 Bay Trail) have been ordered as replacements. This plan covers porting the controller from Windows/RTX to Linux, running on the SandCat hardware. 8 field controllers will eventually be upgraded.

The project lives in `/home/del/Documents/projects/overhead_controller/` - separate from the web interface project.

## Approach: Direct C++ Port

Port the existing C++ code to Linux C++ (g++), reusing all algorithms and data structures verbatim.

**Why not rewrite in Go/Rust:**
- The weighing, grading, drop, and batch algorithms (~11,000 lines in overhead.cpp) have been refined over 20+ years in production. Rewriting risks introducing bugs in critical logic.
- The data structures (`SHARE_MEMORY`, `mhdr`, `THostDropRec`, etc.) are used in binary TCP communication with the API server. C++ keeps struct layouts byte-identical.
- Go's garbage collector causes unpredictable 1-10ms latency spikes - incompatible with the 5ms cycle.

## Architecture: Merge Backend + Interface into Single Process

Currently two separate processes (Overhead.rtss + Interface.exe) communicate via shared memory and mailboxes. On Linux, merge into one process:

- Eliminates all mailbox IPC (6 sets of semaphore/mutex/shared-memory channels)
- Shared memory struct becomes a regular heap struct protected by pthread_mutex
- One binary, one systemd service, simpler deployment

```
[overhead_controller]  (single process)
  Thread: main_loop    (SCHED_FIFO pri 80, 5ms cycle - I/O, weighing, drops)
  Thread: fast_update  (SCHED_FIFO pri 78, 200ms - display data sends)
  Thread: gp_timer     (SCHED_FIFO pri 75, 500ms - housekeeping, stats)
  Thread: tcp_server   (accepts host connections, routes messages)
  Thread: tcp_send     (sends status updates to host)
  Thread: hbm_serial   (HBM load cell serial I/O)
  Thread: isys_comms   (InterSystems TCP to other controllers)
  Thread: drop_manager (drop maintenance)
```

## OS & Real-Time

- **Ubuntu Server 24.04 LTS** with PREEMPT_RT kernel (mainlined in kernel 6.12+)
- `SCHED_FIFO` threads with `mlockall(MCL_CURRENT | MCL_FUTURE)`
- 5ms timer via `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)` - avoids drift
- No desktop environment - minimal X11 only for status display window
- Plymouth for boot logo

## Hardware Access

| Hardware | RTX Method | Linux Method |
|----------|-----------|-------------|
| PCM-3724 DIO (0x300) | `RtReadPortUchar/RtWritePortUchar` | `ioperm()` + `inb()`/`outb()` from `<sys/io.h>` |
| HBM Load Cell (RS-485) | Direct UART register access (COM3/COM4) | `/dev/ttySx` with termios API (fallback: direct UART) |
| VSBC6 GPIO | Not needed on SandCat | Removed |
| EPM-19 FPGA | Not needed on SandCat | Removed |

## Project Structure

```
overhead_controller/
├── CMakeLists.txt
├── README.md
├── config/
│   ├── controller.yaml              # Runtime config
│   └── overhead-controller.service   # systemd unit
├── src/
│   ├── main.cpp                      # Entry, signals, thread creation
│   ├── platform/                     # Linux HAL
│   │   ├── rtcompat.h                # RTX→Linux API macros
│   │   ├── timer.h/cpp               # RT timer thread class
│   │   ├── io_port.h/cpp             # ioperm/inb/outb wrappers
│   │   ├── serial.h/cpp              # termios serial port
│   │   └── event.h/cpp               # Windows Event emulation
│   ├── core/                         # Business logic (from rtx_source)
│   │   ├── overhead.h/cpp            # Main controller (11,341 lines)
│   │   ├── overheadconst.h           # Constants
│   │   ├── overheadtypes.h           # Data structures
│   │   └── utils.h/cpp               # Utilities
│   ├── io/                           # I/O board drivers
│   │   └── io_3724.h/cpp             # PCM-3724 (1-4 cards)
│   ├── loadcell/                     # Load cell drivers
│   │   ├── loadcell.h/cpp            # Base class
│   │   ├── hbm_loadcell.h/cpp        # HBM digital (RS-485)
│   │   └── lc1510.h/cpp              # 1510 analog (future)
│   ├── drops/                        # Drop management
│   │   └── drop_manager.h/cpp        # Drop logic (2,357 lines)
│   ├── intersystems/                 # Multi-line coordination
│   │   └── intersystems.h/cpp        # Coordination (2,201 lines)
│   ├── comm/                         # TCP communication
│   │   ├── ipc.h                     # Message types
│   │   ├── tcp_server.h/cpp          # Host TCP server
│   │   ├── tcp_client.h/cpp          # Inter-controller TCP
│   │   └── msg_queue.h/cpp           # Thread-safe queue
│   └── sim/                          # Built-in simulator
│       └── simulator.h/cpp
├── display/                          # Status display
│   └── display.h/cpp                 # Minimal X11 or ncurses
├── tests/                            # Test suite
│   ├── test_timer_jitter.cpp
│   ├── test_io_port.cpp
│   ├── test_weighing.cpp
│   └── test_protocol.cpp
└── tools/
    ├── install.sh                    # Installation script
    └── setup_rt.sh                   # RT kernel setup
```

## Key API Mappings

| RTX / Win32 | Linux |
|-------------|-------|
| `RtCreateThread()` | `pthread_create()` with `SCHED_FIFO` |
| `RtSetTimerRelative(5ms)` | `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)` |
| `RtReadPortUchar(addr)` | `inb((uint16_t)addr)` |
| `RtWritePortUchar(addr, val)` | `outb(val, (uint16_t)addr)` |
| `RtEnablePortIo(addr, count)` | `ioperm(addr, count, 1)` |
| `CreateMutex()` | `pthread_mutex_init()` with `PTHREAD_PRIO_INHERIT` |
| `CreateSemaphore()` | `sem_init()` |
| `WaitForSingleObject()` | `pthread_mutex_lock()` / `sem_wait()` |
| `Sleep(ms)` | `usleep(ms * 1000)` |
| `RtAttachShutdownHandler()` | `sigaction(SIGTERM, ...)` |
| WinSock `WSAEventSelect` | `epoll_create()` / `epoll_wait()` |
| `__int64` | `int64_t` |
| `d:\gainco\*` paths | `/var/lib/overhead/*` |

## Implementation Phases

### Phase 0: Environment Setup
- Install Ubuntu Server 24.04 LTS on SandCat #1
- Install PREEMPT_RT kernel (`apt install linux-image-*-rt`)
- Install build tools: g++, cmake, libgpiod-dev
- Init git repo in overhead_controller/
- Write and run timer jitter test (target: <100us at 5ms period)
- Verify PCM-3724 I/O access with simple inb/outb test

### Phase 1: Platform Abstraction Layer
- `rtcompat.h` - Type definitions and RTX API macros → Linux equivalents
- `timer.cpp` - RTTimer class (SCHED_FIFO + clock_nanosleep)
- `io_port.cpp` - IOPort class (ioperm + inb/outb)
- `serial.cpp` - SerialPort class (termios for RS-485)
- `event.cpp` - WinEvent class (pthread_cond emulating Windows events)

### Phase 2: Core Logic Port
- Copy and adapt overheadconst.h, overheadtypes.h, ipc.h
- Port io_3724.h/cpp (mechanical: RtReadPortUchar → inb)
- Port LoadCell base class
- Port HBMLoadCell (serial access changes, protocol stays identical)
- Port overhead.h/cpp (the big one - 11K lines, mostly algorithm preservation)
- Port DropManager
- Port InterSystems

### Phase 3: Communication Layer
- Port TCP server (WinSock → BSD sockets + epoll)
- Port TCP client
- Create message router (replaces mailbox IPC with direct calls + queue)
- Verify protocol compatibility with existing Go API server

### Phase 4: Configuration & Service
- YAML config file (line_id, serial port, I/O base address, TCP ports)
- systemd service with RT limits (LimitRTPRIO=99, LimitMEMLOCK=infinity)
- Install script

### Phase 5: Display
- Plymouth boot logo (company branding)
- Minimal status window (ncurses or X11) showing SPM, BPM, mode, errors
- Auto-start via systemd

### Phase 6: Testing & Validation
- Timer jitter verification with oscilloscope
- PCM-3724 I/O verification with OPTO22 racks
- HBM load cell communication test
- TCP protocol compatibility with Go API server
- Full simulation run comparing output with RTX version
- Hardware validation with real load cells and test weights

## Key Source Files (RTX originals to port)

| File | Lines | Porting Complexity |
|------|-------|-------------------|
| `rtx_source/Overhead/overhead.cpp` | 11,341 | High - core algorithms, many RTX calls |
| `rtx_source/Overhead/HBMLoadCell.cpp` | 2,498 | Medium - serial access changes |
| `rtx_source/Overhead/DropManager.cpp` | 2,357 | Medium - timer/thread changes |
| `rtx_source/Overhead/InterSystems.cpp` | 2,201 | Medium - named pipes → TCP |
| `rtx_source/Interface/tcpserver.cpp` | ~500 | Low - WinSock → BSD sockets |
| `rtx_source/Interface/tcpclient.cpp` | ~500 | Low - WinSock → BSD sockets |
| `rtx_source/Common/overheadtypes.h` | 538 | Low - type substitutions only |

## Risks

| Risk | Severity | Mitigation |
|------|----------|-----------|
| Struct alignment GCC vs MSVC | High | `static_assert(sizeof(...))` checks; `__attribute__((packed))` if needed |
| PCM-3724 I/O timing on Linux | Medium | 5ms loop granularity is same as RTX; test with oscilloscope |
| HBM serial latency via termios | Medium | Start with termios; fall back to direct UART if needed |
| RT kernel unavailable for Bay Trail | Low | Ubuntu 24.04 mainline 6.12+ supports RT; Bay Trail is well-tested |

## Verification

1. **Timer accuracy**: Run test_timer_jitter for 10K iterations, verify max jitter < 100us
2. **I/O correctness**: Toggle each OPTO22 output, read each input, verify with multimeter
3. **Protocol compatibility**: Connect Linux controller to existing Go API, verify all message types parse correctly
4. **Numerical accuracy**: Run identical weight sequences through RTX and Linux versions, compare outputs bit-for-bit
5. **Production simulation**: Run built-in simulator at full speed (720 birds/min), verify no missed drops or weight errors
