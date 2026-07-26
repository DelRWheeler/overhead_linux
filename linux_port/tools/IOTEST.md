# iotest — standalone PCM-3724 output test utility

A single-file, no-dependency, statically-linked command-line tool that drives the
PCM-3724 digital outputs **with zero controller software in the loop**. Its whole
purpose is to answer one question conclusively:

> Is a dead/erratic drop output a **card / driver / wiring** fault, or an
> **application** fault?

If `iotest` fires an output and the solenoid clicks, the card and the wiring are
good and the problem is upstream in the controller (schedule, priority,
distribution, kick width, `Active` gating…). If `iotest` fires it and nothing
happens, the fault is in the card, the opto rack, the ribbon, or the field wiring.

`iotest` shares no state with the controller. It initializes the second 8255
group itself, so it works on a cold-booted box where the controller has never
been started.

---

## Build

Native, on an x86_64 Linux box:

```bash
cd linux_port
g++ -O2 -static -o iotest tools/iotest.cpp
```

Cross-compiled from the aarch64 dev VM (this is how the shipped binary was
built — the dev VM is ARM, the SandCat is x86_64):

```bash
cd linux_port
x86_64-linux-gnu-g++ -O2 -Wall -Wextra -static -o tools/iotest tools/iotest.cpp
```

