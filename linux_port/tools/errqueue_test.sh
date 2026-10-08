#!/bin/bash
#--------------------------------------------------------------------------
#  errqueue_test.sh - build + run errqueue_test.cpp (15.7.16)
#
#  Cuts the REAL GenError / SendError / SendErrorMsg / ErrQueuePending out of the working tree's
#  overhead.cpp, and GenError / SendError out of git $OLD_REV (default b269cf8 = 15.7.15) as
#  *_OLD, and compiles errqueue_test.cpp against the controller headers and the real platform
#  mutex (RtCreateMutex / RtWaitForSingleObject, pthreads).
#
#  Usage:   linux_port/tools/errqueue_test.sh
#--------------------------------------------------------------------------
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LP="$(cd "$HERE/.." && pwd)"
REPO="$(cd "$LP/.." && pwd)"
OLD_REV="${OLD_REV:-b269cf8}"
OUT="${TMPDIR:-/tmp}/errqueue_test.$$"
mkdir -p "$OUT"
[ -n "${KEEP:-}" ] || trap 'rm -rf "$OUT"' EXIT
extract() {
    tr -d '\r' < "$1" | awk -v sig="$2" '
        done { next }
        !on && index($0, sig) == 1 { on = 1 }
        on { print; if ($0 ~ /^}/) done = 1 }'
}
NEWSRC="${NEWSRC:-$LP/overhead/overhead.cpp}"
tr -d '\r' < <(git -C "$REPO" show "$OLD_REV:linux_port/overhead/overhead.cpp") > "$OUT/old.cpp"
: > "$OUT/errq.inc"
for f in "void overhead::GenError(int sev, char* txt)" "void overhead::SendError()" "bool overhead::SendErrorMsg(" "bool overhead::ErrQueuePending()"; do
    extract "$NEWSRC" "$f" > "$OUT/n.tmp"; [ -s "$OUT/n.tmp" ] || { echo "extract failed: $f"; exit 2; }
    cat "$OUT/n.tmp" >> "$OUT/errq.inc"
done
extract "$OUT/old.cpp" "void overhead::GenError(int sev, char* txt)" | sed 's/overhead::GenError(/overhead::GenError_OLD(/' >> "$OUT/errq.inc"
extract "$OUT/old.cpp" "void overhead::SendError()" | sed 's/overhead::SendError()/overhead::SendError_OLD()/' >> "$OUT/errq.inc"
[ "$(grep -c '^}' "$OUT/errq.inc")" -eq 6 ] || { echo "extract failed: errq.inc"; exit 2; }
grep -q "errq_head" <(extract "$OUT/old.cpp" "void overhead::GenError(int sev, char* txt)") && { echo "UNEXPECTED: $OLD_REV already queues"; exit 2; }
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
g++ -std=gnu++17 -O2 -fpermissive -w -pthread ${EXTRA_CXXFLAGS:-} \
    -I"$OUT/stub" -I"$OUT" -I"$LP/overhead" -I"$LP/common" -I"$LP/interface" \
    "$HERE/errqueue_test.cpp" -o "$OUT/t"
"$OUT/t"
