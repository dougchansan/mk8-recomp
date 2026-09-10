#!/usr/bin/env python3
"""Boot a ROM and sample FPS repeatedly, reporting a distribution.

A single reading says almost nothing here. The the target attract sequence cycles
between the title screen and demo gameplay, which are very different workloads,
and early samples land while shaders are still compiling. Screenshots of the
status bar taken at arbitrary moments produced a 71-522 FPS spread on the same
build for exactly that reason.

So: skip the warmup, drop any sample taken while shaders are building, and
report the distribution rather than a number.

  python scripts/fps-sample.py <rom> [--warmup 60] [--samples 40] [--interval 2]
"""

import json
import pathlib
import statistics
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402


def call(name, args=None, timeout=120.0):
    out = rpc("tools/call", {"name": name, "arguments": args or {}}, timeout=timeout)
    for block in out.get("result", {}).get("content", []):
        if block.get("type") == "text":
            try:
                return json.loads(block["text"])
            except json.JSONDecodeError:
                return block["text"]
    return out.get("result", out)


def main():
    rom = sys.argv[1]
    args = sys.argv[2:]

    def opt(name, default):
        return type(default)(args[args.index(name) + 1]) if name in args else default

    warmup = opt("--warmup", 60)
    samples = opt("--samples", 40)
    interval = opt("--interval", 2.0)

    print(f"booting {pathlib.Path(rom).name} ...")
    print("  ", json.dumps(call("launch_game_path", {"path": rom})))

    print(f"warmup {warmup}s (shader compilation and boot) ...")
    time.sleep(warmup)

    fps, skipped = [], 0
    for _ in range(samples):
        try:
            st = call("get_emulator_state", timeout=30.0)
        except Exception as e:
            print(f"   (lost suyu: {type(e).__name__})")
            break
        if not st.get("game_running"):
            break
        # A sample taken while shaders compile measures the compiler, not the
        # recompiled code.
        if st.get("shaders_building", 0) > 0:
            skipped += 1
        else:
            v = st.get("fps")
            if isinstance(v, (int, float)) and v > 0:
                fps.append(float(v))
        time.sleep(interval)

    print("stopping ...")
    try:
        call("stop_emulation", timeout=120.0)
    except Exception:
        pass

    if not fps:
        print("no usable samples")
        return 1

    if "--out" in args:
        pathlib.Path(args[args.index("--out") + 1]).write_text(
            json.dumps(fps), encoding="utf-8")

    fps.sort()
    print(f"\nsamples={len(fps)}  skipped_shader_building={skipped}")
    print(f"  min    {min(fps):8.1f}")
    print(f"  p25    {fps[len(fps)//4]:8.1f}")
    print(f"  median {statistics.median(fps):8.1f}")
    print(f"  p75    {fps[3*len(fps)//4]:8.1f}")
    print(f"  max    {max(fps):8.1f}")
    print(f"  mean   {statistics.fmean(fps):8.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
