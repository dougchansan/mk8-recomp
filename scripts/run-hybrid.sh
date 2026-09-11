#!/usr/bin/env bash
# Boot the target with AOT images loaded, headless. --baseline runs dynarmic only.
#
# SUYU_RECOMP_DIR must be set before launch: the CPU backend is picked once at
# process start, so attaching to a running suyu silently gets the JIT.
set -euo pipefail

ROOT="${ROOT:-$HOME/mk8-recomp}"
TARGET="${TARGET:-target}"
ROM="${ROM:-$HOME/roms/target.xci}"
RUN_SECONDS="${RUN_SECONDS:-60}"
DISPLAY_NUM="${DISPLAY_NUM:-:78}"
LOG="${LOG:-/tmp/suyu-hybrid.log}"
CFG="$HOME/.config/suyu/qt-config.ini"
SUYU_LOG="$HOME/.local/share/suyu/log/suyu_log.txt"

BASELINE=0
UNLIMITED=0
TAS=0
ONLY=""
while [ $# -gt 0 ]; do
    case "$1" in
        --baseline)  BASELINE=1; shift ;;
        --unlimited) UNLIMITED=1; shift ;;
        --tas)       TAS=1; shift ;;
        --only)      ONLY="$2"; shift 2 ;;
        --seconds)   RUN_SECONDS="$2"; shift 2 ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

EXE="$ROOT/build/suyu/bin/suyu"
RECOMP_IN="$ROOT/build/recomp/$TARGET"
[ -x "$EXE" ] || { echo "not built: $EXE" >&2; exit 1; }
[ -f "$ROM" ] || { echo "no rom: $ROM" >&2; exit 1; }

pkill -x suyu 2>/dev/null || true
sleep 2

if [ "$BASELINE" = 1 ]; then
    unset SUYU_RECOMP_DIR || true
    echo 'BASELINE: dynarmic only'
else
    if [ -n "$ONLY" ]; then
        stage="$ROOT/build/recomp-stage"
        rm -rf "$stage"
        for m in ${ONLY//,/ }; do
            [ -d "$RECOMP_IN/$m" ] || { echo "no such module: $m" >&2; exit 1; }
            mkdir -p "$stage/$m"
            find "$RECOMP_IN/$m" -name '*.so' -exec ln {} "$stage/$m/" \;
        done
        RECOMP_IN="$stage"
        echo "SUBSET: $ONLY"
    fi
    n=$(find "$RECOMP_IN" -name '*.so' | wc -l)
    [ "$n" -gt 0 ] || { echo "no .so under $RECOMP_IN" >&2; exit 1; }
    export SUYU_RECOMP_DIR="$RECOMP_IN"
    echo "HYBRID: $n AOT modules from $RECOMP_IN"
    find "$RECOMP_IN" -name '*.so' -printf '    %-14f %10s bytes\n'
fi

# suyu rewrites qt-config.ini on exit, so these are set per run. A setting whose
# \default companion is true is ignored, which is how this silently did nothing
# the first time it was tried.
if [ "$UNLIMITED" = 1 ] && [ -f "$CFG" ]; then
    sed -i -e 's/use_speed_limit\\default=true/use_speed_limit\\default=false/' \
           -e 's/use_speed_limit=true/use_speed_limit=false/' \
           -e 's/use_vsync\\default=true/use_vsync\\default=false/' \
           -e 's/use_vsync=[0-9]*/use_vsync=0/' "$CFG"
    echo 'UNLIMITED: frame limiter and vsync off'
fi

rm -f "$SUYU_LOG"

if ! pgrep -f "[X]vfb $DISPLAY_NUM" >/dev/null 2>&1; then
    setsid Xvfb "$DISPLAY_NUM" -screen 0 1920x1080x24 >/tmp/xvfb.log 2>&1 < /dev/null &
    sleep 2
fi

setsid env DISPLAY="$DISPLAY_NUM" SUYU_RECOMP_DIR="${SUYU_RECOMP_DIR:-}" \
    "$EXE" -hacker > "$LOG" 2>&1 < /dev/null &
sleep 3
echo "suyu pid $(pgrep -x suyu | head -1)"

for _ in $(seq 1 60); do
    if (exec 3<>/dev/tcp/127.0.0.1/9742) 2>/dev/null; then exec 3>&- 2>/dev/null || true; ok=1; break; fi
    sleep 2
done
[ "${ok:-}" = 1 ] || { echo 'MCP did not come up' >&2; tail -5 "$LOG" >&2; exit 1; }

if [ "$TAS" = 1 ]; then
    [ -f "$HOME/.local/share/suyu/tas/script0-1.txt" ]         || { echo 'no script0-1.txt in the tas directory' >&2; exit 1; }
    python3 "$ROOT/scripts/boot-and-stop.py" --tas "$ROM" "$RUN_SECONDS"
else
    python3 "$ROOT/scripts/boot-and-stop.py" "$ROM" "$RUN_SECONDS"
fi

# The execution-coverage report prints from the last ArmRecomp destructor, which
# only runs on a real teardown. Killing suyu here loses the whole report.
echo 'closing suyu ...'
pkill -x -TERM suyu 2>/dev/null || true
for _ in $(seq 1 120); do pgrep -x suyu >/dev/null || break; sleep 2; done
pkill -x -KILL suyu 2>/dev/null || true
echo 'done'
