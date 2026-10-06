#!/bin/bash
#--------------------------------------------------------------------------
#  sszero_crosscheck_test.sh - build + run sszero_crosscheck_test.cpp (15.7.16)
#
#  Drives the REAL controller code, cut out of overhead.cpp:
#    NEW     ProcessSyncs / GradeSyncs + every SingleSensor* helper incl. the 15.7.16
#            cross-sync check (SingleSensorXC*), from the working tree
#    OLD     ProcessSyncs from git $OLD_REV (default b269cf8 = 15.7.15, md5 cfacfa13,
#            the build at Pitman). The helpers it calls are asserted byte-identical
#            between $OLD_REV and the working tree, so OLD == the deployed code.
#    OLD13   ProcessSyncs / SingleSensorIsZeroTab / SingleSensorWarnOk from f9f2968
#            (15.7.13), only to replay the 14:45 capture that 15.7.13 recorded.
#    SPEC    NEW with the cross-check verdict replaced by the literal spec rule
#            (symmetric 2-of-3 majority, k = +/-1, no anchor rule, no stop rule) - to
#            show what that rule does on the real captures. Test-only, by sed.
#    NOTRACK NEW with offset tracking switched off (scalar offsets). Test-only, by sed.
#  and replays the real Pitman Sensor Scope captures ($CAPDIR, default
#  /home/del/data/pitman-scope-2026-10-05; skipped if absent) plus synthetic chains.
#
#  Usage:   linux_port/tools/sszero_crosscheck_test.sh [group ...]
#           groups: real synth_pitman synth_chicken synth_faults synth_2sync synth_phase
#                   synth_double synth_4sync identity  (default: all)
#           LOG=<file> keeps the output; KEEP=1 keeps the build dir.
#--------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
LP="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$LP/.." && pwd)"
OLD_REV="${OLD_REV:-b269cf8}"
OLD13_REV="${OLD13_REV:-f9f2968}"
CAPDIR="${CAPDIR:-/home/del/data/pitman-scope-2026-10-05}"
OUT="${TMPDIR:-/tmp}/sszero_crosscheck_test.$$"
mkdir -p "$OUT"
[ -n "${KEEP:-}" ] || trap 'rm -rf "$OUT"' EXIT

extract() {
    tr -d '\r' < "$1" | awk -v sig="$2" '
        done { next }
        !on && index($0, sig) == 1 { on = 1 }
        on { print; if ($0 ~ /^}/) done = 1 }'
}
subst() {
    local n; n=$(grep -cF "$4" "$1" || true)
    [ "$n" -eq "$3" ] || { echo "sed anchor '$4' matched $n times in $1, expected $3"; exit 2; }
    sed -i "$2" "$1"
}

NEWSRC="${NEWSRC:-$LP/overhead/overhead.cpp}"
tr -d '\r' < <(git -C "$REPO" show "$OLD_REV:linux_port/overhead/overhead.cpp")   > "$OUT/old.cpp"
tr -d '\r' < <(git -C "$REPO" show "$OLD13_REV:linux_port/overhead/overhead.cpp") > "$OUT/old13.cpp"

HELPERS=( "int overhead::RingSub(" "bool overhead::SingleSensorIsZeroTab(" "bool overhead::SingleSensorTabRule("
          "int overhead::SingleSensorRevTrolleys()" "int overhead::SingleSensorRevGate()"
          "bool overhead::SingleSensorBootConfirm(" "bool overhead::SingleSensorBootPending("
          "void overhead::SingleSensorTrolleyEdge(" "void overhead::SingleSensorAlarmTick("
          "bool overhead::SingleSensorAlarmOk(" "void overhead::GradeSyncs()" )
XC=( "double overhead::SingleSensorXCWrap(" "double overhead::SingleSensorXCPos(" "bool overhead::SingleSensorXCEligible("
     "bool overhead::SingleSensorXCSteady(" "double overhead::SingleSensorXCOffset(" "void overhead::SingleSensorXCGapEdge("
     "void overhead::SingleSensorXCDrop(" "void overhead::SingleSensorXCZero(" "void overhead::SingleSensorXCOverrun("
     "int overhead::SingleSensorXCCheck(" "bool overhead::SingleSensorXCApply(" )

#--- shared: the helpers OLD and NEW both call (asserted identical) + the cross-check
: > "$OUT/xc_shared.inc"
for f in "${HELPERS[@]}"; do
    extract "$NEWSRC" "$f" > "$OUT/n.tmp"; extract "$OUT/old.cpp" "$f" > "$OUT/o.tmp"
    [ -s "$OUT/n.tmp" ] || { echo "extract failed: $f"; exit 2; }
    cmp -s "$OUT/n.tmp" "$OUT/o.tmp" || { echo "UNEXPECTED: '$f' differs between $OLD_REV and the working tree"; exit 2; }
    cat "$OUT/n.tmp" >> "$OUT/xc_shared.inc"
done
tr -d '\r' < "$NEWSRC" | grep -E '^static inline (double|int) +ssxc_(abs|round)\(' >> "$OUT/xc_shared.inc"
[ "$(grep -c 'ssxc_' "$OUT/xc_shared.inc")" -eq 2 ] || { echo "extract failed: ssxc_ helpers"; exit 2; }
for f in "${XC[@]}"; do
    extract "$NEWSRC" "$f" > "$OUT/n.tmp"
    [ -s "$OUT/n.tmp" ] || { echo "extract failed: $f"; exit 2; }
    cat "$OUT/n.tmp" >> "$OUT/xc_shared.inc"
