# Spec — RTSS zero parity and known-weight/zero coupling

**Status:** SPEC ONLY — not built. For build + deploy the evening of 2026-08-26.
**Driver:** Holmes Foods incident 2026-08-25/26. See `REPORTS_DATE_OFFSET` companion and the
incident report for background.
**Rule:** RTSS is the authority. Where RTSS has no equivalent (the known weight), the design
must not let a new feature defeat a restored RTSS behaviour.

---

## 0. Scope

Two changes that must ship **together or not at all**:

- **Part A** — restore RTSS zero behaviour (the zero re-evaluates at every zero trolley).
- **Part B** — make the known-weight reference account for the zero, so restoring A cannot
  drag the reference the way it did on 2026-08-17.

Part A alone reproduces the 08-17 known-weight shift. Part B alone leaves the frozen zero in
place. The AutoSpan `CheckWeight` guards (already built, separate change) are independent of
both and can ship on their own.

---

## 1. Part A — restore RTSS zero behaviour

### 1.1 The deviation

RTSS defaults, from `rtx_source/Overhead/overhead.cpp`:

```c
app->zero_bias_mode          = 2;    // USE_ZERO_BIAS_LIM
pShm->scl_set.ZeroNumber     = 4;
pShm->scl_set.AutoBiasLimit  = 18;
```

`AutoZero()`, its call site, `ZeroAverage()` and all three `get_weight_init` reset points are
byte-identical between RTSS and the Linux port. The zero path was never ported wrong.

The deviation is host-side, in `apps/api/internal/controller/client.go`:

```go
func EffectiveZeroBiasMode(autoBias bool, mode int) int32 {
    if !autoBias {
        return ZeroBiasModeOff   // 0 — overrides the operator's stored mode
    }
    ...
}
```

Delphi exposes a **single** control — `cbZeroBiasMode`, a three-item selector mapping straight
to `ZeroFilterMode` (0 = No Auto Bias, 1 = Zero Avg at Shackle 1, 2 = Zero w/ Range Check).
The "Enable Auto Bias" checkbox is a host invention with no counterpart in the authority, and
it silently overrides a correctly-stored mode.

Holmes stores `zero_bias_mode = 2` on both lines. The operator setting has been right all along.

### 1.2 Change

Push the operator's stored `zero_bias_mode` verbatim, as Delphi did.

- **A1 (controller-facing, required):** remove the `!autoBias` override so the stored mode
  reaches the controller unmodified.
- **A2 (UI, follow-up — not tonight):** retire the "Enable Auto Bias" checkbox and expose the
  three-way selector as Delphi does, so the two controls can never disagree again. Until A2
  lands, the checkbox must not be the thing that decides the mode.

### 1.3 MANDATORY pre-deploy audit

A1 changes zero behaviour at **any** line whose stored `zero_bias_mode` differs from the mode
it is effectively running today. This must be enumerated before deploying anywhere.

For every line at every site, record and review:

```sql
SELECT line_id, auto_bias, zero_bias_mode, auto_bias_limit FROM line_settings ORDER BY line_id;
```

Effective mode today = `auto_bias ? zero_bias_mode : 0`. Effective mode after A1 =
`zero_bias_mode`. **Any line where those differ changes behaviour on deploy** and must be
signed off individually. Do not deploy A1 to a site that has not been audited.

Known today: Holmes lines 1 and 2 are `auto_bias = f`, `zero_bias_mode = 2` — both change from
effective 0 to effective 2. Lines 3 and 4 are `auto_bias = t`, `zero_bias_mode = 0` — unchanged
(and are not physically present at Holmes).

### 1.4 `AutoBiasLimit` — needs a measured decision

RTSS default is **18**, set for analog cells. Holmes runs HBM at 115,500 counts/lb and has
`auto_bias_limit = 9999`, which is **0.087 lb** — far below the observed empty-deck scatter of
roughly ±0.5 lb (≈ ±57,750 counts).

In `USE_ZERO_BIAS_LIM` the branch is:

```c
if ((wt > (average_zero + AutoBiasLimit)) || (wt < (average_zero - AutoBiasLimit)))
    pShm->AutoBias[s] = wt;          // treat as something fell on/off the scale
else
    pShm->AutoBias[s] = average_zero;
```

With a limit far below the natural scatter, the out-of-limits branch fires nearly every pass
and the zero becomes "the latest single reading". That is still self-correcting — a bad draw
survives one revolution — and matches RTSS's practical behaviour on these lines. But it forfeits
the averaging, and the value should be **chosen deliberately rather than inherited**.

Proposal, to be confirmed by measurement during tonight's line test: set the limit so ordinary
scatter is averaged and only genuine events (something on the deck) trip the reset branch —
approximately **0.5 lb ≈ 57,750 counts** at Holmes' scaling. Measure the actual empty-deck
distribution over several hundred revolutions first; do not guess.

**Open for Del.**

---

## 2. Part B — the known weight must account for the zero

### 2.1 The asymmetry

The reference shackle has its tare forced to zero:

```c
TARE_SHACKLE(s, pShm) = 0;
```

