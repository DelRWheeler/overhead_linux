#!/bin/bash
#--------------------------------------------------------------------------
#  sszero_window_test.sh - build + run sszero_window_test.cpp (15.7.15)
#
#  Drives the REAL single-sensor zero code over synthetic 5 ms scan streams of a
#  moving chain (speed steps, ramps, stops, jitter, noise, missing flags):
#    NEW  ProcessSyncs() / GradeSyncs() / SingleSensorIsZeroTab() and its helpers
#         (tab rule, revolution gate, boot confirmation, alarm gate) from the working tree
#    OLD  ProcessSyncs() / GradeSyncs() / SingleSensorIsZeroTab() / SingleSensorWarnOk()
#         from git $OLD_REV (default f9f2968 = 15.7.13, the build Pitman runs), for the
#         side-by-side columns.
#
#  Usage:   linux_port/tools/sszero_window_test.sh [group ...]
#           (no group = all; JOBS=n parallel groups, default 8;
#            EXTRA_CXXFLAGS=-DSS_BOOT_CONFIRM=0 to test with boot confirmation off;
#            LOG=<file> to keep the full output)
#--------------------------------------------------------------------------
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
LP="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$LP/.." && pwd)"
OLD_REV="${OLD_REV:-f9f2968}"
OUT="${TMPDIR:-/tmp}/sszero_window_test.$$"
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

tr -d '\r' < <(git -C "$REPO" show "$OLD_REV:linux_port/overhead/overhead.cpp") > "$OUT/old.cpp"
NEWSRC="${NEWSRC:-$LP/overhead/overhead.cpp}"

{
    extract "$OUT/old.cpp" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_OLD()/'
    extract "$OUT/old.cpp" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_OLD()/'
    extract "$OUT/old.cpp" "bool overhead::SingleSensorIsZeroTab(" | sed 's/overhead::SingleSensorIsZeroTab(/overhead::SingleSensorIsZeroTab_OLD(/'
    extract "$OUT/old.cpp" "bool overhead::SingleSensorWarnOk()"   | sed 's/overhead::SingleSensorWarnOk()/overhead::SingleSensorWarnOk_OLD()/'
} > "$OUT/sszero_old.inc"
subst "$OUT/sszero_old.inc" 's/? SingleSensorIsZeroTab(/? SingleSensorIsZeroTab_OLD(/' 3 '? SingleSensorIsZeroTab('
subst "$OUT/sszero_old.inc" 's/(SingleSensorWarnOk())/(SingleSensorWarnOk_OLD())/'     3 '(SingleSensorWarnOk())'
{
    extract "$NEWSRC" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_NEW()/'
    extract "$NEWSRC" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_NEW()/'
} > "$OUT/sszero_new.inc"
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
[ "$(grep -c '^}' "$OUT/sszero_old.inc")" -eq 4 ] || { echo "extract failed: sszero_old.inc"; exit 2; }
[ "$(grep -c '^}' "$OUT/sszero_new.inc")" -eq 2 ] || { echo "extract failed: sszero_new.inc"; exit 2; }

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
    "$HERE/sszero_window_test.cpp" -o "$OUT/t"

SEL_ALL="pitman_steady pitman_steps pitman_steppos pitman_stop chicken alarms boot anyplant_3x"
for s in 0 1; do for n in 100 304 600 1189 1500; do SEL_ALL="$SEL_ALL anyplant_s${s}_$n"; done; done
SEL="${*:-$SEL_ALL}"

i=0
for g in $SEL; do i=$((i+1)); printf '%02d %s\n' "$i" "$g"; done > "$OUT/groups"
# run groups in parallel; each writes its own log, then print in order
xargs -P "${JOBS:-8}" -L 1 sh -c '"$0/t" "$2" > "$0/log.$1" 2>&1; echo $? > "$0/rc.$1"' "$OUT" < "$OUT/groups"
{
    while read -r n g; do
        [ "$n" = "01" ] && head -1 "$OUT/log.$n"
        sed -n '2,${/^== Totals/q;p;}' "$OUT/log.$n"
    done < "$OUT/groups"
    echo; echo "== Totals (NEW, all syncs, summed over groups)"
    cat "$OUT"/log.* | grep -E '^  [a-z0-9_]+ +runs ' | awk '
        { k = $1; runs[k] += $3; tabs[k] += $5; miss[k] += $7; fz[k] += $9; sil[k] += $11; al[k] += $13;
          if ($16 > mpr[k]) mpr[k] = $16; viol[k] += $19 }
        END { for (k in runs) printf "  %-20s runs %4d  tabs %6d  missed %4d  false %4d  silent %3d  alarms %4d  max alarms/rev/sync %d  rule viol %d\n",
                                     k, runs[k], tabs[k], miss[k], fz[k], sil[k], al[k], mpr[k], viol[k] }' | sort
    echo
    awk '/ passed, .* failed/ { p += $1; f += $3 } END { printf "%d passed, %d failed\n", p, f }' "$OUT"/log.*
} | tee "${LOG:-/dev/null}"
fail=0
for f in "$OUT"/rc.*; do [ "$(cat "$f")" = "0" ] || fail=1; done
exit $fail
