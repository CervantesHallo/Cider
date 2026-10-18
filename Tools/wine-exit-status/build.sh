#!/bin/bash
# Compile PE collectors using Cider's cached toolchain or host MinGW cross compilers.
# An optional destination lets the App builder keep all outputs in its private stage.
set -euo pipefail
TASK_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TASK_TOOLCHAIN="${CIDER_TOOLCHAIN:-$HOME/Library/Caches/Cider/toolchain}"
TASK_OUT="${1:-$TASK_ROOT/out/wine-exit-status}"
mkdir -p "$TASK_OUT"
for TASK_ARCH in x86_64 i686; do
    TASK_CC="$TASK_TOOLCHAIN/mingw/toolchain-$TASK_ARCH/bin/$TASK_ARCH-w64-mingw32-gcc"
    if [ ! -x "$TASK_CC" ]; then
        TASK_CC="$(command -v "$TASK_ARCH-w64-mingw32-gcc" || true)"
    fi
    [ -n "$TASK_CC" ] && [ -x "$TASK_CC" ] || {
        echo "Missing MinGW cross compiler: $TASK_ARCH (use CIDER_TOOLCHAIN or install mingw-w64)" >&2
        exit 1
    }
    "$TASK_CC" -std=c11 -Wall -Wextra -Werror -O2 -municode -static-libgcc \
        -Wl,--no-insert-timestamp "$TASK_ROOT/Tools/wine-exit-status/capture.c" -o "$TASK_OUT/cider-exit-status-$TASK_ARCH.exe"
done
swift "$TASK_ROOT/Tools/wine-exit-status/package.swift" "$TASK_OUT" "$TASK_ROOT/Tools/wine-exit-status/capture.c"
