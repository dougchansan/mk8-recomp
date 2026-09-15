#!/usr/bin/env python3
"""Boot a ROM and sample the frame counters repeatedly, reporting a distribution.

A single reading says almost nothing here. The attract sequence cycles
between the title screen and demo gameplay, which are very different workloads,
and early samples land while shaders are still compiling. Screenshots of the
status bar taken at arbitrary moments produced a 71-522 FPS spread on the same
build for exactly that reason.

So: skip the warmup, drop any sample taken while shaders are building, and
report the distribution rather than a number.

Three counters are recorded per sample, not one. fps is presentation rate,
vps is the guest's own frame rate, and frame_ms is host frame time. A CPU
backend change can move vps without moving fps at all when the host is waiting
on the GPU, so recording only fps can hide the thing being measured.

The recompiler liveness fields are recorded alongside them. A static arm that
fell back to the JIT immediately still produces a perfectly good FPS
distribution - that has already happened on this project, and the resulting
numbers were credited to static optimizations before anyone noticed the arm had
executed zero static blocks. Whether an arm is valid is a separate question
from how fast it was, so both are written out and the caller decides.

  python scripts/fps-sample.py <rom> [--warmup 60] [--samples 40] [--interval 2]
"""

import json
import pathlib
import statistics
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402

METRICS = ("fps", "vps", "frame_ms")


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
    label = opt("--label", "")

    # --no-launch: the caller already booted the game and started whatever it
    # wants measured (TAS playback, say), so launching again would restart it.
    if "--no-launch" not in args:
        print(f"booting {pathlib.Path(rom).name} ...")
        print("  ", json.dumps(call("launch_game_path", {"path": rom})))

    print(f"warmup {warmup}s (shader compilation and boot) ...")
    time.sleep(warmup)

    series = {m: [] for m in METRICS}
    skipped = 0
    # Why a sample produced nothing matters more than the fact that it did. A
    # title that runs but never presents a frame and a title that stopped during
    # warmup both end up with an empty fps list, and they are completely
    # different problems.
    zero_fps = 0
    polls = 0
    stop_reason = "completed"
    # Blocks only ever accumulate, so first and last observations bound the work
    # this arm actually did statically. jit_transitions is the fallback count.
    live = {"first": None, "last": None}

    for _ in range(samples):
        try:
            st = call("get_emulator_state", timeout=30.0)
        except Exception as e:
            print(f"   (lost suyu: {type(e).__name__})")
            stop_reason = f"lost suyu: {type(e).__name__}"
            break
        polls += 1
        if not st.get("game_running"):
            stop_reason = "game stopped running"
            break

        snapshot = {k: st.get(k) for k in (
            "static_backend_active", "static_blocks", "jit_transitions",
            "jit_available", "guard_v2_ready")}
        if live["first"] is None:
            live["first"] = snapshot
        live["last"] = snapshot

        # A sample taken while shaders compile measures the compiler, not the
        # recompiled code.
        if st.get("shaders_building", 0) > 0:
            skipped += 1
        else:
            fps_now = st.get("fps")
            if not (isinstance(fps_now, (int, float)) and fps_now > 0):
                zero_fps += 1
            for m in METRICS:
                v = st.get(m)
                if isinstance(v, (int, float)) and v > 0:
                    series[m].append(float(v))
        time.sleep(interval)

    print("stopping ...")
    try:
        call("stop_emulation", timeout=120.0)
    except Exception:
        pass

    first, last = live["first"] or {}, live["last"] or {}
    record = {
        "label": label or pathlib.Path(rom).stem,
        "metrics": series,
        "polls": polls,
        "skipped_shader_building": skipped,
        "zero_fps_samples": zero_fps,
        "stop_reason": stop_reason,
        "recomp": {
            "backend_active": last.get("static_backend_active"),
            "jit_available": last.get("jit_available"),
            "guard_v2_ready": last.get("guard_v2_ready"),
            "static_blocks_first": first.get("static_blocks"),
            "static_blocks_last": last.get("static_blocks"),
            "jit_transitions": last.get("jit_transitions"),
        },
    }

    # Written even when there is nothing to plot. An arm that produced no
    # samples is the case where the recompiler evidence below is most worth
    # keeping - a title can run hundreds of millions of static blocks and still
    # never present a frame, and that is not the same failure as one that
    # stopped during warmup.
    if "--out" in args:
        pathlib.Path(args[args.index("--out") + 1]).write_text(
            json.dumps(record, indent=2), encoding="utf-8")

    r = record["recomp"]
    if not series["fps"]:
        print(f"\nno usable samples after {polls} poll(s): {stop_reason}")
        print(f"  skipped while shaders were building : {skipped}")
        print(f"  polled with fps still at zero       : {zero_fps}")
        print(f"  static_blocks {r['static_blocks_first']} -> {r['static_blocks_last']}"
              f"  jit_transitions={r['jit_transitions']}")
        if zero_fps and not skipped:
            print("  the title ran but never presented a frame; this is not a"
                  " sampling problem, compare against the baseline arm")
        return 1

    for m in METRICS:
        xs = sorted(series[m])
        if not xs:
            continue
        print(f"\n{m}: n={len(xs)}  skipped_shader_building={skipped}")
        print(f"  min    {min(xs):8.1f}")
        print(f"  p25    {xs[len(xs)//4]:8.1f}")
        print(f"  median {statistics.median(xs):8.1f}")
        print(f"  p75    {xs[3*len(xs)//4]:8.1f}")
        print(f"  max    {max(xs):8.1f}")
        print(f"  mean   {statistics.fmean(xs):8.1f}")

    print(f"\nrecomp: backend_active={r['backend_active']} jit_available={r['jit_available']}"
          f" guard_v2={r['guard_v2_ready']}")
    print(f"        static_blocks {r['static_blocks_first']} -> {r['static_blocks_last']}"
          f"  jit_transitions={r['jit_transitions']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
