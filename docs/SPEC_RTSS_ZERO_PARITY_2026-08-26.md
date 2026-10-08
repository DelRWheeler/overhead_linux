# Spec — RTSS zero parity and AutoSpan guards

**Status:** REVISED 2026-08-26 evening. Supersedes the earlier revision of this file,
which contained a **wrong premise** — see §0.1.
**Driver:** Holmes Foods incident 2026-08-25/26.
**Rule:** RTSS is the authority. Where RTSS has no equivalent (the known weight), a new
feature must not be allowed to defeat a restored RTSS behaviour.

---

## 0. Scope

Two changes, deployed and tested together:

- **A — zero parity.** Stop the host overriding the operator's stored `zero_bias_mode`,
  so the zero re-evaluates at every zero trolley as RTSS did. **Configuration only.**
- **B — AutoSpan guards.** RTSS `CheckWeight` (±25%) applied per pass, full-pass-count
  requirement, ±25% band on the average, 1% warning. **Controller binary.** Already built.

### 0.1 CORRECTION to the earlier revision

The earlier revision claimed the connect-time bootstrap does not push the zero mode, and
therefore that a Go change (adding `sendLineSettings` to `sendBootstrapSettings`) was
required for the fix to survive a controller restart.

**That was wrong.** `sendScaleSettings()` — which *is* in the bootstrap — already pushes it:

```go
// so the host is the only authority that survives a controller reboot.
putLE32(buf, 0, controller.EffectiveZeroBiasMode(lineSettings.AutoBias, lineSettings.ZeroBiasMode))
sendCommand(lineID, ctrlMgr, controller.CmdAutobiasMode, buf)
```

The error came from grepping for a `sendLineSettings` helper, not finding one, and
concluding the mode was unasserted. It is asserted — from a different helper.

**Consequences of the correction:**

- No Go change is required. No API rebuild, no API restart.
- `AUTOBIAS_MODE`, `ZeroNumber` (26) and `AutoBiasLimit` (27) are all pushed from the
  database on **every** controller connect, so a configuration change is inherently durable.
- It also explains how the controller reached mode 0 after the 19:24 restart on 08-25:
  the bootstrap put it there via `EffectiveZeroBiasMode(false, 2) → 0`. No line-settings
  save was involved.

---

## 1. Part A — zero parity (configuration only)

### 1.1 The deviation

RTSS defaults, from `rtx_source/Overhead/overhead.cpp`:

```c
app->zero_bias_mode          = 2;    // USE_ZERO_BIAS_LIM
pShm->scl_set.ZeroNumber     = 4;
pShm->scl_set.AutoBiasLimit  = 18;
```

`AutoZero()`, its call site, `ZeroAverage()` and all three `get_weight_init` reset points
are byte-identical between RTSS and the Linux port. The zero path was never ported wrong.

The single deviation is `EffectiveZeroBiasMode()`:

```go
if !autoBias {
    return ZeroBiasModeOff   // 0 — overrides the operator's stored mode
}
```

Delphi exposes **one** control — `cbZeroBiasMode`, a three-item selector mapping straight to
`ZeroFilterMode`. The "Enable Auto Bias" checkbox is a host invention with no counterpart in
the authority, and it overrides a correctly-stored mode.

Holmes stores `zero_bias_mode = 2` on both lines. The operator setting has always been right.

### 1.2 The change

```sql
UPDATE line_settings SET auto_bias = true, auto_bias_limit = 90000 WHERE line_id IN (1,2);
```

Then one `SET_LINE_SETTINGS` push per line to apply immediately rather than waiting for the
next connect.

`auto_bias = true` does **not** re-enable a host behaviour that was deliberately turned off.
It stops the host overriding the stored mode 2 — which is the RTSS configuration.

**Proper fix, deferred:** retire the checkbox and expose the three-way selector as Delphi
does, so the two controls cannot disagree. Weekend, with Pitman and Claxton.

### 1.3 `AutoBiasLimit` = 90,000 — Del's call, evidence below

RTSS's `18` is in raw counts and is therefore scale-dependent; it was set for analog cells.
Porting the *number* to HBM at 115,500 counts/lb gives 0.00016 lb, far below the natural
scatter. We port the *intent* — a band above normal scatter — not the literal.

Measured empty-deck scatter, 172 crossings on 2026-08-26:

| Line | mean | sd | range | 3σ |
|---|---|---|---|---|
| 1 | 16.328 lb | 0.332 | 16.037 – 16.881 | 0.997 lb (115,208 counts) |
| 2 | 16.141 lb | 0.248 | 15.868 – 16.563 | 0.743 lb (85,867 counts) |

**90,000 counts = 0.779 lb** — 2.35σ on line 1, 3.14σ on line 2, and above every deviation
observed (largest was +0.553 lb / 63,872 counts). So the snap-to-sample branch should not
fire in normal running; the zero rides the 4-deep average.

The limit must be **larger** than normal scatter. Below it, the out-of-limits branch fires
nearly every pass, the zero becomes the latest single sample, and bird spread widens
noticeably (projected sd 0.561 → ~0.652). The current 9999 (0.087 lb, 0.26σ) is far too small.

A lower limit also caps how far the zero can lurch in one revolution, since out-of-limits
causes an immediate full jump to that sample.

**Do not "correct" this back to 18.** It is a conscious, measured deviation.

---

## 2. Part B — AutoSpan guards (controller binary, built)

Exact port of RTSS `AutoSpan.pas`:

