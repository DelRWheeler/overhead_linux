# Scale Sync Offset

Status: **design locked 2026-06-27, build starting.**
Scope: **SandCat / Linux only.** EPM-19 / EPM-15 keep working with the interface and stay on the
legacy baked-in convention — their `rtx_source` is never modified.

## Why
Today the scale-sensor→weigh-deck distance (−1 turkey / −2 chicken / −3 some chains) is **implicit**:
every sync counts its drop offsets starting at that negative number, so the deck distance is baked
into every drop-offset value and lives only in the commissioning tech's head. This makes drop
offsets count from a confusing baseline and gives the turkey/chicken difference no visible setting.

Making it an explicit, per-scale **scale sync offset** lets drop offsets be counted **from 0**
(intuitive), turns the −1/−2/−3 into one visible number, and — critically — gives the controller the
deck geometry it needs for **auto-calibration** (the next feature, which locates the reference-weight
shackle from this offset).

## Decisions (Del, 2026-06-27)
1. **Calc lives in the SandCat controller** (host pushes the offset; controller applies it in the
   fire count). EPM-19/15 are untouched and stay baked-in.
2. **UI on Line Setup**, **one field per line** — dual-scale lines share one offset (both decks are
   always mounted the same distance from the sensor on a given line).
3. **Range 0 to −5** (covers −1/−2/−3 plus headroom; sensor is always at or before the deck).
4. Drop offsets are entered/counted **from 0** when a scale offset is set.
5. The offset folds into **all three** fire calcs — drops, grade, missed-bird — uniformly
   (`RingSub(shackle, offset + syncOffset, Shackles)`), since every sync is counted the same way.
6. **Sign is rig-proven, not assumed:** confirm a `from-0 + offset` config fires on the identical
   shackle count as the equivalent `offset 0 + baked-in` config before trusting it.

## The math
```
fire_count = sync_count + scale_offset + drop_offset
```
- `scale_offset = 0` + old baked-in drops  ==  today's behavior (legacy, bit-identical).
- real offset (−2) + drops counted from 0   ==  the same `fire_count`, the clean way.
- The entered drop NUMBERS shift by the offset magnitude (a drop that read 2 reads 4 from 0); same
  physical result.
- **Double-apply landmine:** a line is EITHER (from-0 drops + real offset) OR (baked-in drops +
  offset 0) — never both, or every drop walks `|offset|` trolleys off.

## Rig verification (no physical drops on the rig)
Success criterion = **the drop output fires on the correct shackle count and holds for the correct
on-duration** — nothing about physical bird placement. Verify by confirming a `scale_offset + from-0`
config fires on the **same shackle count and same on-duration** as the equivalent `offset 0 +
baked-in` config (via the controller drop-fire debug trace + output on-time). Optional aid: surface
the drop-output bits on the Sensor Scope to watch the fire land on the expected count.

## Layers
- **DB:** `scale_sync_offset` per line+scale, default 0.
- **Controller (`linux_port/overhead` only):** a new spare shmID (next free after the single-sensor
  96/97/98) carries `[scale1, scale2]` offsets; applied in the sync→drop fire-count calc; defaults 0
  if never received, so the legacy/EPM-19 path is bit-identical. Host arch-gated, so an EPM-19 never
  receives it.
- **API:** get/set + push (SandCat-gated, exactly like `sendZeroFlagMode`) on save and on bootstrap.
- **Web:** per-scale "Scale Sync Offset" field on Line Setup (0 to −5); drop-offset screens count
  from 0 when an offset is set.

## Compatibility / migration
Default 0 = legacy; existing plants unchanged. On each SandCat cutover, set the real offset and
re-count drops from 0 (or a verified `new = old + |offset|` with a couple of drops spot-checked).

## Out of scope (next feature)
Auto-calibration (the verify-span loop). Its reference shackle is derived from this offset
(`reference = zero/flag position + scale_offset`), which is why the offset goes into the controller now.
