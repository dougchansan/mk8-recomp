#!/usr/bin/env bash
set -euo pipefail

ROOT="${ROOT:-${MK8R_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)}}"
ROM="${ROM:-${MK8R_ROM:-}}"
OUT="${OUT:-$ROOT/generated/${MK8R_TARGET:-target}}"
FMT="${FMT:-source}"
LOG="${LOG:-/tmp/export-drive.log}"

[ -n "$ROM" ] || { echo "set MK8R_ROM or ROM" >&2; exit 1; }
[ -f "$ROM" ] || { echo "no such rom: $ROM" >&2; exit 1; }

setsid python3 "$ROOT/scripts/export-recomp.py" \
    --rom "$ROM" --out "$OUT" "$FMT" > "$LOG" 2>&1 < /dev/null &

sleep 20
cat "$LOG"
