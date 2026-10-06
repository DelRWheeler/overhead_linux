#!/bin/bash
#--------------------------------------------------------------------------
#  sszero_tab_dup_test.sh - build + run sszero_tab_dup_test.cpp (15.7.14, 15.7.15)
#
#  The test drives the REAL controller functions, so this script first cuts
#  them out of overhead.cpp:
#    OLD  ProcessSyncs() / GradeSyncs() / SingleSensorIsZeroTab() /
#         SingleSensorWarnOk()  from git $OLD_REV (default f9f2968 = 15.7.13,
#         the build deployed at Pitman / Holmes / Claxton L1)       -> *_OLD
#    NG   ProcessSyncs() / GradeSyncs() from the working tree with ONLY the
#         15.7.14 duplicate-pass guard switched off (3 lines, sed below) -> *_NG
#    NEW  ProcessSyncs() / GradeSyncs() from the working tree        -> *_NEW
#    SingleSensorIsZeroTab / SingleSensorTabRule / SingleSensorRevTrolleys /
#    SingleSensorRevGate / SingleSensorBootConfirm / SingleSensorTrolleyEdge /
#    SingleSensorAlarmTick / SingleSensorAlarmOk / RingSub from the working tree.
#  15.7.15 changed the detector and the alarm rule, so OLD and NEW legitimately
#  differ in single-sensor mode. The test therefore checks:
#    - standard mode: OLD (15.7.13) vs NEW, event-for-event identical;
#    - single-sensor: NG vs NEW, the guard removes exactly the duplicates.
#  Then compiles against the real controller headers and runs.
#
#  Usage (any Linux host with g++, from anywhere):
#      linux_port/tools/sszero_tab_dup_test.sh
#--------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
LP="$(cd "$HERE/.." && pwd)"                    # linux_port
REPO="$(cd "$LP/.." && pwd)"
OLD_REV="${OLD_REV:-f9f2968}"
OUT="${TMPDIR:-/tmp}/sszero_tab_dup_test.$$"
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT

# extract <file> <signature-prefix> : print the function that starts with the
# line beginning <signature-prefix> through its first column-0 closing brace.
extract() {
    tr -d '\r' < "$1" | awk -v sig="$2" '
        done { next }                              # read to EOF (no SIGPIPE under pipefail)
        !on && index($0, sig) == 1 { on = 1 }
        on { print; if ($0 ~ /^}/) done = 1 }'
}
# subst <file> <sed-expr> <expected-count> : apply, insisting it matched exactly that often
subst() {
    local n; n=$(grep -cF "$4" "$1" || true)
    [ "$n" -eq "$3" ] || { echo "sed anchor '$4' matched $n times in $1, expected $3"; exit 2; }
    sed -i "$2" "$1"
}

tr -d '\r' < <(git -C "$REPO" show "$OLD_REV:linux_port/overhead/overhead.cpp") > "$OUT/old.cpp"
NEWSRC="${NEWSRC:-$LP/overhead/overhead.cpp}"     # override to run a mutant

#--- OLD: 15.7.13 as deployed, with its own detector + global 30 s throttle
{
    extract "$OUT/old.cpp" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_OLD()/'
    extract "$OUT/old.cpp" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_OLD()/'
    extract "$OUT/old.cpp" "bool overhead::SingleSensorIsZeroTab(" | sed 's/overhead::SingleSensorIsZeroTab(/overhead::SingleSensorIsZeroTab_OLD(/'
    extract "$OUT/old.cpp" "bool overhead::SingleSensorWarnOk()"   | sed 's/overhead::SingleSensorWarnOk()/overhead::SingleSensorWarnOk_OLD()/'
} > "$OUT/sszero_old.inc"
subst "$OUT/sszero_old.inc" 's/? SingleSensorIsZeroTab(/? SingleSensorIsZeroTab_OLD(/' 3 '? SingleSensorIsZeroTab('
subst "$OUT/sszero_old.inc" 's/(SingleSensorWarnOk())/(SingleSensorWarnOk_OLD())/'     3 '(SingleSensorWarnOk())'

