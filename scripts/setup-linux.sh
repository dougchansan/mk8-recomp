#!/usr/bin/env bash
# One-shot setup for Linux: install dependencies, clone, build.
#
# Works on any distro with a recognised package manager (apt, dnf/yum, pacman,
# zypper, apk, xbps, eopkg). Standalone - it can be piped straight from the web
# before the repository exists:
#
#   curl -fsSL .../scripts/setup-linux.sh | bash
#
# Run it from inside an existing clone and it builds that clone in place.
#
#   --deps-only   install dependencies and stop
#   --no-build    install dependencies and clone, but do not build
#   --dry-run     print what would be installed instead of installing it
#
# Does NOT fetch a game, keys, or firmware. Those are yours to supply.
set -euo pipefail

REPO_URL="${MK8R_REPO:-https://github.com/dougchansan/mk8-recomp}"
CMAKE_MIN=3.31.0
CMAKE_GET=3.31.6

deps_only=0; no_build=0; dry_run=0
for arg in "$@"; do
    case "$arg" in
        --deps-only) deps_only=1 ;;
        --no-build)  no_build=1 ;;
        --dry-run)   dry_run=1 ;;
        -h|--help)   sed -n '2,16p' "$0" | cut -c3-; exit 0 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

say()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarn\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31merror\033[0m %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = Linux ] || die "this script is for Linux; on Windows use scripts/setup-windows.ps1"

# --- privilege --------------------------------------------------------------
# Containers and minimal images often run as root with no sudo installed.
SUDO=''
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null; then SUDO=sudo; else die "not root and sudo not found; re-run as root"; fi
fi

# --- package manager --------------------------------------------------------
# ID_LIKE from os-release is deliberately not consulted: the manager binary is
# what decides the package names, and derivatives keep their parent's.
if   command -v apt-get >/dev/null; then PM=apt
elif command -v dnf     >/dev/null; then PM=dnf
elif command -v yum     >/dev/null; then PM=yum
elif command -v pacman  >/dev/null; then PM=pacman
elif command -v zypper  >/dev/null; then PM=zypper
elif command -v apk     >/dev/null; then PM=apk
elif command -v xbps-install >/dev/null; then PM=xbps
elif command -v eopkg   >/dev/null; then PM=eopkg
else PM=unknown
fi

# Every list below is the same set under each distro's naming: a C++ toolchain,
# Ninja, nasm, Python, glslang, Qt 6 (base plus private headers, svg, charts,
# multimedia, opengl), Boost, libusb, OpenSSL, FFmpeg, zstd and lz4.
case "$PM" in
apt)
    PKGS="build-essential pkg-config git curl ca-certificates cmake ninja-build nasm autoconf python3 python3-pip glslang-tools
          qt6-base-dev qt6-base-private-dev libqt6svg6-dev libqt6charts6-dev qt6-multimedia-dev libqt6opengl6-dev
          libboost-dev libboost-filesystem-dev libboost-system-dev libboost-context-dev
          libusb-1.0-0-dev libssl-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
          libzstd-dev liblz4-dev libgl1-mesa-dev" ;;
dnf|yum)
    PKGS="gcc gcc-c++ make pkgconf-pkg-config git curl cmake ninja-build nasm autoconf python3 python3-pip glslang
          qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtsvg-devel qt6-qtcharts-devel qt6-qtmultimedia-devel
          boost-devel libusb1-devel openssl-devel
          ffmpeg-free-devel libzstd-devel lz4-devel mesa-libGL-devel" ;;
pacman)
    PKGS="base-devel pkgconf git curl cmake ninja nasm autoconf python python-pip glslang
          qt6-base qt6-svg qt6-charts qt6-multimedia
          boost libusb openssl ffmpeg zstd lz4" ;;
zypper)
    PKGS="gcc gcc-c++ make pkg-config git curl cmake ninja nasm autoconf python3 python3-pip glslang
          qt6-base-devel qt6-base-private-devel qt6-svg-devel qt6-charts-devel qt6-multimedia-devel
          libboost_headers-devel libboost_filesystem-devel libboost_system-devel libboost_context-devel
          libusb-1_0-devel libopenssl-devel
          libavcodec-devel libavformat-devel libavutil-devel libswscale-devel
          libzstd-devel liblz4-devel Mesa-libGL-devel" ;;
apk)
    PKGS="build-base pkgconf git curl cmake samurai nasm autoconf python3 py3-pip glslang-dev
          qt6-qtbase-dev qt6-qtsvg-dev qt6-qtcharts-dev qt6-qtmultimedia-dev
          boost-dev libusb-dev openssl-dev ffmpeg-dev zstd-dev lz4-dev mesa-dev" ;;
xbps)
    PKGS="base-devel pkg-config git curl cmake ninja nasm autoconf python3 glslang
          qt6-base-devel qt6-svg-devel qt6-charts-devel qt6-multimedia-devel
          boost-devel libusb-devel openssl-devel ffmpeg-devel libzstd-devel liblz4-devel MesaLib-devel" ;;
eopkg)
    PKGS="-c system.devel git curl cmake ninja nasm autoconf python3 glslang
          qt6-base-devel qt6-svg-devel qt6-charts-devel qt6-multimedia-devel
          boost-devel libusb-devel openssl-devel ffmpeg-devel zstd-devel lz4-devel mesalib-devel" ;;
*)  PKGS='' ;;
esac
PKGS=$(echo $PKGS)

install_pkgs() {
    case "$PM" in
    apt)    $SUDO apt-get install -y --no-install-recommends "$@" ;;
    dnf)    $SUDO dnf install -y "$@" ;;
    yum)    $SUDO yum install -y "$@" ;;
    pacman) $SUDO pacman -S --needed --noconfirm "$@" ;;
    zypper) $SUDO zypper --non-interactive install --no-recommends "$@" ;;
    apk)    $SUDO apk add "$@" ;;
    xbps)   $SUDO xbps-install -Sy "$@" ;;
    eopkg)  $SUDO eopkg install -y "$@" ;;
    esac
}

