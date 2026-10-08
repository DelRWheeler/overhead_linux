# SandCat Overhead Controller — Field Wiring Diagram

Built from the **official DCH field-wiring drawing** (`DCH Overhead Field Wiring.pdf`, 5 pp) — the
authoritative terminal schedule — cross-referenced with the controller I/O firmware
(`linux_port/overhead/3724_io.cpp`, `3724_io.h`, `common/overheadconst.h`) and the cabinet photos.
This is the box run as line 1 at `192.168.100.11`.

Two terminal blocks: **TB-1** = all field wiring (sensors, load cells, kicker outputs, incoming
power). **TB-2** = internal logic-power distribution (no field connections).

---

## 1. Incoming power (TB-1, end)

| Terminal | Signal |
|----------|--------|
| TB-L1 | Incoming 120 VAC 60 Hz **HOT** |
| TB-L2 | Incoming 120 VAC 60 Hz **NEUTRAL** |
| TB-GND | Incoming power **GROUND** |
| F-16 (6 A) | Main incoming-power fuse |

## 2. Logic power (TB-2 — internal, no field landing)

| Source | Rail |
|--------|------|
| F-17 (3 A) | **+5 VDC** to CPU & I/O logic (TB+5) |
| TB-5 | **−5 VDC** to CPU & I/O logic |
| F-18 (3 A) | **+24 VDC to OPTO boards** (the white/red OPTO-22 banks) |
| F-19 (3 A) | **+24 VDC to monitor** (door display) |
| TB+24 ×5 / TB−24 ×5 | 24 VDC positive / common buss (distribution) |
| TB-GND ×2 | ground |

---

## 3. Scales — sync signals + RS-422 load cells (TB-1, p1)

Each scale has a **shackle-detect (SD = count/sync)** and **zero-detect (ZD)** sensor signal, its
own fused +24, and an **RS-422 load cell on a DB-9** (4 data + shield). **The load cell is serial to
the PC COM port — it does NOT go through the 3724 / OPTO banks.**

| Function | Terminal | Wire | Fuse |
|----------|----------|------|------|
| Scale-1 +24 / −24 | (F-1) / TB-24 com | RED / BLACK | F-1 (3 A) |
| Scale-1 **SD signal** (count) | **TB-1** | WHITE | |
| Scale-1 **ZD signal** (zero) | **TB-2** | GREEN | |
| Scale-1 load cell | **TB-3..TB-6 + TB-SHLD** | RS-422 | |
| Scale-2 +24 / −24 | (F-2) / TB-24 com | RED / BLACK | F-2 (3 A) |
| Scale-2 **SD signal** | **TB-7** | WHITE | |
| Scale-2 **ZD signal** | **TB-8** | GREEN | |
| Scale-2 load cell | **TB-9..TB-12 + TB-SHLD** | RS-422 | |

**Load-cell DB-9 ↔ terminal ↔ cable (per scale):**

| DB-9 pin | Wire | TB (scale 1 / scale 2) | RS-422 signal |
|----------|------|------------------------|---------------|
| 3 | GREEN | TB-3 / TB-9 | TXD− |
| 8 | BLACK | TB-4 / TB-10 | RXD+ |
| 7 | WHITE | TB-5 / TB-11 | TXD+ |
| 2 | RED | TB-6 / TB-12 | RXD− |
| GND | BARE | TB-SHLD | SHIELD |

Also p1: TB-4..TB-7 carry **ORANGE +24VDC / WHITE −24VDC** to the scale (HBM amp power).

---

## 4. Drop syncs 1–6 (TB-1, p1–p2)

Each drop-zone sync has SD (count) + ZD (zero), own fused +24, shared −24 (TB-24 common).

| Sync | +24 fuse | SD (count) | ZD (zero) |
|------|----------|------------|-----------|
| Drop Sync-1 | F-3 (3 A) | **TB-13** | **TB-14** |
| Drop Sync-2 | F-4 (3 A) | **TB-15** | **TB-16** |
| Drop Sync-3 | F-5 (3 A) | **TB-17** | **TB-18** |
| Drop Sync-4 | F-6 (3 A) | **TB-19** | **TB-20** |
| Drop Sync-5 | F-7 (3 A) | **TB-21** | **TB-22** |
| Drop Sync-6 | F-8 (3 A) | **TB-23** | **TB-24** |

(SD = WHITE, ZD = GREEN throughout.)

## 5. Grade (TB-1, p2)

| Function | +24 fuse | Terminal |
|----------|----------|----------|
| Grade **sync** SD (count) | F-9 (3 A) | **TB-25** (WHITE) |
| Grade **sync** ZD (zero) | | **TB-26** (GREEN) |
| **Grade detect-1** | F-10 (3 A) | **TB-27** |
| **Grade detect-2** | | **TB-28** |
| **Grade detect-3** | | **TB-29** |
| (spare) | | TB-30 = **no connection** |

3 grade detects → 4 grades (A/B/C/D). Grade **sync** times the read; grade **detects** are the
photo-eyes that read the bird.

## 6. Missed-bird detects (TB-1, p3)

