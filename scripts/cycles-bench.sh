#!/usr/bin/env bash
# Measure the replay in cycles and instructions per guest frame.
#
#   ARM=hybrid|baseline REPS=5 ./scripts/cycles-bench.sh
#
# Why not wall clock: it carries about +/-3% on this box, which is larger than
# most of what is worth measuring now. This reports ~1.5% on the median.
#
# Three things had to be right to get there, each of which cost a round of
# confusing numbers:
#
#   - perf attaches to the running suyu. Wrapping the runner counts the wrapper,
#     because the runner uses setsid to survive a dropped ssh.
#   - The window is bracketed by frame numbers, not seconds. A fixed duration
#     lands on a different stretch of the lap each run and the game does
#     different work per frame, which is worth ~7% on its own.
#   - The frame counter is polled every 50ms. Frames advance at roughly 700/s,
#     so a one-second poll overshoots by a different ~700 frames each run and
#     puts that same spread straight back.
#
# Take the median: the replay occasionally diverges, and when it does cycles and
# instructions both rise together while IPC holds - that is the guest doing
# different work, not the measurement wobbling.
set -euo pipefail

. "$HOME/.mk8r-env"
ROOT="$MK8R_ROOT"
cd "$ROOT"

LABEL="${LABEL:-cyc}"
REPS="${REPS:-3}"
ARM="${ARM:-hybrid}"
FROM="${FROM:-2000}"
TO="${TO:-7000}"

flag=""
[ "$ARM" = baseline ] && flag="--baseline"

frame_now() {
    python3 scripts/mcp-call.py get_emulator_state 2>/dev/null \
        | grep -oP '"tas_frame": \K[0-9]+' || echo 0
}

wait_for_frame() {
    local target=$1 limit=$2 f
    for _ in $(seq 1 $((limit * 20))); do
        f=$(frame_now)
        [ "${f:-0}" -ge "$target" ] && { echo "${f}"; return 0; }
        # Poll fast. Frames advance at roughly 700/s here, so a one-second poll
        # overshoots the bracket by a different ~700 frames every run, and the
        # ranges then cover different parts of the lap - which is what put 7%
        # of spread into a metric meant to remove it.
        sleep 0.05
    done
    echo 0
    return 1
}

for i in $(seq 1 "$REPS"); do
    pkill -x suyu 2>/dev/null || true
    sleep 3
    L=$(cut -d' ' -f1 /proc/loadavg)

    ./scripts/run-hybrid.sh $flag --tas --unlimited --seconds 600 \
        > "/tmp/cyc-$LABEL-$i.log" 2>&1 &
    runner=$!

    f0=$(wait_for_frame "$FROM" 240 || echo 0)
    if [ "${f0:-0}" -lt "$FROM" ]; then
        echo "$ARM rep$i FAILED never reached frame $FROM"
        wait "$runner" 2>/dev/null || true
        continue
    fi

    pid=$(pgrep -x suyu | head -1)
    sudo -n perf stat -e cycles,instructions -x, -o "/tmp/cyc-$LABEL-$i.txt" \
        -p "$pid" -- sleep 600 > /dev/null 2>&1 &
    perfpid=$!

    f1=$(wait_for_frame "$TO" 240 || echo 0)
    # SIGINT, not SIGKILL: perf writes its counts on interrupt and not otherwise.
    sudo -n pkill -INT -x perf 2>/dev/null || true
    wait "$perfpid" 2>/dev/null || true
    sleep 1

    cyc=$(grep -oP '^[0-9]+(?=,+cycles)' "/tmp/cyc-$LABEL-$i.txt" 2>/dev/null | head -1 || echo 0)
    ins=$(grep -oP '^[0-9]+(?=,+instructions)' "/tmp/cyc-$LABEL-$i.txt" 2>/dev/null | head -1 || echo 0)

    pkill -x suyu 2>/dev/null || true
    wait "$runner" 2>/dev/null || true

    python3 - "$ARM" "$i" "$L" "${f0:-0}" "${f1:-0}" "${cyc:-0}" "${ins:-0}" <<'PY'
import sys
arm, i, load, f0, f1, cyc, ins = sys.argv[1:8]
f0, f1, cyc, ins = int(f0), int(f1), int(cyc), int(ins)
d = f1 - f0
if d <= 0 or cyc <= 0:
    print(f"{arm} rep{i} FAILED range={f0}->{f1} cycles={cyc}")
else:
    print(f"{arm:8s} rep{i} load={load:5s} {f0}->{f1} ({d} frames) "
          f"Mcyc/frame={cyc/d/1e6:8.4f} Mins/frame={ins/d/1e6:8.4f} IPC={ins/cyc:.3f}")
PY
done
