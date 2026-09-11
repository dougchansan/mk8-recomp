#!/usr/bin/env bash
# Rebuild suyu, re-export the target, rebuild every module.
#
# The manifest delete is not optional: a surviving aot_manifest.json is a cache
# hit and the "re-export" regenerates nothing, so the run measures the old code.
set -euo pipefail

ROOT="${ROOT:-$HOME/mk8-recomp}"
TARGET="${TARGET:-${MK8R_TARGET:-}}"
PACKAGE="${PACKAGE:-${MK8R_PACKAGE:-}}"
GEN="$ROOT/generated/$TARGET"

echo '=== 1/4 suyu ==='
cmake --build "$ROOT/build/suyu" --target suyu -- -j"$(nproc)" 2>&1 | tail -3

echo '=== 2/4 clearing previous export ==='
rm -rf "$GEN"
echo "removed $GEN"

echo '=== 3/4 export ==='
"$ROOT/scripts/run-gui.sh" >/dev/null
"$ROOT/scripts/export-target.sh" | tail -5
until [ -f "$GEN/$PACKAGE/aot_cache/aot_manifest.json" ]; do sleep 10; done
# The manifest is written before the last module's sources settle.
sleep 30
pkill -x suyu 2>/dev/null || true
sleep 3
grep -E 'AOT coverage \[[a-z0-9]+\]: [0-9]' "$HOME/.local/share/suyu/log/suyu_log.txt" | tail -5

echo '=== 4/4 modules ==='
for m in main sdk subsdk0 rtld; do
    "$ROOT/scripts/build-recomp.sh" --package "$PACKAGE" "$m" 2>&1 | tail -2
done
