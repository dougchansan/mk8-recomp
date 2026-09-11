#!/usr/bin/env bash
# Launch the suyu Qt frontend headless, so its MCP server comes up.
#
# Two things this has to get right on a headless box over SSH:
#
#   - suyu is a Qt application and needs a display even when nothing will look
#     at it, so it runs under Xvfb.
#   - `nohup cmd &` over SSH dies when the connection closes. setsid detaches it
#     into its own session, which survives.
#
# The mode flag is not optional: on first launch a modal ModeSelector blocks
# startup and ApplyAppMode - which starts the MCP listener - never runs.
#
#   ./scripts/run-gui.sh              # start, wait for MCP
#   ./scripts/run-gui.sh --stop       # stop suyu and Xvfb
set -euo pipefail

ROOT="${ROOT:-$HOME/mk8-recomp}"
EXE="$ROOT/build/suyu/bin/suyu"
DISPLAY_NUM="${DISPLAY_NUM:-:78}"
MODE="${MODE:-hacker}"
LOG="${LOG:-/tmp/suyu-gui.log}"

# The bracket trick: a plain pattern would match this script's own command line
# and kill the shell running it.
stop() {
    pkill -f "[s]uyu/bin/suyu" 2>/dev/null || true
    pkill -f "[X]vfb $DISPLAY_NUM" 2>/dev/null || true
    sleep 1
}

if [ "${1:-}" = "--stop" ]; then
    stop
    echo 'stopped'
    exit 0
fi

[ -x "$EXE" ] || { echo "not built: $EXE" >&2; exit 1; }
stop

if ! pgrep -f "[X]vfb $DISPLAY_NUM" >/dev/null 2>&1; then
    setsid Xvfb "$DISPLAY_NUM" -screen 0 1920x1080x24 >/tmp/xvfb.log 2>&1 < /dev/null &
    sleep 2
fi

setsid env DISPLAY="$DISPLAY_NUM" "$EXE" "-$MODE" > "$LOG" 2>&1 < /dev/null &
sleep 3
pid=$(pgrep -f "[s]uyu/bin/suyu" | head -1)
echo "suyu pid ${pid:-unknown}, DISPLAY=$DISPLAY_NUM, log $LOG"

for _ in $(seq 1 60); do
    if (exec 3<>/dev/tcp/127.0.0.1/9742) 2>/dev/null; then
        exec 3>&- 2>/dev/null || true
        echo 'MCP server listening on 127.0.0.1:9742'
        exit 0
    fi
    sleep 2
done

echo 'MCP server did not come up within 120s' >&2
tail -5 "$LOG" >&2
exit 1
