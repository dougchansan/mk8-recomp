#!/usr/bin/env bash
# Rebuild suyu, re-export the target, rebuild every module.
#
# The manifest delete is not optional: a surviving aot_manifest.json is a cache
# hit and the "re-export" regenerates nothing, so the run measures the old code.
set -euo pipefail

ROOT="${ROOT:-${MK8R_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)}}"
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
# The ROM to export is not the ROM to boot: a cartridge can carry a base built
# for a different architecture than the update that actually runs, so exporting
# from the boot image silently produces the wrong module set.
ROM="${MK8R_EXPORT_ROM:-${MK8R_ROM:-}}" "$ROOT/scripts/export-target.sh" | tail -5
until [ -f "$GEN/$PACKAGE/aot_cache/aot_manifest.json" ]; do sleep 10; done
# The manifest is written before the last module's sources settle.
sleep 30
pkill -x suyu 2>/dev/null || true
sleep 3
# A grep that matches nothing exits 1 and, under pipefail, would take the whole
# script with it - silently skipping the module build and leaving stale .so files
# for the next benchmark to measure.
grep -E 'AOT coverage \[[a-z0-9]+\]: [0-9]' "$HOME/.local/share/suyu/log/suyu_log.txt" | tail -5 || true

echo '=== 4/4 modules ==='
for m in main sdk subsdk0 rtld; do
    "$ROOT/scripts/build-recomp.sh" --package "$PACKAGE" "$m" 2>&1 | tail -2
done

# Refuse to hand a stale build to a benchmark. Every measurement that has had to
# be thrown away in this project was a build that silently did not happen.
emitter="$ROOT/third_party/suyu/src/core/recompiler/arm64_to_c.h"
stale=0
for so in $(find "$ROOT/build/recomp/$TARGET" -name '*.so'); do
    if [ "$emitter" -nt "$so" ]; then
        echo "STALE: $so is older than the emitter" >&2
        stale=1
    fi
done
[ "$stale" = 0 ] || { echo 'refusing to report success with stale modules' >&2; exit 1; }
echo 'all modules newer than the emitter'
