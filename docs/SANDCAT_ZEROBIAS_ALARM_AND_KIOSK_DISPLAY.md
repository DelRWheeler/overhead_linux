# SandCat: "Zero Bias Off" Alarm Fix, Kiosk Display Tuning, and the Negative-Weights Incident (2026-07-15)

This documents three related things worked out on the office test rig on 2026-07-15:

1. The negative-weights incident on line 1 and its true root cause.
2. The change that stops the "zero bias off" red banner from nagging before the scale has zeroed.
3. New environment variables that let the DCH Server kiosk GUI be sized/fonted without editing code.

Rig involved: `.103` Pi (API/web, `overhead-pi-test`, `site_type=test_rig`) driving line 1 = SandCat
`.11` (`dchserver1`), whose HBM load-cell signal is produced by the **HBM FIT7 simulator** on
SandCat A `.21` over RS-485 (see `docs`/memory `hbm_signal_gen`). The simulator emits an empty
shackle at **0.58 lb = 67,570 counts** (`counts_per_pound = 116500`), 0 counts at the zero flag,
and bird weights from a table when hanging.

---

## 1. Negative-weights incident — root cause = FOREIGN TARES, not a frozen cell or a bad zero

**Symptom:** line 1 empties read −2.5 to −5.6 lb (varying per shackle) and the display looked like it
was "weighing negative birds"; line 2 sat correctly at 0.00.

**What it was NOT (ruled out with live `/dev/shm/SharedMemory` reads on `.11`):**

- Not the software zero. `AutoBias` = −0.002 lb, stable and correct.
- Not a frozen HBM. The dead-constant `67,567` counts is the simulator's *designed* empty-shackle
  output (0.58 lb), not a stuck cell.
