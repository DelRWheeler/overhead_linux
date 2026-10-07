# SandCat Controller — Fab Wiring Diagram: Status & Next Steps (2026-06-30)

## Goal (clarified end of day)
A **fabrication-grade** wiring diagram a **fab shop can build the cabinet from cold** — no
instructor standing over them. This is a higher bar than the reference diagrams made so far.

## What's DONE (reference diagrams — good for a tech, NOT yet fab-complete)
5 PDFs in the shared folder `/mnt/hgfs/VMSharedFiles/OverheadController/` and committed to
`overhead_controller/docs/`:
- `SandCat Controller Panel Wiring.pdf` — realistic cabinet front-view panel layout, **ANSI D (34×22)**, now includes the **10" door touch panel** (DB15 video + USB→expansion + 24 VDC).
- `SandCat Controller IO Loop Wiring.pdf` — PLC loop/harness style (11×17, 3 pages).
- `SandCat Controller Field Wiring.pdf` — bit-level point-to-point (11×17, 4 pages).
- `SandCat Controller Block Overview.pdf` — one-page system block.
- `SANDCAT_CONTROLLER_WIRING.md` — terminal schedule.

**Editable sources (regenerate via chromium headless):** repo `docs/sandcat_panel_wiring.html`,
`sandcat_loop_wiring.html`, `sandcat_p2p_wiring.html`, `sandcat_wiring_diagram.html`. The Python
generators live in the session scratchpad (gen_panel.py / gen_loop2.py / gen_p2p2.py) — TEMP, so
the repo HTML is the durable source. Render trick: snap chromium needs a **non-hidden file in
$HOME** (blocks /tmp + dotfiles); `chromium-browser --headless=new --no-pdf-header-footer
--print-to-pdf=/home/del/x.pdf file:///home/del/x.html`.

## Del's two specific gaps (the trigger)
1. **DB-9 load-cell connectors not shown** — I HAVE the data (DCH field-wiring PDF): per scale,
   DB-9 pin3 GRN→TXD−, pin8 BLK→RXD+, pin7 WHT→TXD+, pin2 RED→RXD−, GND→shield, landing on
   TB-3…6 (scale-1) / TB-9…12 (scale-2), then RS-422 to the PC COM. → **ADD THIS.**
2. **+5 V logic on the OPTO-22 modules not shown** — DCH TB-2 has F-17 → **+5 VDC** and TB-5 →
   **−5 VDC** "to CPU and I/O logic" = the OPTO backplane logic power. → **ADD THIS** (but exact
   backplane terminal needs the rack model, see below).

## Data I ALREADY HAVE (accurate)
- Full terminal schedule TB-1…56 + incoming power + TB-2 logic power (DCH field-wiring PDF, 5pp).
- OPTO channel ↔ 3724 bit map, **confirmed by Del**: In0=Scale-1 count, sequential "all the way
  around" (count=even ch, zero=odd); Out0=Drop-1…Out23=Drop-24 (A1/B1/C1); **power-loss = In21 =
  C0.5** (Del confirmed it wires to input module 21; no 2nd grade sync); missed-bird = C0.6/.7.
- DB-9 load-cell pinout (above). Fuses F-1…F-19. 120 VAC incoming.
- Physical layout from 8 cabinet photos + the touch-panel photo.

## What's MISSING for a true fab drawing (Del to provide — from the BOM)
1. **OPTO-22 module part numbers** (24 VDC-in input module; DC-out output module) **and the
   rack/backplane model** — the +5 V logic terminals AND the field-terminal positions are defined
   by that backplane. Guessing = shop builds it wrong.
2. **50-pin ribbon pinout** — which physical pin on the PCM-3724 header → which module position /
   channel, for each of the two ribbons (Port 0→input rack, Port 1→output rack).
3. **Internal wire schedule** — gauge + color for panel-INTERNAL wiring (have field colors, not
   internal).
4. **DB-9 gender / connector part numbers** and component designators the shop expects.

## >>> START HERE TOMORROW <<<
- **Find the BOM.** Del: it's in "our app on DO" (likely overheadsizing.com — check Products / a
  BOM feature) **or** on **DCHServiceSolutions.com**. Locate it → pull OPTO module + backplane
  part numbers, connector p/ns, and any wire schedule.
- Then: rebuild the panel diagram as a **fab package** — add DB-9 pinout + 5 V logic, plus the
  backplane detail, ribbon pinout, and a wire/BOM callout table. Consider splitting into
  fab-standard sheets (power, I/O racks + ribbon, terminal schedule, connector detail).

(Separate, still open from earlier today: Calibration Log screen, auto-cal reading ~0.47 lb /
"missing", timing-waveform ~1/3-cycle jump — see SANDCAT_GRADE_WAVEFORM_SESSION_2026-06-30.md.)
