# Robustness fixes — 2026-06-29 (SandCat/Linux controller + interface)

Two production-critical robustness fixes found while recovering a 2-line SandCat rig
after a controller lost power (flaky power supply). Both close failure modes that could
only be cleared by a power cycle — unacceptable for a plant install. SandCat/Linux only;
EPM-19 untouched.

---

## 1. TCP "Max Clients" stale-connection pile-up (interface)

**File:** `linux_port/interface/tcpserver.cpp`, `tcpconfig.h`

**Symptom:** Line 1's interface logged `Warning: Max clients reached, rejecting connection`
and rejected the host's legitimate connection.

**Root cause:** The TCP server keeps a fixed array of `TCP_MAX_CLIENTS` (6) slots. A new
connection takes the first free slot; `SO_KEEPALIVE` was on but with the Linux **default
~2-hour** dead-peer timeout. When a peer (the host, or an InterSystems controller) **lost
power / hard-rebooted with no FIN**, its old connection stayed "connected" for ~2 hours.
Each reconnect consumed another slot. A controller power-cycling (`.12`) piled **5 stale
connections** onto `.11`'s interface → all 6 slots full → every new (legitimate)
connection refused. Same class as the Claxton ghost-connection flood.

**Fix (makes it impossible by design):**
- **Per-peer dedup on accept:** before taking a slot, close any existing slot from the
  **same peer IP**. A peer uses exactly one connection, so a reconnect always reclaims its
  own slot — pile-up cannot happen.
- **Aggressive keepalive:** `TCP_KEEPIDLE=10s`, `TCP_KEEPINTVL=5s`, `TCP_KEEPCNT=3` →
  any other ghost dies in ~25 s instead of ~2 h.
- **Graceful eviction:** if all slots are somehow held by distinct live peers, evict the
  oldest rather than refuse the new one — a live connection is never rejected.

**Proven:** hard-reconnected `.12` three times in a row; `.11`'s server-slot count from
`.12` stayed at exactly **1**, never grew.

---

## 2. Single-sensor zero detector death-spiral (controller)

**File:** `linux_port/overhead/overhead.cpp` (`SingleSensorIsZeroTab`), `overhead.h`

**Symptom:** After a restart storm, lines counted and ran fine but **never zeroed** — the
shackle count climbed straight past `Shackles` with no reset, `bpm` stayed 0, no
distribution. The **grade** sync zeroed ("Initial Zero Flag Detected. Grade Sync 1") but
the **scale/count** sync never did. Only a power cycle "fixed" it (by chance re-seeding).

**Root cause:** In single-sensor (ZeroFlagMode==1 / competitor) mode there is no separate
zero sensor — the zero is a double-pulse (sheet-metal tab) on the one count sensor, detected
purely by timing. The detector **learns the trolley period T at runtime** (EMA) and calls a
gap "the zero tab" only if it is well under T. The hole: if T ever gets seeded/corrupted
**too small** (a bad gap during a stop/start), then every *real* trolley gap reads as
`gap > 3×T` → handled as "line momentarily stopped, keep T" → **T can never grow back, the
tab is never recognized, the line silently stops zeroing forever.** The author had guarded
the *other* corruption direction (don't fold short gaps into T) but not this one.

**Fix (self-heal):** track consecutive oversized gaps per sync. A real line stop is a
single big gap; if oversized gaps **persist (4 in a row)** the learned T is genuinely wrong,
so **re-seed T from the actual trolley interval**. The detector recovers on its own within a
fraction of a second instead of wedging until a power cycle. Added `ss_trolley_stall[]` /
`ss_grade_trolley_stall[]`; `SingleSensorIsZeroTab` takes a third `int &stall`.

**Proven:** deployed to both controllers; after restart both logged "Initial Zero Flag
Detected. **Scale 1**" and the shackle counts reset and held — single-sensor zero recovered
on both lines with no power cycle.

---

**Deploy:** interface + overhead rebuilt (x86_64) and deployed to `.11` and `.12`. Both
fixes will clone into every future SandCat controller image.