- Not spontaneous tare drift. Tares only change on **Calibrate Shackle Tares** (Del's rule).

**What it WAS:** the controller's shackle tares were **3–5 lb, varied per shackle** — real-plant
hardware values, foreign to a sim rig whose empty signal is a flat 0.58 lb. With `net = raw −
AutoBias − tare`, an empty shackle read `0.58 − 0 − (3…5.76) = −2.5…−5.6 lb`. Line 2 was fine because
its tares matched its 0.58 lb signal.

**How the foreign tares got onto the rig:** the API auto-restores its cached `apps/api/data/
tares_line{N}.bin` to the controller on every reconnect/bootstrap (an RTX-reboot-recovery feature:
`SHM_READ: sent group 3 (Shackle Tares) … restored from stored data`). That cache held stale
Jul-8 real-plant tares (md5 `ec17a155…`, verified identical to what landed in the controller), and
the push **overwrote the rig's correct ~0.58 lb tares with no Calibrate involved** — exactly the
illegitimate tare write Del's rule forbids.

**Immediate fix (applied):** Calibrate Shackle Tares with the deck empty. Watched the tares rewrite
from 3–5 lb → **0.59 lb** across one revolution; new tares also propagated to the host cache
(controller and `.103` cache now share md5 `b5949217…`), so a later reconnect no longer re-pushes the
foreign values. Only shackle 3 stayed 3.03 lb — it sits in the `ScaleSyncOffset = -2` dead-zone
(omitted from drops), so it is harmless.

**Permanent fix (recommended, NOT yet implemented):** the API must **not** auto-restore its cached
tares to a controller that already holds valid tares — cold-recovery only (controller with no tares),
or a freshness handshake. That auto-push is what put real-plant tares onto the sim rig. See memory
`sandcat_line1_stale_tares_and_send_gap`.

---

## 2. "Zero bias off" banner — only alarm AFTER the scale zeros (`overhead.cpp`, `AutoSpanMonitor`)

The red "zero bias off" banner comes from the SandCat-only Auto-Span reference-shackle monitor
(`overhead::AutoSpanMonitor`, added in the 2026-07-02 Auto-Span redesign; not present in the EPM-19
tree). The known-weight reference shackle crosses the scale once per revolution and raises a flag.

**Old behavior:** `if (!WeighZero[s]) flag = 2 ("zero bias off")`. This fired **every revolution
before the line had ever zeroed** — i.e. after every restart, for the whole pre-zero window. Pure
noise, and it conflated "not zeroed yet (normal)" with "zero genuinely went bad."

**New behavior (per Del):** the monitor is meaningless until the scale is zeroed, so when
`!WeighZero[s]` it now clears any prior alarm and returns without raising anything. It only alarms
**after** the scale has zeroed and the known reference weight is then **missing** (flag 3, < 50 % of
known) or **drifted** past the operator threshold (flag 5). Flag 2 is removed.

```c
if (!pShm->WeighZero[s]) {          // not zeroed yet -> reference check is meaningless
    pShm->AutoSpanAlarm[s] = 0;     // clear; do NOT raise "zero bias off"
    return;
}
if (final_ref < (known * AUTOSPAN_MISSING_PCT)/100)  flag = 3;   // zeroed but known weight not seen
else if (abs_err*1000 > known*drift_ppt)             flag = 5;   // zeroed but reference drifted
```

Verified on `.11`: with the line caught un-zeroed (`WeighZero=[0,0]`) after a restart, **no**
"zero bias off" appears in the API log — where the old binary would have repeated it every revolution.

---

## 3. DCH Server kiosk GUI — tunable size + menu font (`dch-server-gui.py`)

The 10" kiosk panel over-reports its EDID usable area, so the full-screen window overflows; the menu
bar also used the tiny GTK theme default font. The GUI now reads three env vars (set them in
`start-kiosk.sh` on the box and restart the service to iterate — no file rebuild):

| Env var | Default | Effect |
|---|---|---|
| `DCH_GUI_WIDTH`        | (screen width)  | explicit window width; unset = full screen |
| `DCH_GUI_HEIGHT`       | (screen height) | explicit window height; unset = full screen |
| `DCH_GUI_MENU_FONT_SIZE` | `20` (pt)     | menu-bar / menu font size, **independent** of the message font |
| `DCH_GUI_FONT_SIZE`    | `24` (pt)       | message / terminal font (pre-existing) |

When `DCH_GUI_WIDTH`/`HEIGHT` are set, the window is sized explicitly and `fullscreen()` is skipped.
Deployed on `.11` at **1600×880 / 20pt menu** (panel reports 1680×945).

---

## Deploy notes for `.11` (SandCat B)

- `dch-server-gui.py` and the `overhead` binary live in `/home/dchservice/dchservices/bin/` and are
  **root-owned**. Deploy the binary via scp to `/tmp` then `sudo install -m 755 -o root -g root`
  (a direct scp-over as `dchservice` is Permission denied).
- `sudo` on `.11` wants the password piped: `echo dchservice | sudo -S …`.
- Restarting `dch-server-gui.service` **restarts the controller too** (same service) — weighing stops
  and the load cell re-inits. Fine on a test line; coordinate on a live plant.
- Backups from this change: `overhead.bak-20260715-171955`,
  `dch-server-gui.py.bak-20260715-171402`, `start-kiosk.sh.bak-20260715-171402`.

---

## 10" kiosk window position — `DCH_GUI_X` / `DCH_GUI_Y` offsets (2026-08-15, Holmes Foods)

**Problem (overscan).** The 10" touch panels over-report their usable area in EDID, so a
full-frame kiosk window placed at origin `0,0` spilled off the **left and top** edges — the
menu bar and part of the left column were pushed past the visible glass. Section 3 above already
added `DCH_GUI_WIDTH` / `DCH_GUI_HEIGHT` to shrink the window, but shrinking alone leaves the
window pinned at the top-left, so the clipped-off area stayed clipped. What was missing was a way
to **move the window's origin** down and to the right into the visible region.

**New offset env vars.** `dch-server-gui.py` now also reads two position variables alongside the
existing size ones:

| Env var | Default | Effect |
|---|---|---|
| `DCH_GUI_X` | `0` | window origin X — pushes the window **right** off the left edge |
| `DCH_GUI_Y` | `0` | window origin Y — pushes the window **down** off the top edge |

Default `0,0` is unchanged (top-left) behavior, so boxes that don't set them are unaffected. On
realize the GUI now does `move(DCH_GUI_X, DCH_GUI_Y)` then `resize(DCH_GUI_WIDTH, DCH_GUI_HEIGHT)`.

**Approved standard values (10" panel).** Tuned live on both Holmes Foods controllers (`.11` and
`.12`) and approved:

```
DCH_GUI_X=300
DCH_GUI_Y=50
DCH_GUI_WIDTH=1600
DCH_GUI_HEIGHT=1100
```

**Now version-controlled.** These four exports previously lived only in the hand-edited
`~/dchservices/bin/start-kiosk.sh` on each box — there was no copy in the repo, and this is the
**third time** we've hand-fixed this same panel geometry. The canonical launcher carrying the
approved values now lives in the repo at **`linux_port/interface/start-kiosk.sh`** (mode 0755), so
it is the source of truth going forward rather than tribal knowledge on the boxes.

**Overriding per-panel.** If a future screen has different overscan, edit the four `export` lines
(`DCH_GUI_X` / `DCH_GUI_Y` / `DCH_GUI_WIDTH` / `DCH_GUI_HEIGHT`) at the top of `start-kiosk.sh` on
that box and restart `dch-server-gui`. Note that restarting the GUI **bounces the controller too**
(same service) — weighing stops and the load cell re-inits — so only do this when the line is
**not weighing**.

### ⚠️ Open action item — update the CLONE-MASTER image

New SandCat controllers are produced by **cloning an existing controller image**, *not* by
deploying from this repo. So committing `start-kiosk.sh` and the `dch-server-gui.py` change here
does **not**, by itself, make future controllers inherit the fix. For that to happen automatically,
the **clone-master image must be updated** with the new `start-kiosk.sh`.

> **Status 2026-09-19:** still open, and it bit again — see the next section, which supersedes the
> geometry values above and adds tooling so a stale clone is caught in one command.

---

## 2026-09-19, Claxton — the fourth hand-fix, and what finally closes it

Claxton lines 2–4 were changed over to SandCat stacks. All three came up on **1920x1200**,
overscanning off the glass — the same hand-fix for the **fourth** time. Root cause confirmed: the
stacks were cloned from a `dchserver2` image that predates the fix (they arrived named
`dchserver2` with `NTP=192.168.100.103`, the office rig's Pi), so the clone master is still stale.

### The geometry that is now standard: force the panel's true mode

Two rival fixes existed for this same panel, and **the repo was carrying the one that cannot work
on any box in the field**:

| | approach | works on fleet boxes? |
|---|---|---|
| Holmes / old repo | keep 1920x1200, move a 1600x1100 window to `+300+50` | ❌ **No** — needs `DCH_GUI_X`/`DCH_GUI_Y`, and every Claxton box runs `dch-server-gui.py` md5 `e0b10f18`, which has **zero** X/Y support. The exports are silently ignored and the window lands at `0,0`. |
| Claxton line 1 | `xrandr` the panel down to its true **1280x800**, window 1280x800 | ✅ **Yes** — needs only `DCH_GUI_WIDTH`/`HEIGHT`, which `e0b10f18` does support. Proven on Claxton line 1 for 9 weeks. |

Forcing the mode is also the better fix on the merits: it makes the *screen* honest instead of
positioning a window inside a frame that lies, so the window is an exact fit with nothing clipped
and no per-panel offsets to re-tune. **`linux_port/interface/start-kiosk.sh` now carries it**, and
the repo copy is byte-identical (md5 `0cb1d96a`) to what runs on Claxton lines 2–4.

```bash
DCH_PANEL_MODE="${DCH_PANEL_MODE-1280x800}"      # "" skips xrandr entirely
for out in $(xrandr | awk '/ connected/ {print $1}'); do
    xrandr --output "$out" --mode "$DCH_PANEL_MODE" 2>/dev/null && break
done
export DCH_GUI_WIDTH="${DCH_PANEL_MODE%x*}"
export DCH_GUI_HEIGHT="${DCH_PANEL_MODE#*x}"
```

It applies to whichever output is **actually connected** rather than a hardcoded `DP-1`, so a panel
moved to `VGA-1` still comes up right. If a panel does not offer the mode, `xrandr` fails quietly
and X keeps what it picked — the window is then merely small, never clipped. Retune by editing the
one `DCH_PANEL_MODE` line.

⚠️ `DCH_GUI_X`/`DCH_GUI_Y` are **not** used by this launcher. Do not reintroduce them without first
checking `grep -c DCH_GUI_X dch-server-gui.py` on the target box.

### `commission-stack.sh` — the actual answer to "every single time"

The geometry was never the whole problem; it was one of **four** defects a new or cloned stack
arrives with, each previously carried as tribal knowledge. They are now one idempotent, re-runnable
script, `linux_port/interface/commission-stack.sh`:

| # | defect on a fresh/cloned stack | why it matters |
|---|---|---|
| 1 | stock `start-kiosk.sh` | GUI overscans off the glass |
| 2 | `NTP=192.168.100.103` | that is the Pi on the **office rig** but the **STANDBY PC** at every HA plant (PC1=`.104`, PC2=`.103`, VIP=`.102`) — a clone aims its clock at the standby and never follows failover |
| 3 | hostname inherited from the clone source | three boxes all answering `dchserver2` makes every later diagnostic ambiguous |
| 4 | `Restart=on-failure` | never catches a **clean** GUI exit, leaving the box pingable with the controller dead |

```bash
scp commission-stack.sh dchservice@<ip>:/tmp/
ssh dchservice@<ip> 'bash /tmp/commission-stack.sh --check --ntp 192.168.100.102'   # report only
ssh dchservice@<ip> 'bash /tmp/commission-stack.sh --ntp 192.168.100.102'           # apply
```

- `--ntp` is **required** and has no default: it is the **VIP `192.168.100.102`** at an HA plant,
  but the Pi `192.168.100.103` on the office rig. Getting it wrong is defect #2 again.
- Hostname is derived from the last octet (`.11` → `dchserver1`); override with `--hostname`.
- The launcher body is **spliced from `start-kiosk.sh` at build time**, so the two cannot drift.
- It writes `start-kiosk.sh` **in place**, because that path is hardlinked to
  `/home/del/dchservices/bin/` — `mv`/`install` would break the link and leave one path stale.
- It **reports** `overhead`/`interface` md5s rather than deploying them; compare against the fleet.
- The `daemon-reload` for defect #4 is **gated on `NRestarts`** and refuses above 100,000 — a reload
  on a box with a huge accumulated count has segfaulted systemd and frozen PID 1. Reboot first.
- It does **not** restart the GUI unless given `--restart`. Restarting `dch-server-gui` **bounces
  the controller** — weighing stops and the load cell re-inits — so it is never implicit.

### Claxton state after this work

Lines 2, 3, 4 had the launcher applied and the GUI restarted (lines idle, Del approved); all four
lines now read `screen=1280x800  window=1280x800+0+0` and were confirmed on the glass. All four
controllers reconnected, `NRestarts=0`. All four run byte-identical binaries — `overhead`
`e1bf768c`, `interface` `6990c930`, `15.7.13 Sep 6 2026` — so **line 1 needed no software update**
despite being in place longest.

Still owed at Claxton (found by the scan, not yet applied — each needs a word from Del):
- lines 2–4: `NTP` → `.102`, hostnames → `dchserver3`/`dchserver4` (neither bounces the controller)
- **line 1 only**: `Restart=on-failure` → `always` (the one box still carrying defect #4)

### 🔴 Still open: the clone master

`commission-stack.sh` makes a stale clone a **one-command, verifiable** fix instead of four
remembered ones, but it is still a manual step. The loop only truly closes when the **clone-master
image** is rebuilt with this `start-kiosk.sh`. Until then, run `--check` on every new stack.
