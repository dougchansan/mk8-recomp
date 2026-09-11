#!/usr/bin/env bash
# Build the suyu fork on Linux.
#
# The PowerShell script alongside this one is MSVC-only: it shells out to
# vcvars64.bat, points at a downloaded Qt, and passes /MAP. None of that
# translates, so this is a separate script rather than a cross-platform one.
#
#   ./scripts/build-suyu.sh              # configure if needed, then build
#   ./scripts/build-suyu.sh --configure  # force a reconfigure
#   ./scripts/build-suyu.sh --clean      # start from scratch
set -euo pipefail

ROOT="${ROOT:-${MK8R_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}}"
BUILD="$ROOT/build/suyu"
SRC="$ROOT/third_party/suyu"
BUILD_TYPE="${BUILD_TYPE:-Release}"

configure=0
for arg in "$@"; do
    case "$arg" in
        --configure) configure=1 ;;
        --clean)     rm -rf "$BUILD"; configure=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

[ -d "$SRC" ] || { echo "no suyu checkout at $SRC" >&2; exit 1; }
mkdir -p "$BUILD"

# suyu's CPMUtil.cmake requires CMake 3.31, and Ubuntu 24.04 ships 3.28. Prefer
# a newer one unpacked under ~/.local rather than replacing the system package.
CMAKE="${CMAKE:-$(command -v cmake)}"
if [ -x "$HOME/.local/bin/cmake" ]; then
    CMAKE="$HOME/.local/bin/cmake"
fi
cmake_ver=$("$CMAKE" --version | head -1 | awk '{print $3}')
echo "=== cmake $cmake_ver ($CMAKE) ==="
if [ "$(printf '%s\n3.31.0\n' "$cmake_ver" | sort -V | head -1)" != "3.31.0" ]; then
    cat >&2 <<'EOF'
cmake 3.31 or newer is required (CMakeModules/CPMUtil.cmake). Install one
without disturbing the system package:

  V=3.31.6
  curl -fsSL -o /tmp/cmake.tar.gz \
    "https://github.com/Kitware/CMake/releases/download/v${V}/cmake-${V}-linux-x86_64.tar.gz"
  mkdir -p ~/.local/opt && tar xzf /tmp/cmake.tar.gz -C ~/.local/opt
  ln -sfn ~/.local/opt/cmake-${V}-linux-x86_64 ~/.local/opt/cmake
  mkdir -p ~/.local/bin && ln -sf ~/.local/opt/cmake/bin/cmake ~/.local/bin/cmake
EOF
    exit 1
fi

# The generated recompiler modules are enormous - the main image is ~290 MB of C
# in one directory - and GCC needs far more memory per translation unit than
# MSVC does. On a 24-thread box with 23 GB that is enough to invoke the OOM
# killer partway through a link, so cap parallelism by available memory rather
# than by core count.
mem_gb=$(awk '/MemTotal/ {printf "%d", $2/1048576}' /proc/meminfo)
cores=$(nproc)
jobs=$(( mem_gb / 2 ))
[ "$jobs" -lt 1 ] && jobs=1
[ "$jobs" -gt "$cores" ] && jobs="$cores"
echo "=== ${mem_gb} GB / ${cores} threads -> -j${jobs} ==="

if [ "$configure" = 1 ] || [ ! -f "$BUILD/build.ninja" ]; then
    echo '=== configure ==='
    "$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DENABLE_QT=ON \
        -DYUZU_USE_BUNDLED_QT=OFF \
        -DYUZU_CMD=ON \
        -DYUZU_TESTS=OFF \
        -DENABLE_WEB_SERVICE=OFF \
        -DYUZU_ROOM=OFF \
        -DYUZU_ROOM_STANDALONE=OFF \
        -DENABLE_QT_TRANSLATION=OFF \
        -DUSE_DISCORD_PRESENCE=OFF \
        `# Ubuntu ships fmt 9; suyu's logging.h calls format_string::get(),` \
        `# which only exists from fmt 10. CPMUtil prefers a system package when` \
        `# it finds one, and suyu only sets fmt_FORCE_BUNDLED inside a branch` \
        `# that does not apply to a normal Linux build - so without this the` \
        `# build dies ~450 files in with "has no member named get".` \
        -Dfmt_FORCE_BUNDLED=ON
fi

echo '=== build: suyu suyu-cmd ==='
start=$(date +%s)
"$CMAKE" --build "$BUILD" --target suyu suyu-cmd -- -j"$jobs"
echo "=== built in $(( ($(date +%s) - start) / 60 )) min ==="
ls -la "$BUILD/bin/" 2>/dev/null | head -5
