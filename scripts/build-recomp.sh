#!/usr/bin/env bash
# Build one generated recompiled module into a shared library.
#
# The PowerShell version builds recompiled_<module>.dll with MSVC; this builds
# librecompiled_<module>.so with GCC. Same generated CMake project, same target
# names - suyu resolves recomp_image_lookup / recomp_image_set_base out of
# whatever the platform's shared library happens to be called.
#
#   ./scripts/build-recomp.sh main
#   ./scripts/build-recomp.sh --target <target> --package "<package dir>" rtld
set -euo pipefail

ROOT="${ROOT:-$HOME/mk8-recomp}"
TARGET="${TARGET:-${MK8R_TARGET:-}}"
PACKAGE="${PACKAGE:-${MK8R_PACKAGE:-}}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

MODULE=""
while [ $# -gt 0 ]; do
    case "$1" in
        --target)  TARGET="$2"; shift 2 ;;
        --package) PACKAGE="$2"; shift 2 ;;
        *)         MODULE="$1"; shift ;;
    esac
done
[ -n "$MODULE" ] || { echo "usage: $0 [--target T] [--package P] <module>" >&2; exit 2; }

SRC="$ROOT/generated/$TARGET/$PACKAGE/aot_cache/exefs/$MODULE"
# Namespaced by target: one game's build tree being reused for another's sources
# produces a library that loads and then executes the wrong game's code, which is
# worse than not building at all.
BUILD="$ROOT/build/recomp/$TARGET/$MODULE"

[ -d "$SRC" ] || { echo "no generated project at $SRC" >&2; exit 1; }

CMAKE="${CMAKE:-$(command -v cmake)}"
[ -x "$HOME/.local/bin/cmake" ] && CMAKE="$HOME/.local/bin/cmake"

# The main image is ~290 MB of C in one directory and GCC needs far more memory
# per translation unit than MSVC. Cap by memory, not cores, or the OOM killer
# takes the build somewhere in the middle.
mem_gb=$(awk '/MemTotal/ {printf "%d", $2/1048576}' /proc/meminfo)
cores=$(nproc)
jobs=$(( mem_gb / 3 ))
[ "$jobs" -lt 1 ] && jobs=1
[ "$jobs" -gt "$cores" ] && jobs="$cores"

src_mb=$(du -sm "$SRC/src" 2>/dev/null | cut -f1)
echo "=== module $MODULE: ${src_mb} MB of generated C, -j${jobs} ==="

# The main NSO's shared target is recompiled_image, not recompiled_main - that is
# the name suyu's loader scans for.
if [ "$MODULE" = "main" ]; then
    CMAKE_TARGET="recompiled_image"
else
    CMAKE_TARGET="recompiled_$MODULE"
fi

"$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
start=$(date +%s)
"$CMAKE" --build "$BUILD" --target "$CMAKE_TARGET" -- -j"$jobs"
echo "=== built in $(( ($(date +%s) - start) / 60 )) min ==="
find "$BUILD" -name '*.so' -printf '%f  %s bytes\n'
