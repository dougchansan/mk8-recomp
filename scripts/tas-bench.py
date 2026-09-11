#!/usr/bin/env python3
"""Replay the loaded TAS script to its last frame and report elapsed time.

  python scripts/tas-bench.py <rom> [timeout_seconds]
"""

import json
import pathlib
import statistics
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402


def call(name, args=None, timeout=300.0):
    out = rpc("tools/call", {"name": name, "arguments": args or {}}, timeout=timeout)
    if "error" in out:
        return out["error"]
    for block in out.get("result", {}).get("content", []):
        if block.get("type") == "text":
            try:
                return json.loads(block["text"])
            except json.JSONDecodeError:
                return block["text"]
    return out.get("result", out)


def main():
    rom = sys.argv[1]
    budget = int(sys.argv[2]) if len(sys.argv) > 2 else 600

    print(f"booting {pathlib.Path(rom).name} ...")
    print("  ", json.dumps(call("launch_game_path", {"path": rom})))

    for _ in range(60):
        time.sleep(2)
        if call("get_emulator_state", timeout=30.0).get("game_running"):
            break
    else:
        print("game never started")
        return 1

    st = call("get_emulator_state", timeout=30.0)
    total = st.get("tas_total_frames") or 0
    if total <= 0:
        print(f"no TAS script loaded (tas_total_frames={total})")
        return 1
    print(f"script: {total} frames")

    print("  ", json.dumps(call("trigger_ui_action", {"action": "tas_start_stop"})))
    start = time.monotonic()
    samples = []
    peak = 0
    stalled = 0
    done_at = None
    outcome = "TIMEOUT"

    # Tas::Stop resets current_command to 0, so completion reads as the counter
    # falling back to zero rather than reaching the total.
    while True:
        time.sleep(0.5)
        elapsed = time.monotonic() - start
        st = call("get_emulator_state", timeout=30.0)
        frame = st.get("tas_frame") or 0
        running = st.get("tas_running")
        fps = st.get("fps") or 0.0
        if fps:
            samples.append(fps)

        if peak > 0 and (frame < peak or not running):
            done_at = elapsed
            outcome = "COMPLETE" if peak >= total - 120 else "ENDED_EARLY"
            break
        if frame == peak:
            stalled += 1
            if stalled >= 40:
                outcome = "STALLED"
                break
        else:
            stalled = 0
            if int(elapsed * 2) % 4 == 0:
                print(f"   {elapsed:6.1f}s  frame {frame:>6}/{total}  fps={fps:.1f}")
        peak = max(peak, frame)
        if elapsed > budget:
            break

    elapsed = done_at if done_at is not None else time.monotonic() - start
    print(f"RESULT {outcome} frames={peak}/{total} "
          f"seconds={elapsed:.2f} "
          f"mean_fps={statistics.mean(samples) if samples else 0:.1f} "
          f"median_fps={statistics.median(samples) if samples else 0:.1f}")

    call("stop_emulation", timeout=120.0)
    time.sleep(8)
    return 0


if __name__ == "__main__":
    sys.exit(main())