| Function | +24 fuse | Terminal |
|----------|----------|----------|
| Missed Bird Detect-1 | F-11 | **TB-31** |
| Missed Bird Detect-2 | F-11 | **TB-32** |

## 7. Drop (kicker) outputs 1–24 (TB-1, p3–p4)

24 kicker solenoid outputs (BLACK return), fed by fused +24 banks of 8.

| Drops | +24 bank fuse | Output terminals |
|-------|---------------|------------------|
| 1–8 | F-12 (5 A) | **TB-33 … TB-40** |
| 9–16 | F-13 (5 A) | **TB-41 … TB-48** |
| 17–24 | F-14 (5 A) | **TB-49 … TB-56** |
| spare | F-15 (5 A) | (reserved +24) |

`DROP-n` lands on `TB-(32+n)` (DROP-1→TB-33 … DROP-24→TB-56).

---

## 8. Firmware cross-reference (terminal → PCM-3724 → variable)

The PC drives **PCM-3724** DIO cards over PC/104 (base **0x300**, one card per line). Field signals
map to firmware as:

| Field signal (TB) | 3724 port (bank0 IN / bank1 OUT) | Firmware var |
|-------------------|----------------------------------|--------------|
| Scale/Drop **SD** (count) signals | Port **A0/B0** even bits | `sync_in[]` |
| Scale/Drop **ZD** (zero) signals | Port **A0/B0** odd bits | `sync_zero[]` |
| **Grade** sync SD/ZD, **grade detects 1-3**, **missed-bird** | Port **C0** | `switch_in[]` (b0 gradeSync, b1 gradeZero, b2-4 grade detect, **b5 = power-loss/UPS**) |
| **Drop outputs 1-24** | Port **A1/B1/C1** (active-low) | `output_byte[0..2]` |
| **Load cells (RS-422)** | **NOT on 3724** — DB-9 → PC COM | serial / HBM driver |

Inputs are OPTO-22-sinking so firmware reads them inverted (`~`); outputs written inverted too.
Power-loss: **C0 bit 5** (the DC-UPS "external power" contact) → countdown → graceful shutdown.

---

## 9. Block diagram (now terminal-accurate)

```
 120VAC L1/L2/GND ─► F-16(6A) ─► PULS CP10.241 (24V) ─► PULS UB10.241 (DC-UPS buffer)
                                                              │ 24V bus (F-18 → OPTO boards, F-19 → monitor)
                                                              │ F-17 → ±5V CPU/IO logic
   ┌──────────────────────────────────────────────────────────┘
   ▼
 INDUSTRIAL PC (x86_64, overhead+interface)
   ├─ Ethernet ─────────► TCP :5000 ─► Host Pi (.103)
   ├─ COM RS-422 ───────► Load cells  ◄─ DB-9 ◄─ TB-3..6 (scale1) / TB-9..12 (scale2)
   ├─ VGA ──────────────► door display
   └─ PC/104 (0x300…) ─► PCM-3724 (×4)
         IN  A0/B0 ◄─ WHITE OPTO-22 ◄─ SD/ZD sync signals:
         │                 Scale1 TB-1/2 · Scale2 TB-7/8
         │                 DropSync1-6 TB-13..24
         │   C0    ◄─ Grade sync TB-25/26 · Grade detect1-3 TB-27/28/29
         │              Missed-bird TB-31/32 · (C0 b5 = UPS power-loss)
         OUT A1/B1/C1 ─► RED OPTO-22 ─► kicker solenoids:
                            DROP-1..24  TB-33..56
```

**Cabinet layout (photos):** top shelf = PCM-3724 carrier; mid shelf = WHITE OPTO-22 **inputs**
(left) + RED OPTO-22 **outputs** (right); bottom = PULS supplies (CP10.241 24V + UB10.241 DC-UPS +
redundancy) + breakers + the TB-1/TB-2 terminal rails (right side & bottom).

---

## 10. Quick reference — TB-1 terminal index

| TB | Signal | TB | Signal |
|----|--------|----|--------|
| 1 | Scale-1 SD | 25 | Grade sync SD |
| 2 | Scale-1 ZD | 26 | Grade sync ZD |
| 3–6 | Scale-1 load cell (RS-422) | 27–29 | Grade detect 1–3 |
| 7 | Scale-2 SD | 30 | (no connection) |
| 8 | Scale-2 ZD | 31–32 | Missed-bird detect 1–2 |
| 9–12 | Scale-2 load cell (RS-422) | 33–40 | DROP 1–8 |
| 13–14 | Drop Sync-1 SD/ZD | 41–48 | DROP 9–16 |
| 15–16 | Drop Sync-2 SD/ZD | 49–56 | DROP 17–24 |
| 17–18 | Drop Sync-3 SD/ZD | | |
| 19–20 | Drop Sync-4 SD/ZD | F-1..F-10 | 3 A (sensor +24) |
| 21–22 | Drop Sync-5 SD/ZD | F-12..F-15 | 5 A (drop +24) |
| 23–24 | Drop Sync-6 SD/ZD | F-16 | 6 A (main) |

Source: `DCH Overhead Field Wiring.pdf` (DCH Service Solutions, 154 Sunset St, Clarkesville GA).
```
