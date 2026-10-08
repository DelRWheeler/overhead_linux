# Dead-Zone Omission & Reference Trolley

Status: **DONE — built, deployed to rig (.11/.12 + siggen .21), verified live, committed 2026-07-05.**
Scope: **SandCat / Linux only.** EPM-15 / EPM-19 run RTSS/Delphi and are never modified — a
no-offset plant is by definition an EPM box, so this change has zero legacy exposure. This is also
*why* the fix belongs in the controller (`overhead.cpp`), not the shared Interface.

## The bug that started this

With Auto-Span enabled on the rig (lines 1 & 2: `auto_span_enable=true`, `auto_span_known_weight=3`,
`auto_span_ref_shackle=2`), the welded 3-lb known-weight shackle rides the reference trolley every
revolution and **leaks into the unassigned-birds list** — one ~3.0-lb record per rev per line, always
`shackle=2`. It is not a coverage hole (normal 3.0-lb/grade-B birds assign fine to drops 11/12); it
is the reference shackle being counted as a bird.

Root cause in `overhead.cpp`:
- `6313` skips `FindDrops` for the reference (`if (!acIsRef) FindDrops(i+1);`) → its `drop` stays 0.
- but `AddDropRecord` (`10041`) still emits a host record for anything ≥ `MinBird`.
- host `onDropRecord` (`main.go:321`) sees `drop==0 && weight>0` → `InsertUnassignedBird`.

The controller comment already states the intent — *"never assign it to a drop **or count it**"* —
but only the "assign to a drop" half was implemented. The "or count it" half is this work.

Historically only **trolley 0** (the zero trolley, WeighShackle 1) was ever omitted from weighment.
Trolleys 1/2/3 were never omitted; they simply never carried a shackle, so nothing leaked — until a
known weight was parked in that zone.

## Physical model (2-sensor / standard sync)

Trolleys after the zero, with the rubberized zero flag:

| Trolley | WeighShackle | Flag        | Shackle mountable? | Role                                   |
|---------|--------------|-------------|--------------------|----------------------------------------|
| 0       | 1            | —           | No                 | Zero trolley (AutoZero)                |
| 1       | 2            | in dead-zone| No                 | reserved / empty                       |
| 2       | 3            | **attached**| No                 | reserved / empty                       |
| 3       | 4            | none        | **Yes**            | reserved slot — empty normally, **known weight under Auto-Span** |
| 4       | 5            | —           | Yes                | **first real bird**                    |

Key facts (Del, 2026-07-05):
- The flag is bolted **to trolley 2**, spanning toward 3 — so **trolley 3 has no flag and can hold a
  shackle**, but in normal operation no shackle is placed on trolleys 0–3.
- Trolleys 0–3 are **non-bird in both modes.** The only per-mode difference is trolley 3: under
  Auto-Span it carries the known weight and must be **weighed for span**; otherwise it is just dead.
- If someone *did* hang a bird on trolley 1/2/3 it would historically have weighed and distributed
  normally — there was never an explicit omission. That is what we are formalizing.

## Decisions (Del, 2026-07-05)

1. **Fix is controller-side** (`overhead.cpp`), SandCat-only. Interface/host untouched. No RTSS edits.
2. **Omission tracks the scale offset, gated by Auto-Span.** `N = |scale_offset|`:
   - **Auto-Span ON** omits trolleys **`0 … N`** → **first bird trolley `N+1`**.
   - **Auto-Span OFF (standard)** omits trolleys **`0 … N+1`** → **first bird trolley `N+2`**.
   - Auto-Span frees the zero-flag trolley (`N+1`) for a bird, so the first bird moves one
     trolley **earlier** when it is on. The reference known weight rides **inside** the dead
     zone (trolley 1 on the rig, shackle 2) — weighed for span, never distributed or counted.
     (Corrected 2026-07-05: an earlier draft wrongly put the reference on `N+1` and the first
     bird on `N+2` in both modes — Del caught it live on the rig.)
3. **`scale_offset = 0` preserves today's behavior** (omit trolley 0 only) even on SandCat, so a line
   that has not opted into the offset model is unaffected.
4. **Reference trolley is operator-settable** on the Features screen (SandCat-only / arch-gated).
   Turkey lines have different geometry; deriving it silently from the offset would eventually park
   the known weight on the wrong trolley. Default to `N+1`, allow override, **never auto-move it** on
   an offset change — warn instead.
5. **The reference is always excluded from distribution + host records wherever it is set** (it is a
   known weight, not a bird), and *additionally* weighed-for-span only when Auto-Span is on.

## The rule

```
N = |scale_offset|
bound = auto_span_enable ? N : N+1     // last omitted trolley (mode-gated)

distribution/record-omit(trolley) =
      trolley == 0                                  // zero trolley (existing)
   || (scale_offset != 0 && trolley in 1 .. bound)  // flag dead-zone (mode-gated depth)
   || trolley == reference_trolley                  // known weight, wherever set (turkey-safe)

weigh-for-span(trolley) =
      auto_span_enable && trolley == reference_trolley
```
In WeighShackle terms (`WeighShackle = trolley + 1`): omit `ws <= (auto_span_enable ? N+1 : N+2)`.

- `scale_offset == 0` → only trolley 0 omitted → **bit-identical to today.**
- `scale_offset == -2`, Auto-Span off → omit trolleys 0,1,2,3; first bird trolley 4.
- `scale_offset == -2`, Auto-Span on → omit 0,1,2,3 from distribution; trolley 3 also **weighed for
  span**; first bird trolley 4. (Same boundary — only the span read differs.)