```c
check_under_weight = test_weight - (test_weight / 4);
check_over_weight  = test_weight + (test_weight / 4);
return (scale_weight > check_under_weight) && (scale_weight < check_over_weight);
```

- Applied **per pass**, before accumulation. RTSS drops a failing pass and does not advance
  the cycle, so it can only complete on four good readings — ours now matches.
- Completion requires the full accepted-pass count, or **span is never written** and a
  warning names accepted / required / rejected.
- ±25% band replaces 0.5×–2.0× on the average.
- RTSS's 1% high-bias warning, non-blocking as in the original.
- Rejected readings still appear in the Calibration Log (flag 6), rendered amber.

Every reading that inflated span during the incident (0.628–0.706× of known) fails this gate.
The two readings taken after the zero was repaired (0.888, 0.940×) pass it.

**Not included, deliberately:** the AutoBias-vs-trolley validation gate from the earlier
revision. It has no RTSS counterpart, and once mode 2 is active `AutoBias` is *derived from*
the zero-trolley reading, so the check is close to circular. The non-circular guard worth
building instead is an alarm when the effective mode is 0 while AutoSpan is enabled — that is
the genuinely dangerous combination. Weekend.

---

## 3. Deploy — 2026-08-26 evening

Holmes finished production ~15:33. Controllers up, line stopped.

### 3.1 Snapshot (DONE)

Held on `overhead-pc1:/home/DCHService/rollback_20260826/`.

| | Line 1 (.11) | Line 2 (.12) |
|---|---|---|
| `AutoBias` | 1,863,024 (16.1301 lb) | 1,831,131 (15.8539 lb) |
| `span_bias` | 126 | 64 |
| `auto_bias` / mode / limit | f / 2 / 9999 | f / 2 / 9999 |
| binary md5 | `10636ada…` | `10636ada…` |
| backup | `overhead.pre20260826`, `cmp` identical | same |

Audit: only lines 1 and 2 change effective mode (0 → 2). Lines 3/4 stay at 0.

### 3.2 Order

Line 2 first, then line 1. This is not staging the changes — both go on together — it is
just ordering the lines, which costs nothing and means we learn on one before touching two.

Per line:

1. Deploy the controller binary; restart the controller.
2. Apply the config change; push `SET_LINE_SETTINGS`.
3. Start the chain **empty and running**.
4. Let the zero settle. **Watch `zero_bias` in `auto_span_log` across crossings — it must now
   MOVE.** If it stays constant, mode 2 did not take, and nothing else in this plan is valid.
5. Run the tare/span calibration with **4 cycles** (RTSS used 4; `TareTimes` is operator-typed
   and is not stored in the database).
6. Verify against §3.3.

### 3.3 Acceptance, per line

- reference reads **2.95 ± 0.05**, flag 0, across **≥3 consecutive crossings**
- derived span near **121–126** (line 1) / **64–69** (line 2)
- `zero_bias` **varies** between crossings — this is the proof mode 2 is live
- once running: bird means near 3.5 / 3.3, sd near 0.5

### 3.4 Abort and roll back if

- derived span outside roughly **100–150** (line 1) or **45–90** (line 2)
- reference will not hold 2.95 ± 0.05 over 3 crossings
- calibration will not complete after two honest attempts
- settled empty-chain zero readings swing beyond **±0.5 lb**
- `zero_bias` still constant after the config push (mode 2 did not take)

### 3.5 Rollback

`overhead-pc1:/home/DCHService/rollback_20260826/holmes_rollback.sh <1|2>` — one line at a
time, six steps, every value verified by read-back rather than exit code. Refuses to run if
the backup md5 does not match. Prompts before the ACS toggle so it is timed away from the
reference crossing.

Note the ordering constraint it encodes: **`AutoBias` must be written after the controller
restart**, because the restart re-seeds it.

---

## 4. Operational notes

- **`TareTimes` is not in the database.** Operator-typed per calibration. The failed
  calibrations on 08-26 ran 2 passes, the successful one 1. RTSS was hardcoded to 4. Under
  the new guard every configured pass must land inside ±25% or no span is written.
- **`SpanBias` remains host-writable** via shm id 32 (`SET_SPAN_BIAS`), bypassing every guard
  here. That is the deliberate escape hatch used to recover the plant on 08-26 morning and
  should stay — but nothing on that path is range-checked.
- **ACS holds span write authority** while enabled; manual `SET_SPAN_BIAS` is rejected
  host-side. Disabling ACS to write a span must only ever be done with the welded reference
  away from the scale.
- **`adc_reads` is analog-only** and inert on these HBM lines. **`avg_start`/`avg_end` differ
  per line by design** — they compensate for slightly different physical sensor positions.
  Neither is a fault.

---

## 5. Fleet

| Site | Configuration | Action |
|---|---|---|
| Holmes | auto_bias off, ACS on | tonight |
| Pitman Farms | identical combination | weekend — one restart from the same failure |
| Claxton | to confirm | weekend, after its own audit |

Every site needs its own audit before the config change:

```sql
SELECT line_id, auto_bias, zero_bias_mode, auto_bias_limit FROM line_settings ORDER BY line_id;
```

Effective mode today = `auto_bias ? zero_bias_mode : 0`; after = `zero_bias_mode`. Any line
where those differ changes behaviour on deploy and must be signed off individually.
`auto_bias_limit` must be set from that site's own measured empty-deck scatter — **not**
copied from Holmes' 90,000.