done
# NOTRACK: the cross-check without offset tracking
extract "$NEWSRC" "int overhead::SingleSensorXCCheck(" | sed 's/overhead::SingleSensorXCCheck(/overhead::SingleSensorXCCheck_NOTRACK(/' >> "$OUT/xc_shared.inc"
subst "$OUT/xc_shared.inc" 's/            p\.d = SingleSensorXCWrap(p\.d + f \/ SS_XC_TRACK_GAIN);/            ;  \/* NOTRACK *\//' 2 'p.d = SingleSensorXCWrap(p.d + f / SS_XC_TRACK_GAIN);'
# (the first match is in the real SingleSensorXCCheck - put it back)
python3 - "$OUT/xc_shared.inc" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
a = s.index("int overhead::SingleSensorXCCheck(int i, int numSyncs)")
b = s.index(";  /* NOTRACK */", a)
s = s[:b] + "p.d = SingleSensorXCWrap(p.d + f / SS_XC_TRACK_GAIN);" + s[b + len(";  /* NOTRACK */"):]
open(p, "w").write(s)
PY
[ "$(grep -c 'NOTRACK \*/' "$OUT/xc_shared.inc")" -eq 1 ] || { echo "NOTRACK mutant failed"; exit 2; }

#--- ProcessSyncs variants
{
    extract "$NEWSRC"      "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_NEW()/'
    extract "$NEWSRC"      "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_SPEC()/;s/SingleSensorXCCheck(i, NumSyncs)/SpecXCCheck(i, NumSyncs)/'
    extract "$NEWSRC"      "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_NOTRACK()/;s/SingleSensorXCCheck(i, NumSyncs)/SingleSensorXCCheck_NOTRACK(i, NumSyncs)/'
    extract "$OUT/old.cpp" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_OLD()/'
} > "$OUT/xc_ps.inc"
[ "$(grep -c 'SpecXCCheck(i, NumSyncs)' "$OUT/xc_ps.inc")" -eq 1 ] || { echo "SPEC mutant failed"; exit 2; }
[ "$(grep -c 'SingleSensorXCCheck_NOTRACK(i, NumSyncs)' "$OUT/xc_ps.inc")" -eq 1 ] || { echo "NOTRACK ps failed"; exit 2; }
[ "$(grep -c '^}' "$OUT/xc_ps.inc")" -eq 4 ] || { echo "extract failed: xc_ps.inc"; exit 2; }
grep -q 'SingleSensorXC' <(extract "$OUT/old.cpp" "void overhead::ProcessSyncs()") && { echo "UNEXPECTED: OLD has the cross-check"; exit 2; }
{
    extract "$OUT/old13.cpp" "void overhead::ProcessSyncs()"          | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_OLD13()/'
    extract "$OUT/old13.cpp" "bool overhead::SingleSensorIsZeroTab(" | sed 's/overhead::SingleSensorIsZeroTab(/overhead::SingleSensorIsZeroTab_OLD13(/'
    extract "$OUT/old13.cpp" "bool overhead::SingleSensorWarnOk()"   | sed 's/overhead::SingleSensorWarnOk()/overhead::SingleSensorWarnOk_OLD13()/'
} > "$OUT/xc_old13.inc"
subst "$OUT/xc_old13.inc" 's/? SingleSensorIsZeroTab(/? SingleSensorIsZeroTab_OLD13(/' 1 '? SingleSensorIsZeroTab('
subst "$OUT/xc_old13.inc" 's/(SingleSensorWarnOk())/(SingleSensorWarnOk_OLD13())/'     1 '(SingleSensorWarnOk())'
[ "$(grep -c '^}' "$OUT/xc_old13.inc")" -eq 3 ] || { echo "extract failed: xc_old13.inc"; exit 2; }

echo "extracted: NEW=working tree  OLD=$OLD_REV (helpers identical)  OLD13=$OLD13_REV  + SPEC / NOTRACK test mutants"

#--- captures -> raw scan files (count byte, event byte per 5 ms scan)
if [ -d "$CAPDIR" ]; then
    for c in scope-20261005 scope-rev scope-watch-20261005-1648-1710; do
        [ -f "$CAPDIR/$c.jsonl" ] || continue
        python3 - "$CAPDIR/$c.jsonl" "$OUT/$c.scan" <<'PY'
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
cnt = sum((r["data"]["count"] for r in rows), [])
ev  = sum((r["data"]["event"] for r in rows), [])
with open(sys.argv[2], "wb") as f:
    f.write((rows[0]["t"] - 1000).to_bytes(8, "little"))      # ms epoch of scan 0
    f.write(bytes(x for p in zip(cnt, ev) for x in p))
PY
    done
fi

mkdir -p "$OUT/stub/sys"
cat > "$OUT/stub/sys/io.h" <<'STUB'
#pragma once
static inline unsigned char  inb (unsigned short)                 { return 0; }
static inline unsigned short inw (unsigned short)                 { return 0; }
static inline unsigned int   inl (unsigned short)                 { return 0; }
static inline void           outb(unsigned char,  unsigned short) {}
static inline void           outw(unsigned short, unsigned short) {}
static inline void           outl(unsigned int,   unsigned short) {}
static inline int            iopl(int)                            { return -1; }
static inline int            ioperm(unsigned long, unsigned long, int) { return -1; }
STUB

g++ -std=gnu++17 -O2 -fpermissive -w ${EXTRA_CXXFLAGS:-} \
    -I"$OUT/stub" -I"$OUT" -I"$LP/overhead" -I"$LP/common" -I"$LP/interface" \
    "$HERE/sszero_crosscheck_test.cpp" -o "$OUT/t"
CAPS="$OUT" "$OUT/t" "$@" | tee "${LOG:-/dev/null}"
