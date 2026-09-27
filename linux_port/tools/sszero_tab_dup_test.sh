#!/bin/bash
#--------------------------------------------------------------------------
#  sszero_tab_dup_test.sh - build + run sszero_tab_dup_test.cpp (15.7.14)
#
#  The test drives the REAL controller functions, so this script first cuts
#  them out of overhead.cpp:
#    OLD  ProcessSyncs() / GradeSyncs()  from git $OLD_REV (default f9f2968 =
#         15.7.13, the build deployed on the rig and the fleet) -> *_OLD
#    NEW  ProcessSyncs() / GradeSyncs()  from the working tree      -> *_NEW
#    SingleSensorIsZeroTab / SingleSensorWarnOk / RingSub from the working
#    tree (checked byte-identical to OLD: the detector is NOT changed).
#  Then compiles against the real controller headers and runs.
#
#  Usage (any x86_64 Linux host with g++, from anywhere):
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

git -C "$REPO" show "$OLD_REV:linux_port/overhead/overhead.cpp" > "$OUT/old.cpp"
NEWSRC="${NEWSRC:-$LP/overhead/overhead.cpp}"     # override to run a mutant

{
    extract "$OUT/old.cpp" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_OLD()/'
    extract "$OUT/old.cpp" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_OLD()/'
} > "$OUT/sszero_old.inc"
{
    extract "$NEWSRC" "void overhead::ProcessSyncs()" | sed 's/overhead::ProcessSyncs()/overhead::ProcessSyncs_NEW()/'
    extract "$NEWSRC" "void overhead::GradeSyncs()"   | sed 's/overhead::GradeSyncs()/overhead::GradeSyncs_NEW()/'
} > "$OUT/sszero_new.inc"

for f in "int overhead::RingSub(" "bool overhead::SingleSensorIsZeroTab(" "bool overhead::SingleSensorWarnOk()"; do
    extract "$NEWSRC"     "$f" > "$OUT/n.tmp"
    extract "$OUT/old.cpp" "$f" > "$OUT/o.tmp"
    [ -s "$OUT/n.tmp" ] || { echo "extract failed: $f"; exit 2; }
    cmp -s "$OUT/n.tmp" "$OUT/o.tmp" || { echo "UNEXPECTED: $f differs between $OLD_REV and the working tree"; exit 2; }
    cat "$OUT/n.tmp" >> "$OUT/sszero_shared.inc"
done

for f in sszero_old.inc sszero_new.inc; do
    [ "$(grep -c '^}' "$OUT/$f")" -eq 2 ] || { echo "extract failed: $f"; exit 2; }
done
echo "extracted: OLD=$OLD_REV ($(wc -l < "$OUT/sszero_old.inc") lines)  NEW=working tree ($(wc -l < "$OUT/sszero_new.inc") lines)"

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

g++ -std=gnu++17 -O2 -fpermissive -w \
    -I"$OUT/stub" -I"$OUT" -I"$LP/overhead" -I"$LP/common" -I"$LP/interface" \
    "$HERE/sszero_tab_dup_test.cpp" -o "$OUT/sszero_tab_dup_test"
"$OUT/sszero_tab_dup_test"