Its reading is `(raw − AutoBias) × (1 + span/1000)`. When `AutoBias` moves, the reference moves
with it. Ordinary birds do **not** — their tares were captured post-subtraction and absorb the
shift. That asymmetry is why:

- a frozen bad zero left birds correct but the reference reading 0.75 lb low, and
- enabling zero tracking on 2026-08-17 moved the known weight 2.96 → 2.81.

RTSS never met this because RTSS had no known-weight reference.

### 2.2 Change — gate span derivation on a validated zero

At the zero trolley the deck is empty by definition. Any disagreement between the stored zero
and the measured empty deck is a zero fault.

1. Accumulate the zero-trolley reading over `N` passes (proposal: `N = ZeroNumber = 4`).
2. Before **any** span derivation in the Part 1 completion path, compare the stored `AutoBias`
   against that averaged reading.
3. On disagreement beyond the band: **do not compute span**, raise `GenError(warning, ...)`
   naming both values and the delta, and set a distinct alarm code.
4. Log both figures on every span record. `rec[6]` already carries `AutoBias` and
   `zero_trolley_weight` is already captured — no new telemetry needed.

**Span must never be allowed to absorb a zero error.** Every guard here serves that one rule.

### 2.3 Threshold — needs a decision

Evidence from the incident (line 1, in pounds):

| State | `AutoBias` | zero-trolley reading | delta |
|---|---|---|---|
| Healthy (before shutdown) | 16.130 | 16.353 | **−0.22** |
| Fault | 16.881 | 16.407 | **+0.47** |

The delta swung **0.70 lb** — the full size of the zero error. Single-pass trolley sd is
**0.36 lb**; averaged over 4 passes that falls to ≈ **0.18 lb**.

Proposal: band of **±0.30 lb on a 4-pass average** — catches the 0.70 swing with margin while
sitting ≈1.7σ clear of the noise. Alternatively expressed as a fraction of the known weight.

**Open for Del.**

### 2.4 Note — the trolley scatter is itself unexplained

Logged empty-deck readings on line 1 span **16.03 → 17.08 lb**. Over a pound of spread on what
should be a physical constant, and larger than the fault it concealed. This is not a blocker for
the spec but it deserves its own investigation — it sets the floor on how tight any zero
validation can ever be.

---

## 3. Part C — ordering interlock

The failure mode this whole spec addresses is *calibrating against an unsettled zero*. Encode
the commissioning sequence rather than relying on procedure:

1. Power up controllers.
2. Start the line **empty and running**.
3. Zero settles under mode 2 across `N` zero-trolley passes.
4. Run the tare/span calibration — **4 cycles**, matching RTSS.
5. Verify the reference reads known ± band with flag 0.

**Interlock:** refuse to run span derivation until the zero has been stable within band across
`N` consecutive zero-trolley passes. Report "zero not settled" rather than producing a span.

Whether this ships tonight or follows is **open for Del** — it is the largest of the three
parts and the least forced by the incident, since Part B already blocks the bad case.

---

## 4. Related operational facts

- **`TareTimes` is not stored in the database.** It is typed by the operator per calibration.
  This morning's failed calibrations ran with 2 passes, the successful one with 1. RTSS was
  hardcoded to **4**. With the new AutoSpan guards, every configured pass must land inside
  ±25% or no span is written.
- **`SpanBias` remains host-writable** via shm id 32 (`SET_SPAN_BIAS`), bypassing every guard
  here. That is the deliberate escape hatch used to recover the plant this morning and should
  stay — but nothing on that path is range-checked.
- **Auto Calculate Span holds write authority over span** while enabled; manual
  `SET_SPAN_BIAS` is rejected host-side. Disabling ACS to write a span must only ever be done
  with the welded reference away from the scale.

---

## 5. Build, deploy, rollback

**Build.** From a clean clone, not a worktree. Verify the binary's mtime actually advanced and
confirm markers with `grep -a` — "Built target" prints even when nothing rebuilt. Record md5.

**Deploy.** One line at a time. Holmes tonight after production ends; Pitman and Claxton at the
weekend, each only after its own audit (§1.3).

**Rollback.** Retain the current binary. Restoring it, plus writing the known-good `AutoBias`
(L1 `1,863,024`, L2 `1,831,131`) and span (L1 `121`, L2 `69`), returns the plant to its current
working state. Verify by read-back, not by return code.

**Acceptance, before tomorrow's start-up.** Per line:

- reference reads `2.95 ± 0.05` with flag 0 across **≥3 consecutive crossings**
- derived span lands near `121` / `69`
- bird means return to roughly `3.6` / `3.4` with sd near `0.5`

If the derived span comes back far from those figures, **stop** — the zero is not settled, and
calibrating on an unsettled zero is what caused this.

---

## 6. Open questions

1. `AutoBiasLimit` value for HBM scaling (§1.4).
2. Zero-validation band and averaging depth (§2.3).
3. A1 only tonight, or A1 + retire the checkbox (A2) together (§1.2).
4. Does the Part C stability interlock ship tonight or follow (§3)?
