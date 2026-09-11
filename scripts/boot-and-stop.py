#!/usr/bin/env python3
"""Boot a ROM through suyu's MCP server, let it run, then stop it cleanly.

The clean stop matters: the execution-coverage report (#13) is printed when the
last ArmRecomp instance is destroyed, so a killed process produces no report.

  python scripts/boot-and-stop.py <rom> [seconds]
"""

import json
import pathlib
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
    argv = [a for a in sys.argv[1:] if a != "--tas"]
    tas = "--tas" in sys.argv
    rom = argv[0]
    seconds = int(argv[1]) if len(argv) > 1 else 60

    print(f"booting {pathlib.Path(rom).name} ...")
    try:
        print("  ", json.dumps(call("launch_game_path", {"path": rom})))
    except Exception as e:
        print(f"   launch failed: {type(e).__name__}: {e}")
        return 1

    if tas:
        # pause_tas_on_load holds the script at frame 0 until this fires, so the
        # replay starts from a known frame rather than wherever boot happened to
        # be. OnTasStartStop is a no-op unless emulation is already running.
        for _ in range(30):
            time.sleep(2)
            try:
                if call("get_emulator_state", timeout=30.0).get("game_running"):
                    break
            except Exception:
                pass
        print("  ", json.dumps(call("trigger_ui_action", {"action": "tas_start_stop"})))

    for elapsed in range(0, seconds, 10):
        time.sleep(10)
        try:
            st = call("get_emulator_state", timeout=30.0)
            print(f"   {elapsed + 10:>4}s  running={st.get('game_running')} "
                  f"first_frame={st.get('first_frame_displayed')} "
                  f"fps={st.get('fps')}")
        except Exception as e:
            print(f"   {elapsed + 10:>4}s  (suyu gone: {type(e).__name__})")
            return 1

    print("stopping ...")
    try:
        print("  ", json.dumps(call("stop_emulation", timeout=120.0)))
    except Exception as e:
        print(f"   stop failed: {type(e).__name__}: {e}")

    # The report fires from the last ArmRecomp destructor, which runs during
    # emulation teardown - give it a moment to reach the log.
    time.sleep(8)
    return 0


if __name__ == "__main__":
    sys.exit(main())
