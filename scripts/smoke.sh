#!/bin/bash
# Engine smoke test (docs/plan/00 ADR-011 gates): in a throwaway CIDER_HOME, install an engine directory,
# create a bottle, and check that 64-bit and 32-bit (WoW64) programs run and that nothing loads from
# /usr/local or /opt/homebrew.
#
#   scripts/smoke.sh out/engines/<id>
set -euo pipefail
ENGINE_DIR=$(cd "$1" && pwd)
REPO=$(cd "$(dirname "$0")/.." && pwd)
CTL="$REPO/Tools/ciderctl/.build/debug/ciderctl"
[ -x "$CTL" ] || (cd "$REPO/Tools/ciderctl" && swift build >/dev/null)
export CIDER_HOME=$(mktemp -d /tmp/cider-smoke.XXXXXX)
trap 'rm -rf "$CIDER_HOME"' EXIT
pass=0; fail=0
check() { if "$@"; then echo "  ✔ $CHECK"; pass=$((pass + 1)); else echo "  ✘ $CHECK"; fail=$((fail + 1)); fi; }

echo "engine: $ENGINE_DIR"
CHECK="engine installs"; check "$CTL" engine install "$ENGINE_DIR" >/dev/null
CHECK="bottle creates (wineboot --init)"; check "$CTL" bottle create Smoke --locale zh-Hans >/dev/null
out64=$("$CTL" run -b Smoke --wait --debug quiet cmd /c ver 2>/dev/null || true)
log64=$(ls -t "$CIDER_HOME"/Logs/sessions/*/*/wine.log 2>/dev/null | head -1)
CHECK="cmd /c ver (64-bit)"; check grep -q "Microsoft Windows" "$log64"
"$CTL" run -b Smoke --wait --debug quiet 'C:\windows\syswow64\cmd.exe' /c ver >/dev/null 2>&1 || true
log32=$(ls -t "$CIDER_HOME"/Logs/sessions/*/*/wine.log 2>/dev/null | head -1)
CHECK="syswow64\\cmd /c ver (32-bit)"; check grep -q "Microsoft Windows" "$log32"
wine=$(find "$CIDER_HOME/AppSupport/Engines" -path "*/bin/wine" -type f | head -1)
libs=$(DYLD_PRINT_LIBRARIES=1 WINEPREFIX="$CIDER_HOME/none" "$wine" --version 2>&1 || true)
CHECK="no /usr/local or /opt/homebrew libraries"; check bash -c "! grep -qE '/usr/local|/opt/homebrew' <<<\"\$0\"" "$libs"
echo "passed $pass, failed $fail"
[ "$fail" -eq 0 ]
