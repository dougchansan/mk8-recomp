#!/usr/bin/env python3
"""Run one unattended title/menu playtest through a running suyu MCP server.

The caller starts suyu with the desired static module directory and strict-mode
environment. This script launches the title, requests guest-renderer screenshots,
stops emulation cleanly, and writes a machine-readable observation. Screenshots
show that visible content rendered; a human still labels whether it is the title
screen/menu rather than a splash, warning, or applet.
"""

import argparse
import json
import pathlib
import sys
import time

from PIL import Image, ImageStat

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402


def call(name, args=None, timeout=300.0):
    response = rpc("tools/call", {"name": name, "arguments": args or {}}, timeout=timeout)
    if "error" in response:
        raise RuntimeError(json.dumps(response["error"]))
    for block in response.get("result", {}).get("content", []):
        if block.get("type") == "text":
            value = json.loads(block["text"])
            if isinstance(value, dict) and value.get("success") is False:
                raise RuntimeError(value.get("error", json.dumps(value)))
            return value
    return response.get("result", response)


def wait_for_file(path, timeout=45.0):
    deadline = time.monotonic() + timeout
    previous = -1
    stable = 0
    while time.monotonic() < deadline:
        if path.exists():
            size = path.stat().st_size
            stable = stable + 1 if size > 0 and size == previous else 0
            previous = size
            if stable >= 2:
                return
        time.sleep(0.5)
    raise TimeoutError(f"screenshot was not written: {path}")


def image_metrics(path):
    with Image.open(path) as image:
        rgb = image.convert("RGB")
        sample = rgb.resize((160, 90))
        colors = len(sample.getcolors(maxcolors=160 * 90) or [])
        extrema = sample.getextrema()
        spread = max(high - low for low, high in extrema)
        variance = sum(ImageStat.Stat(sample).var) / 3.0
        return {"width": rgb.width, "height": rgb.height, "sample_colors": colors,
                "channel_spread": spread, "variance": round(variance, 3),
                "visibly_nonuniform": colors >= 8 and spread >= 16 and variance >= 4.0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom")
    parser.add_argument("output")
    parser.add_argument("--seconds", type=int, default=180)
    parser.add_argument("--capture", type=int, nargs="+", default=[30, 90, 150])
    parser.add_argument("--baseline", action="store_true",
                        help="expect Dynarmic rather than a static backend")
    args = parser.parse_args()
    captures = sorted(set(t for t in args.capture if 0 < t <= args.seconds))
    out_dir = pathlib.Path(args.output).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    result = {"rom_name": pathlib.Path(args.rom).name, "duration": args.seconds,
              "capture_seconds": captures, "states": [], "screenshots": [],
              "missed_captures": [],
              "first_frame": False, "static_backend_active": False,
              "max_static_blocks": 0, "max_jit_transitions": 0,
              "stopped_cleanly": False, "error": None}
    started = time.monotonic()
    next_capture = 0
    try:
        call("launch_game_path", {"path": args.rom})
        while time.monotonic() - started < args.seconds:
            elapsed = int(time.monotonic() - started)
            state = call("get_emulator_state", timeout=30.0)
            snapshot = {key: state.get(key) for key in
                        ("game_running", "first_frame_displayed", "fps", "vps",
                         "shaders_building", "static_backend_active", "static_blocks",
                         "jit_transitions", "jit_available", "guard_v2_ready")}
            snapshot["elapsed"] = elapsed
            result["states"].append(snapshot)
            result["first_frame"] |= bool(state.get("first_frame_displayed"))
            result["static_backend_active"] |= bool(state.get("static_backend_active"))
            result["max_static_blocks"] = max(result["max_static_blocks"],
                                                  int(state.get("static_blocks") or 0))
            result["max_jit_transitions"] = max(result["max_jit_transitions"],
                                                    int(state.get("jit_transitions") or 0))
            if not args.baseline and result["max_jit_transitions"]:
                raise RuntimeError("strict static run requested a JIT transition")
            while next_capture < len(captures) and elapsed >= captures[next_capture]:
                second = captures[next_capture]
                if not state.get("first_frame_displayed"):
                    result["missed_captures"].append(second)
                    next_capture += 1
                    continue
                path = out_dir / f"t{second:03d}.png"
                if path.exists():
                    path.unlink()
                call("capture_game_screenshot", {"path": str(path)})
                wait_for_file(path)
                result["screenshots"].append({"second": second, "path": str(path),
                                                **image_metrics(path)})
                next_capture += 1
            if not state.get("game_running"):
                raise RuntimeError("emulation stopped before the playtest interval completed")
            time.sleep(2)
        call("stop_emulation", timeout=120.0)
        result["stopped_cleanly"] = True
        time.sleep(8)
    except Exception as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
        try:
            call("stop_emulation", timeout=120.0)
        except Exception:
            pass
    result["rendered_visible_content"] = bool(result["screenshots"]) and all(
        shot["visibly_nonuniform"] for shot in result["screenshots"])
    backend_ok = (not result["static_backend_active"]) if args.baseline else (
        result["static_backend_active"] and result["max_jit_transitions"] == 0)
    result["machine_status"] = (
        "REVIEW_REQUIRED" if result["error"] is None and result["first_frame"]
        and backend_ok and result["rendered_visible_content"] and result["stopped_cleanly"]
        else "FAILED"
    )
    output = out_dir / "observation.json"
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["machine_status"] == "REVIEW_REQUIRED" else 1


if __name__ == "__main__":
    raise SystemExit(main())