Offset table (reference rides in the dead zone — trolley 1 / shackle 2 on the rig, settable):

| Offset | Auto-Span omits | Auto-Span 1st bird | Standard omits | Standard 1st bird |
|--------|-----------------|--------------------|----------------|-------------------|
| −1     | 0,1             | trolley 2          | 0,1,2          | trolley 3         |
| −2     | 0,1,2           | **trolley 3**      | 0,1,2,3        | trolley 4         |
| −3     | 0,1,2,3         | trolley 4          | 0,1,2,3,4      | trolley 5         |

(`shackle = trolley + 1`, so offset −2 Auto-Span first bird trolley 3 = shackle 4.)

## Implementation (as shipped)

**Tag at weigh time, consume downstream.** By the time a shackle reaches `AddDropRecord` the
`WeighShackle` counter has advanced, so "is this a dead/reference trolley?" cannot be recomputed
reliably there. So:

1. **Tag** — in the weigh loop (`overhead.cpp` ~6227, where `WeighShackle[i]` is authoritative),
   evaluate the rule and set **one** byte on that shackle's `ShackleStatus` entry: `OmitFromDrop`.
   It reuses `TShackleStatus.spare[0]`, so the struct stays **byte-identical** — the shared-memory
   layout is unchanged and the `interface` binary needs no rebuild. Internal only; never sent to host.
   ```c
   int  n     = abs(ScaleSyncOffset);
   int  bound = AutoSpanEnable ? (n+1) : (n+2);          // WeighShackle upper bound
   bool omit  = (ws == 1) || (off != 0 && ws >= 2 && ws <= bound) || acIsRef;
   ```
2. **FindDrops call site (~`6314`)** — `if (!WEIGH_SHACKLE(i,pShm).OmitFromDrop) FindDrops(i+1);`.
   No drop solenoid fires for a dead/reference trolley.
3. **`AddDropRecord` (~`10046`)** — early-return `if (pshk->OmitFromDrop) return;`. **No host record
   is created** → no phantom production bird, no phantom unassigned. *This is what closes the bug.*
4. **Span** — the per-rev AutoSpanMonitor was already removed; span is set only during Calibrate
   Shackle Tares (separate code path reading `WEIGH_SHACKLE` directly), so omitting the reference from
   distribution/records does not touch calibration. In production the reference is purely "don't count."

Because the tag is computed from live config (`ScaleSyncOffset`, `AutoSpanEnable`, `AutoSpanRefShackle`)
every revolution, toggling Auto-Span or changing the offset "just works" with no separate step.

**Siggen (`apps/signal-generator/hbm/measurement.go`).** So the rig physically demonstrates the model,
the HBM emulator's blank margin is mode-gated on `refEnabled` (= Auto-Span running):
`deadZoneAutoSpan = 2` (blank counters 0..2, first bird counter 3) / `deadZoneStandard = 3`. The
reference now injects on the configured `refCounter` (was hardcoded counter 1). **NOTE:** these margins
hardcode scale offset −2 (N=2); a different offset needs them bumped to N and N+1 (or wired to a
siggen scale-offset config).

**Trolley vs WeighShackle off-by-one.** `WeighShackle = trolley + 1` (trolley 0 = WeighShackle 1);
displayed shackle = WeighShackle. `auto_span_ref_shackle=2` = WeighShackle 2 = trolley 1 = the reference
— that exact confusion is where the bug hid. Label the future UI unambiguously.

## Verified live (rig, offset −2, Auto-Span on) — WS capture, zero crossing #4

```
shk 1  −0.00  drop 0   trolley 0 (zero)     dead
shk 2   3.00  drop 0   trolley 1 REFERENCE  known weight, NOT distributed, NO unassigned
shk 3  −0.00  drop 0   trolley 2            dead
shk 4   3.22  drop 10  trolley 3 FIRST BIRD distributed
shk 5+  birds, drops 10…
```
Both lines: `UNASSIGNED = 0`, and stayed 0 after Del cleared the pre-fix rows. Line 2 on the old binary
served as the A/B control (kept leaking) until it was upgraded too.

## Config / UI — remaining (not built)

- **DB**: `line_settings.scale_sync_offset` + `line_settings.auto_span_ref_shackle` already exist — no
  schema change.
- **Features screen** (SandCat / arch-gated): expose an operator-settable **reference trolley** for
  turkey lines. Default to the current value (trolley 1 / shackle 2); the reference rides *inside* the
  dead zone. On offset change, if the stored reference falls outside the dead zone, **warn** rather
  than silently mutate.

## Files

- `linux_port/common/overheadtypes.h` — `OmitFromDrop` (repurposed `spare[0]`). ✅ shipped
- `linux_port/overhead/overhead.cpp` — tag/consume, mode-gated dead-zone. ✅ shipped
- `apps/signal-generator/hbm/measurement.go` (overhead repo) — mode-gated blank margin. ✅ shipped
- `apps/web/src/pages/Features.tsx` + API — reference-trolley setting. ⬜ remaining

## Open

- Operator-settable reference trolley (Features UI) — designed above, not built.
- Siggen margins hardcode offset −2; generalize if another offset is ever demoed on the rig.
- Single-sensor mode (metal tab, not the rubber flag) may have a different dead-zone. **This doc
  covers 2-sensor / standard sync only;** single-sensor is a separate follow-up if needed.
