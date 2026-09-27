#!/bin/bash
# ADR-005 gate (docs/plan/02 P-1): cold-start the Steam UI N times in a bottle and check that its main window
# renders every time (a black or blank CEF window compresses to a tiny PNG).
#   scripts/steam-coldstart.sh <bottle> [runs=20]
set -uo pipefail
BOTTLE=$1; RUNS=${2:-20}
REPO=$(cd "$(dirname "$0")/.." && pwd)
CTL="$REPO/Tools/ciderctl/.build/debug/ciderctl"
STEAM='C:\Program Files (x86)\Steam\steam.exe'
OUT=$(mktemp -d /tmp/steam-coldstart.XXXXXX)
swiftc -O -o "$OUT/wins" "$REPO/scripts/wine-windows.swift" 2>/dev/null
pass=0; times=()
steam_running() { "$CTL" ps | grep -q 'steam.exe'; }
for i in $(seq 1 "$RUNS"); do
    "$CTL" run -b "$BOTTLE" "$STEAM" -shutdown >/dev/null 2>&1
    for _ in $(seq 1 30); do steam_running || break; sleep 1; done
    "$CTL" kill -b "$BOTTLE" >/dev/null 2>&1; sleep 3
    start=$(date +%s)
    "$CTL" run -b "$BOTTLE" --env DXMT_ALLOW_CROSS_PROCESS_SWAPCHAIN=1 "$STEAM" >/dev/null 2>&1
    ok=0
    for _ in $(seq 1 120); do
        sleep 1
        id=$("$OUT/wins" | awk -F'\t' '$3 == "Steam" && $4 >= 800 && $5 >= 500 {print $1; exit}')
        [ -n "$id" ] || continue
        sleep 8   # let the store/library page paint
        screencapture -x -o -l "$id" "$OUT/run$i.png" 2>/dev/null
        size=$(stat -f %z "$OUT/run$i.png" 2>/dev/null || echo 0)
        if [ "$size" -gt 150000 ]; then ok=1; fi
        break
    done
    elapsed=$(( $(date +%s) - start ))
    if [ $ok -eq 1 ]; then pass=$((pass + 1)); times+=("$elapsed"); echo "run $i: ok (${elapsed}s)"; else echo "run $i: FAIL (${elapsed}s) → $OUT/run$i.png"; fi
done
avg=$( [ ${#times[@]} -gt 0 ] && echo $(( $(IFS=+; echo "$((${times[*]}))") / ${#times[@]} )) || echo "-" )
echo "passed $pass/$RUNS, mean time to a rendered window ${avg}s (screenshots in $OUT)"
[ "$pass" -eq "$RUNS" ]
