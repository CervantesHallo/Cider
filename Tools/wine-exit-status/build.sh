#!/bin/bash
# Compile candidate PE collectors with the existing Cider toolchain; no downloads or execution.
set -euo pipefail
TASK_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TASK_TOOLCHAIN="${CIDER_TOOLCHAIN:-$HOME/Library/Caches/Cider/toolchain}"
TASK_OUT="$TASK_ROOT/out/wine-exit-status"
mkdir -p "$TASK_OUT"
for TASK_ARCH in x86_64 i686; do
    TASK_CC="$TASK_TOOLCHAIN/mingw/toolchain-$TASK_ARCH/bin/$TASK_ARCH-w64-mingw32-gcc"
    [ -x "$TASK_CC" ] || { echo "Existing compiler missing: $TASK_ARCH" >&2; exit 1; }
    "$TASK_CC" -std=c11 -Wall -Wextra -Werror -O2 -municode -static-libgcc \
        "$TASK_ROOT/Tools/wine-exit-status/capture.c" -o "$TASK_OUT/cider-exit-status-$TASK_ARCH.exe"
done