install_deps() {
    if [ "$PM" = unknown ]; then
        cat >&2 <<EOF
no supported package manager found (apt, dnf, yum, pacman, zypper, apk, xbps,
eopkg). Install the equivalent of these, then run scripts/build-suyu.sh:

  a C++17 toolchain, cmake >= $CMAKE_MIN, ninja, nasm, autoconf, pkg-config,
  python3, glslang (glslangValidator), Qt 6 base + private headers + svg +
  charts + multimedia + opengl, Boost (headers, filesystem, system, context),
  libusb-1.0, OpenSSL, FFmpeg (avcodec/avformat/avutil/swscale), zstd, lz4.
EOF
        exit 1
    fi

    say "package manager: $PM"
    if [ "$dry_run" = 1 ]; then
        printf '%s\n' $PKGS
        return
    fi

    case "$PM" in
        apt)    $SUDO apt-get update -qq ;;
        pacman) $SUDO pacman -Sy --noconfirm >/dev/null ;;
    esac

    # One transaction where it works. Package names drift between releases -
    # qt6-base-private-dev and ffmpeg-free-devel in particular - and most
    # managers abort the entire install over one unknown name, so fall back to
    # one at a time and report what was missing rather than stopping here.
    if ! install_pkgs $PKGS >/dev/null 2>&1; then
        warn 'batch install failed; retrying package by package'
        missing=''
        for p in $PKGS; do
            install_pkgs "$p" >/dev/null 2>&1 || missing="$missing $p"
        done
        if [ -n "$missing" ]; then
            warn "not available on this release:$missing"
            warn 'the build will say so if any of them was actually needed'
        fi
    fi
    say 'dependencies installed'
}

# --- cmake ------------------------------------------------------------------
# suyu's CPMUtil.cmake requires 3.31 and several current LTS releases ship
# older. Unpack an official build under ~/.local rather than fighting the
# system package; build-suyu.sh prefers ~/.local/bin/cmake when it is there.
ensure_cmake() {
    have=''
    command -v cmake >/dev/null && have=$(cmake --version | head -1 | awk '{print $3}')
    if [ -n "$have" ] && [ "$(printf '%s\n%s\n' "$have" "$CMAKE_MIN" | sort -V | head -1)" = "$CMAKE_MIN" ]; then
        say "cmake $have is new enough"
        return
    fi
    arch=$(uname -m)
    case "$arch" in
        x86_64|amd64)  arch=x86_64 ;;
        aarch64|arm64) arch=aarch64 ;;
        *) die "cmake ${have:-not installed} is older than $CMAKE_MIN and Kitware publishes no build for $arch; install a newer cmake yourself" ;;
    esac
    say "cmake ${have:-not installed} is older than $CMAKE_MIN - installing $CMAKE_GET under ~/.local"
    tarball="cmake-$CMAKE_GET-linux-$arch"
    tmp=$(mktemp -d)
    curl -fsSL -o "$tmp/cmake.tar.gz" \
        "https://github.com/Kitware/CMake/releases/download/v$CMAKE_GET/$tarball.tar.gz"
    mkdir -p "$HOME/.local/opt" "$HOME/.local/bin"
    tar xzf "$tmp/cmake.tar.gz" -C "$HOME/.local/opt"
    rm -rf "$tmp"
    ln -sfn "$HOME/.local/opt/$tarball" "$HOME/.local/opt/cmake"
    ln -sf "$HOME/.local/opt/cmake/bin/cmake" "$HOME/.local/bin/cmake"
    ln -sf "$HOME/.local/opt/cmake/bin/ctest" "$HOME/.local/bin/ctest"
    export PATH="$HOME/.local/bin:$PATH"
    say "cmake $("$HOME/.local/bin/cmake" --version | head -1 | awk '{print $3}') installed at ~/.local/bin/cmake"
}

# --- source -----------------------------------------------------------------
find_or_clone() {
    # Piped from curl, $0 is bash and BASH_SOURCE is unset, so an existing
    # clone is recognised from the working directory, not from the script path.
    if [ -f scripts/build-suyu.sh ] && [ -f .gitmodules ]; then
        ROOT=$(pwd)
        say "using the clone in $ROOT"
        git submodule update --init --recursive --jobs 8
        return
    fi
    ROOT="${MK8R_ROOT:-$(pwd)/mk8-recomp}"
    if [ -d "$ROOT/.git" ]; then
        say "updating the existing clone at $ROOT"
        git -C "$ROOT" pull --ff-only
        git -C "$ROOT" submodule update --init --recursive --jobs 8
    else
        say "cloning into $ROOT"
        git clone --recursive --jobs 8 "$REPO_URL" "$ROOT"
    fi
}

# --- run --------------------------------------------------------------------
install_deps
[ "$deps_only" = 1 ] && { say 'dependencies only - stopping here'; exit 0; }
[ "$dry_run" = 1 ] && { say 'dry run - stopping before cmake and clone'; exit 0; }
ensure_cmake
find_or_clone
[ "$no_build" = 1 ] && { say "cloned to $ROOT - build with: $ROOT/scripts/build-suyu.sh"; exit 0; }

say 'building - this takes a while'
"$ROOT/scripts/build-suyu.sh"

say 'done'
cat <<EOF
  binaries : $ROOT/build/suyu/bin
  run      : $ROOT/scripts/run-gui.sh

You still need your own legally dumped game and your own keys; nothing here
fetches either. See $ROOT/docs/assumptions.md.
EOF
