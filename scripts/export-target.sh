#!/usr/bin/env bash
set -euo pipefail

ROOT="${ROOT:-$HOME/mk8-recomp}"
ROM="${ROM:-$HOME/roms/target-update.nsp}"
OUT="${OUT:-$ROOT/generated/target}"
FMT="${FMT:-source}"
LOG="${LOG:-/tmp/export-drive.log}"

[ -f "$ROM" ] || { echo "no such rom: $ROM" >&2; exit 1; }

setsid python3 "$ROOT/scripts/export-recomp.py" \
    --rom "$ROM" --out "$OUT" "$FMT" > "$LOG" 2>&1 < /dev/null &

sleep 20
cat "$LOG"