#--- NEW and NG (= NEW minus the 15.7.14 duplicate-pass guard)
{
    extract "$NEWSRC" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_NEW()/'
    extract "$NEWSRC" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_NEW()/'
} > "$OUT/sszero_new.inc"
{
    extract "$NEWSRC" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_NG()/'
    extract "$NEWSRC" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_NG()/'
} > "$OUT/sszero_ng.inc"
subst "$OUT/sszero_ng.inc" 's/ssTabDup = ss_ovr_pass\[i\];/ssTabDup = false;/'                                 1 'ssTabDup = ss_ovr_pass[i];'
subst "$OUT/sszero_ng.inc" 's/ssGradeTabDup = ss_grade_ovr_pass\[GradeSyncIndex\];/ssGradeTabDup = false;/'   2 'ssGradeTabDup = ss_grade_ovr_pass[GradeSyncIndex];'
subst "$OUT/sszero_ng.inc" 's/if (!ssTab)$/if (true)/'                                                         1 'if (!ssTab)'

: > "$OUT/sszero_shared.inc"
for f in "int overhead::RingSub(" "bool overhead::SingleSensorIsZeroTab(" "bool overhead::SingleSensorTabRule(" \
         "int overhead::SingleSensorRevTrolleys()" "int overhead::SingleSensorRevGate()" \
         "bool overhead::SingleSensorBootConfirm(" "bool overhead::SingleSensorBootPending(" \
         "void overhead::SingleSensorTrolleyEdge(" \
         "void overhead::SingleSensorAlarmTick(" "bool overhead::SingleSensorAlarmOk("; do
    extract "$NEWSRC" "$f" > "$OUT/n.tmp"
    [ -s "$OUT/n.tmp" ] || { echo "extract failed: $f"; exit 2; }
    cat "$OUT/n.tmp" >> "$OUT/sszero_shared.inc"
done
# 15.7.16: the cross-sync count check the working tree's ProcessSyncs calls (if present)
if grep -q 'SingleSensorXCCheck' "$NEWSRC"; then
    tr -d '\r' < "$NEWSRC" | grep -E '^static inline (double|int) +ssxc_(abs|round)\(' >> "$OUT/sszero_shared.inc"
    for f in "double overhead::SingleSensorXCWrap(" "double overhead::SingleSensorXCPos(" "bool overhead::SingleSensorXCEligible(" \
             "bool overhead::SingleSensorXCSteady(" "double overhead::SingleSensorXCOffset(" "void overhead::SingleSensorXCGapEdge(" \
             "void overhead::SingleSensorXCDrop(" "void overhead::SingleSensorXCZero(" "void overhead::SingleSensorXCOverrun(" \
             "int overhead::SingleSensorXCCheck(" "bool overhead::SingleSensorXCApply("; do
        extract "$NEWSRC" "$f" > "$OUT/n.tmp"
        [ -s "$OUT/n.tmp" ] || { echo "extract failed: $f"; exit 2; }
        cat "$OUT/n.tmp" >> "$OUT/sszero_shared.inc"
    done
fi
extract "$OUT/old.cpp" "int overhead::RingSub(" > "$OUT/o.tmp"
extract "$NEWSRC"      "int overhead::RingSub(" > "$OUT/n.tmp"
cmp -s "$OUT/n.tmp" "$OUT/o.tmp" || { echo "UNEXPECTED: RingSub differs between $OLD_REV and the working tree"; exit 2; }

[ "$(grep -c '^}' "$OUT/sszero_old.inc")" -eq 4 ] || { echo "extract failed: sszero_old.inc"; exit 2; }
for f in sszero_new.inc sszero_ng.inc; do
    [ "$(grep -c '^}' "$OUT/$f")" -eq 2 ] || { echo "extract failed: $f"; exit 2; }
done
echo "extracted: OLD=$OLD_REV ($(wc -l < "$OUT/sszero_old.inc") lines)  NEW=working tree ($(wc -l < "$OUT/sszero_new.inc") lines)  NG=NEW minus dup guard"

# The test never does port I/O. A stub <sys/io.h> lets it build and run on any
# host (the dev VM is aarch64; the controller itself is cross-built for x86_64).
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
    "$HERE/sszero_tab_dup_test.cpp" -o "$OUT/sszero_tab_dup_test"
"$OUT/sszero_tab_dup_test"