No libraries, no headers beyond libc. The binary is fully static so it runs on
any x86_64 SandCat regardless of what is (or isn't) installed there.

---

## Prerequisites — read before running

1. **Run as root.** Port access uses `iopl(3)`. Always invoke with `sudo`.
2. **Stop the controller first.** `iotest` refuses to drive outputs while a
   process named `overhead` is alive, because two writers on one PCM-3724 fight
   over the same output latches — the controller's output scan would overwrite
   `iotest`'s writes, and `iotest` would clobber live drop firing.

   ```bash
   sudo systemctl stop dchserver      # or however the controller is stopped on this box
   pgrep -x overhead                  # must print nothing
   ```

   `--status` is read-only and is allowed to run with the controller live.
   `--force` overrides the guard — only with a reason, and never with birds on
   the line.
3. **Outputs are ACTIVE LOW.** Latch `0xFF` = all 8 channels off; clearing a bit
   energizes that channel. This matches the controller, which always writes
   `~output_byte[n]`.

---

## Usage

```
sudo ./iotest --all [options]
sudo ./iotest --drop N [options]
sudo ./iotest --status
```

### Modes

| Flag | Meaning |
|------|---------|
| `-a`, `--all` | Sweep outputs 1..24 in order, dwelling on each one, announcing port and bit. |
| `-d N`, `--drop N` | Fire a single output N (1..24). |
| `--status` | Read and print the three output latches (and decode which outputs are currently energized), then exit. **Writes nothing** — safe with the controller running. |

### Options

| Flag | Default | Meaning |
|------|---------|---------|
| `-t SECS`, `--time SECS` | `2.0` | Hold/dwell per output. Floats OK (`-t 0.25`). |
| `-r K`, `--repeat K` | `1` | Repeat the fire / the whole sweep K times. |
| `--readback` | off | After **every** write, read the port back and verify the latch. Mismatches are flagged loudly and set exit code 2. |
| `--gap SECS` | `0.3` | Off-time between outputs and between repeats. |
| `--force` | off | Run even though the `overhead` controller is alive. Dangerous. |
| `--init-inputs` | off | Also write `CFG_REG_0` (0x303) `<= 0x9B`, forcing the first 8255 group to all-inputs exactly as the controller does. Off by default so the input group is never touched. |
| `-h`, `--help` | | Full help. |

### Examples

```bash
sudo ./iotest -a                      # sweep all 24 outputs, 2 s each
sudo ./iotest -d 11 -t 5 --readback   # hold output 11 (0x305 bit 2) for 5 s, verify the latch
sudo ./iotest -a -t 0.5 -r 3          # three fast sweeps
sudo ./iotest -d 6 -t 0.2 -r 20       # chatter output 6 twenty times (relay / opto check)
sudo ./iotest --status                # what is energized right now (read-only)
```

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | OK, no verify errors |
| 1 | Usage error, not root, or refused because the controller is running |
| 2 | Readback/verify mismatch — the write did not stick |

---

## Output-to-port map

24 outputs across the second 8255 group. Identical arithmetic to
`overhead.cpp SetOutput()`: `byte = (N-1)/8`, `bit = (N-1)&7`, `mask = 1<<bit`.

| Outputs | Port | Register |
|---------|------|----------|
| 1..8 | `0x304` | PORT_A1 |
| 9..16 | `0x305` | PORT_B1 |
| 17..24 | `0x306` | PORT_C1 |

Example: output 11 → byte 1 (`0x305`), bit 2, mask `0x04`, latch written `0xFB`.

---

## Safety design

* **Refuses to run with the controller alive** (except `--status`). Native
  `/proc` scan equivalent to `pgrep -x overhead`, so it works even on a box with
  no `procps` installed. The refusal happens *before* `iopl()` — nothing on the
  card is touched at all.
* **Every exit path releases every output.** Normal completion, error paths,
  `atexit`, and handlers for SIGINT / SIGTERM / SIGHUP / SIGQUIT and for fatal
  faults (SIGSEGV / SIGBUS / SIGFPE / SIGILL / SIGABRT) all write `0xFF` to
  `0x304`, `0x305`, `0x306` before exiting. Ctrl-C during a dwell drops the
  output immediately.
* **The first 8255 group (0x300–0x303, the sensor inputs) is never written**
  unless you pass `--init-inputs`, and even then only the all-inputs control
  word `0x9B` — which cannot drive anything.
* **Single threaded.** `iopl()` privilege is per-thread on Linux; a background
  thread's `outb()` would be silently lost. Everything runs on the main thread.
* **Final verification.** The run ends by reading all three latches back and
  reporting `latches idle FF/FF/FF`; anything else is flagged.

---

## Init sequence and the one deliberate deviation

Production (`linux_port/overhead/3724_io.cpp`, `io_3724::initialize()`) writes,
in this order:

```
GATE_CNTRL  (0x309) <= 0x00   disable all buffer gates
CFG_REG_0   (0x303) <= 0x9B   group 0 all inputs
CFG_REG_1   (0x307) <= 0x80   group 1 all outputs
BUFFER_DIR  (0x308) <= 0x38   A0/B0/C0 in, A1/B1/C1 out
GATE_CNTRL  (0x309) <= 0xFF   re-enable all gates
PORT_A1/B1/C1       <= 0xFF   all outputs off   <-- AFTER the gates open
```

`iotest` uses the **same registers and the same values**, with two differences:

1. **Data before gates (safety).** The 8255 mode-set write to `CFG_REG_1` clears
   the group's output latches to `0x00`, which on this active-low wiring means
   *all 24 channels energized*. Production then opens the gates **before**
   writing `0xFF`, so the field sees an all-on glitch lasting the duration of
   those two I/O writes. `iotest` writes the all-off pattern to the data latches
   **first** and enables the gates **last**, so nothing is ever driven except the
   channel you asked for. It then re-asserts `0xFF` once the buffers are live.

2. **`CFG_REG_0` (0x303) is not written by default.** That register belongs to
   the input group, which the tool has no business touching. Skipping it is safe:
   the 8255 powers up / resets with all ports as inputs, and `BUFFER_DIR = 0x38`
   independently holds the A0/B0/C0 buffers in the input direction. Pass
   `--init-inputs` to restore the exact production write if you want byte-for-byte
   parity.

Resulting order in the tool:

```
GATE_CNTRL  (0x309) <= 0x00
[CFG_REG_0  (0x303) <= 0x9B  only with --init-inputs]
CFG_REG_1   (0x307) <= 0x80
BUFFER_DIR  (0x308) <= 0x38
PORT_A1/B1/C1       <= 0xFF   <-- moved up, gates still closed
GATE_CNTRL  (0x309) <= 0xFF
PORT_A1/B1/C1       <= 0xFF   (re-assert, belt and braces)
```

---

## Reading the results

* **Output clicks / solenoid fires** → card, opto rack, ribbon and field wiring
  are good for that channel. Look upstream in the controller.
* **Nothing happens, but `--readback` says "verify OK"** → the 8255 latch took
  the value, so the fault is downstream of the chip: buffer/gate hardware, opto
  module, ribbon, field wiring, or the solenoid itself.
* **`--readback` reports a MISMATCH** → the write never reached a latch. Card
  absent or failed, wrong base address, or a second writer on the bus. An absent
  card typically reads back `0xFF` for everything, so the mismatch shows up on
  the ON write (`wrote FB, read FF`).
* **Some outputs work and others don't, consistently by channel** → opto module
  or field wiring on the dead channels.
* **`--status` shows channels energized while the controller is stopped** → the
  card was left in a driven state; run any `iotest` mode to reset it to all-off,
  or check whether something else opened the gates.
