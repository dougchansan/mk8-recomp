#!/usr/bin/env python3
"""Replay the configured TAS under strict static execution and retain evidence."""

import argparse
import hashlib
import json
import os
import pathlib
import re
import sys
import time

from PIL import Image, ImageStat

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from mcp import rpc  # noqa: E402


def tas_directory(config, default):
    """Read suyu's existing TAS directory setting without interpreting other keys."""
    text = config.read_text(encoding="utf-8-sig") if config.is_file() else ""
    match = re.search(r"(?m)^tas_directory=(.*)$", text)
    value = match.group(1).strip().strip('"') if match else ""
    return pathlib.Path(value).resolve() if value else pathlib.Path(default).resolve()


def fixture_manifest(directory):
    scripts = sorted(directory.glob("script*.txt"))
    if not (directory / "script0-1.txt").is_file():
        raise ValueError(f"fixture has no script0-1.txt: {directory}")
    return [{"file": script.name, "bytes": script.stat().st_size,
             "sha256": hashlib.sha256(script.read_bytes()).hexdigest()} for script in scripts]


def select_fixture(config, directory, restore_file):
    """Temporarily select a directory using the driver's existing configuration."""
    if restore_file.exists():
        raise ValueError(f"fixture restoration record already exists: {restore_file}")
    if not directory.is_dir():
        raise ValueError(f"fixture directory does not exist: {directory}")
    original = config.read_text(encoding="utf-8-sig")
    lines = re.findall(r"(?m)^tas_directory(?:\\default)?=.*(?:\n|$)", original)
    replacement = f"tas_directory={directory.resolve().as_posix()}\ntas_directory\\default=false\n"
    updated = re.sub(r"(?m)^tas_directory(?:\\default)?=.*(?:\n|$)", "", original)
    section = re.search(r"(?m)^\[Data(?:%20| )Storage\]\s*\n", updated)
    if section is None:
        raise ValueError("configuration has no [Data Storage] section")
    updated = updated[:section.end()] + replacement + updated[section.end():]
    restore_file.write_text(json.dumps({"config": str(config.resolve()), "lines": lines,
                                      "selected_directory": str(directory.resolve())}, indent=2),
                            encoding="utf-8")
    config.write_text(updated, encoding="utf-8")


def restore_fixture(restore_file):
    """Restore just TAS selection, retaining other settings saved by the frontend."""
    record = json.loads(restore_file.read_text(encoding="utf-8"))
    config = pathlib.Path(record["config"])
    if tas_directory(config, "") != pathlib.Path(record["selected_directory"]):
        raise RuntimeError("TAS directory changed outside this run; restore record retained")
    current = config.read_text(encoding="utf-8-sig")
    updated = re.sub(r"(?m)^tas_directory(?:\\default)?=.*(?:\n|$)", "", current)
    section = re.search(r"(?m)^\[Data(?:%20| )Storage\]\s*\n", updated)
    if section is None:
        raise ValueError("configuration has no [Data Storage] section; restore record retained")
    restored_lines = "".join(line.rstrip("\n") + "\n" for line in record["lines"])
    updated = updated[:section.end()] + restored_lines + updated[section.end():]
    config.write_text(updated, encoding="utf-8")
    record["restored"] = True
    restore_file.write_text(json.dumps(record, indent=2), encoding="utf-8")


def fixture_command(argv):
    parser = argparse.ArgumentParser(description="Select or restore an isolated TAS fixture")
    parser.add_argument("operation", choices=("select-fixture", "restore-fixture", "describe-fixture"))
    parser.add_argument("--config", type=pathlib.Path)
    parser.add_argument("--directory", type=pathlib.Path)
    parser.add_argument("--restore-file", type=pathlib.Path)
    args = parser.parse_args(argv)
    if args.operation == "restore-fixture":
        if not args.restore_file:
            parser.error("--restore-file is required")
        restore_fixture(args.restore_file)
    elif args.operation == "select-fixture":
        if not all((args.config, args.directory, args.restore_file)):
            parser.error("--config, --directory, and --restore-file are required")
        select_fixture(args.config, args.directory, args.restore_file)
    else:
        if not args.directory and not args.config:
            parser.error("--directory or --config is required")
        directory = args.directory or tas_directory(
            args.config, pathlib.Path(os.environ["APPDATA"]) / "suyu" / "tas")
        print(json.dumps({"directory": str(directory.resolve()),
                          "scripts": fixture_manifest(directory)}))
    return 0


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


def capture(path):
    if path.exists():
        path.unlink()
    call("capture_game_screenshot", {"path": str(path)})
    deadline = time.monotonic() + 45
    previous, stable = -1, 0
    while time.monotonic() < deadline:
        if path.exists():
            size = path.stat().st_size
            stable = stable + 1 if size > 0 and size == previous else 0
            previous = size
            if stable >= 2:
                with Image.open(path) as image:
                    sample = image.convert("RGB").resize((160, 90))
                    colors = len(sample.getcolors(maxcolors=160 * 90) or [])
                    spread = max(high - low for low, high in sample.getextrema())
                    variance = sum(ImageStat.Stat(sample).var) / 3
                    # Hashed so a held still image can be told from a live scene.
                    # visibly_nonuniform only asks whether pixels vary within one
                    # frame; it passes a frozen picture just as happily.
                    digest = hashlib.sha256(path.read_bytes()).hexdigest()
                    return {"path": str(path), "width": image.width, "height": image.height,
                            "sample_colors": colors, "channel_spread": spread,
                            "variance": round(variance, 3), "sha256": digest,
                            "visibly_nonuniform": colors >= 8 and spread >= 16 and variance >= 4}
        time.sleep(0.5)
    raise TimeoutError(f"screenshot was not written: {path}")


def difference_hash(path):
    with Image.open(path) as image:
        pixels = list(image.convert("L").resize((9, 8)).getdata())
    value = 0
    for row in range(8):
        for column in range(8):
            value = (value << 1) | (pixels[row * 9 + column] >
                                    pixels[row * 9 + column + 1])
    return value


def record_state(result, state, phase="replay"):
    result["first_frame"] |= bool(state.get("first_frame_displayed"))
    result["static_backend_active"] |= bool(state.get("static_backend_active"))
    result["max_static_blocks"] = max(result["max_static_blocks"],
                                      int(state.get("static_blocks") or 0))
    result["max_jit_transitions"] = max(result["max_jit_transitions"],
                                        int(state.get("jit_transitions") or 0))
    snapshot = {key: state.get(key) for key in
                ("tas_frame", "tas_running", "fps", "vps", "game_running",
                 "first_frame_displayed", "static_blocks", "jit_transitions",
                 "jit_available", "guard_v2_ready", "tas_completion_generation",
                 "tas_completed_commands", "tas_completion_looping")}
    snapshot["phase"] = phase
    # Seconds since the run started. A replayed script is a fixed workload, so
    # the honest CPU comparison between backends is how long each takes to reach
    # the last command - but without a time axis on these samples that cannot be
    # computed after the fact, which is why the first attempt at an equal-work
    # benchmark had to be thrown away.
    snapshot["t"] = round(time.monotonic() - result["_t0"], 3)
    result["states"].append(snapshot)
    # A transition is a failed run only when the arm claimed to be strict. A
    # deliberately partial image set - module bisection hands one module to the
    # JIT - takes transitions by construction, and aborting on the first one
    # ends the replay before it starts.
    if result["max_jit_transitions"] and not result.get("_allow_jit"):
        raise RuntimeError("strict TAS replay requested a JIT transition")
    return snapshot


def observe_after_eof(result, output, seconds, interval):
    """Collect delayed rendering evidence without changing the exact-EOF verdict."""
    observation = result["post_eof"]
    if result["tas_outcome"] != "COMPLETE":
        raise RuntimeError("post-EOF observation requires natural, non-looping completion")
    observation["outcome"] = "OBSERVING"
    started = time.monotonic()
    next_capture = min(interval, seconds)
    while True:
        elapsed = time.monotonic() - started
        time.sleep(min(2.0, max(0.0, next_capture - elapsed), max(0.0, seconds - elapsed)))
        state = call("get_emulator_state", timeout=30)
        snapshot = record_state(result, state, "post_eof")
        elapsed = time.monotonic() - started
        snapshot["seconds_after_eof"] = round(elapsed, 3)
        observation["elapsed_seconds"] = round(elapsed, 3)
        if (not state.get("game_running") or state.get("tas_running")
                or int(state.get("tas_completion_generation") or 0) !=
                result["tas_completion_generation"]
                or int(state.get("tas_completed_commands") or 0) !=
                result["tas_completed_commands"] or state.get("tas_completion_looping")):
            observation["outcome"] = "INTERRUPTED"
            raise RuntimeError("game or completed TAS changed during post-EOF observation")
        if elapsed >= next_capture or elapsed >= seconds:
            shot = capture(output / f"post-eof-{elapsed:07.2f}s.png")
            shot["seconds_after_eof"] = round(elapsed, 3)
            observation["screenshots"].append(shot)
            next_capture = min(next_capture + interval, seconds)
        if elapsed >= seconds:
            observation["outcome"] = "COMPLETE"
            break


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom")
    parser.add_argument("output")
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--baseline", action="store_true")
    parser.add_argument("--expected-final-image")
    parser.add_argument("--max-final-hash-distance", type=int, default=12)
    parser.add_argument("--expected-commands", type=int, default=0)
    parser.add_argument("--fixture-directory", type=pathlib.Path,
                        help="directory selected before host startup; recorded as input provenance")
    parser.add_argument("--observe-after-seconds", type=float, default=0,
                        help="after natural EOF, observe idle rendering for 0..180 seconds")
    parser.add_argument("--observe-capture-interval", type=float, default=30,
                        help="seconds between post-EOF screenshots (default: 30)")
    parser.add_argument("--allow-jit", action="store_true",
                        help="expect JIT transitions; for a deliberately partial image set")
    parser.add_argument("--start-delay", type=int, default=0,
                        help="diagnostic: wait this many seconds after launch before playback")
    args = parser.parse_args(argv)
    if not 0 <= args.observe_after_seconds <= 180:
        parser.error("--observe-after-seconds must be between 0 and 180")
    if not 0 < args.observe_capture_interval <= 180:
        parser.error("--observe-capture-interval must be greater than 0 and at most 180")
    output = pathlib.Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    result = {"rom_name": pathlib.Path(args.rom).name, "_t0": time.monotonic(),
              "_allow_jit": args.allow_jit, "jit_allowed": args.allow_jit,
              "states": [], "screenshots": [],
              "first_frame": False, "static_backend_active": False, "max_static_blocks": 0,
              "max_jit_transitions": 0, "stopped_cleanly": False, "tas_total_frames": 0,
              "tas_peak_frame": 0, "tas_outcome": "NOT_STARTED", "error": None,
              "post_eof": {"requested_seconds": args.observe_after_seconds,
                           "capture_interval_seconds": args.observe_capture_interval,
                           "elapsed_seconds": 0, "outcome": "NOT_REQUESTED" if
                           not args.observe_after_seconds else "NOT_STARTED", "screenshots": []}}
    fixture = args.fixture_directory or tas_directory(
        pathlib.Path(os.environ.get("APPDATA", "")) / "suyu/config/qt-config.ini",
        pathlib.Path(os.environ.get("APPDATA", "")) / "suyu/tas")
    tas_file = fixture / "script0-1.txt"
    if tas_file.is_file():
        result["tas_script"] = str(tas_file)
        result["tas_script_sha256"] = hashlib.sha256(tas_file.read_bytes()).hexdigest()
    try:
        result["tas_fixture_directory"] = str(fixture.resolve())
        result["tas_fixture_manifest"] = fixture_manifest(fixture)
        initial_state = call("get_emulator_state", timeout=30)
        start_generation = initial_state.get("tas_completion_generation")
        if start_generation is None:
            raise RuntimeError("host does not expose exact TAS completion telemetry")
        start_generation = int(start_generation)
        launch_started = time.monotonic()
        launch_args = {"path": args.rom}
        if args.start_delay <= 0:
            launch_args["tas_mode"] = "playback"
        call("launch_game_path", launch_args)
        for _ in range(60):
            state = call("get_emulator_state", timeout=30)
            total = int(state.get("tas_total_frames") or 0)
            if state.get("game_running") and total > 0:
                break
            time.sleep(2)
        else:
            raise RuntimeError("game or TAS script did not become ready")
        result["tas_total_frames"] = total
        if args.expected_commands and total != args.expected_commands:
            raise RuntimeError(
                f"TAS length changed: expected {args.expected_commands}, loaded {total}"
            )
        result["playback_start_delay_seconds"] = max(0, args.start_delay)
        if args.start_delay > 0:
            remaining = args.start_delay - (time.monotonic() - launch_started)
            if remaining > 0:
                time.sleep(remaining)
            call("trigger_ui_action", {"action": "tas_reset"})
            call("trigger_ui_action", {"action": "tas_start_stop"})
        started = time.monotonic()
        peak, stalled = 0, 0
        thresholds = [total * fraction // 100 for fraction in (25, 50, 75, 95)]
        captured = set()
        while time.monotonic() - started <= args.timeout:
            time.sleep(0.5)
            state = call("get_emulator_state", timeout=30)
            frame = int(state.get("tas_frame") or 0)
            running = bool(state.get("tas_running"))
            record_state(result, state)
            for index, threshold in enumerate(thresholds):
                if frame >= threshold and index not in captured:
                    shot = capture(output / f"frame-{frame:06d}.png")
                    shot["frame"] = frame
                    result["screenshots"].append(shot)
                    captured.add(index)
            completion_generation = int(state.get("tas_completion_generation") or 0)
            if completion_generation > start_generation:
                completed_commands = int(state.get("tas_completed_commands") or 0)
                completion_looping = bool(state.get("tas_completion_looping"))
                result["tas_completed_commands"] = completed_commands
                result["tas_completion_generation"] = completion_generation
                result["tas_completion_looping"] = completion_looping
                result["tas_outcome"] = (
                    "LOOPED" if completion_looping else
                    "COMPLETE" if completed_commands == total else "ENDED_EARLY"
                )
                if result["tas_outcome"] == "COMPLETE" and result["first_frame"]:
                    shot = capture(output / f"frame-final-{completed_commands:06d}.png")
                    shot["frame"] = peak
                    shot["completed_commands"] = completed_commands
                    shot["final"] = True
                    result["screenshots"].append(shot)
                    if args.expected_final_image:
                        expected = pathlib.Path(args.expected_final_image).resolve()
                        if not expected.is_file():
                            raise RuntimeError(f"expected final image does not exist: {expected}")
                        distance = (difference_hash(pathlib.Path(shot["path"])) ^
                                    difference_hash(expected)).bit_count()
                        result["expected_final_image"] = str(expected)
                        result["final_hash_distance"] = distance
                        result["final_image_match"] = distance <= args.max_final_hash_distance
                break
            if peak > 0 and not running:
                result["tas_outcome"] = "STOPPED_EARLY"
                break
            # Frame zero includes the entire guarded boot before the first TAS
            # command is consumed. Only call it stalled after playback advances.
            stalled = stalled + 1 if peak > 0 and frame == peak else 0
            if stalled >= 40:
                result["tas_outcome"] = "STALLED"
                break
            peak = max(peak, frame)
            result["tas_peak_frame"] = peak
        else:
            result["tas_outcome"] = "TIMEOUT"
        if args.observe_after_seconds > 0 and result["tas_outcome"] == "COMPLETE":
            observe_after_eof(result, output, args.observe_after_seconds,
                              args.observe_capture_interval)
        call("stop_emulation", timeout=120)
        result["stopped_cleanly"] = True
        time.sleep(8)
    except Exception as exc:
        result["error"] = f"{type(exc).__name__}: {exc}"
        if result["post_eof"]["outcome"] == "OBSERVING":
            result["post_eof"]["outcome"] = "FAILED"
        try:
            call("stop_emulation", timeout=120)
        except Exception:
            pass
    result["rendered_visible_content"] = bool(result["screenshots"]) and all(
        shot["visibly_nonuniform"] for shot in result["screenshots"])

    # A still image held for minutes passes every per-frame check above. MK8
    # static once ended on a byte-identical title screen across the last 2212
    # replayed commands and 90s of post-EOF observation, and the run was
    # reported as a pass. Distinct capture hashes are what separates a live
    # scene from a frozen one.
    every_shot = result["screenshots"] + result["post_eof"].get("screenshots", [])
    tail = [shot.get("sha256") for shot in every_shot if shot.get("sha256")]
    frozen = len(tail) >= 3 and len(set(tail[-3:])) == 1
    result["distinct_capture_hashes"] = len(set(tail))
    result["capture_count"] = len(tail)
    result["frozen_tail"] = frozen
    if frozen:
        result["frozen_tail_sha256"] = tail[-1]
    result["rendered_live_content"] = result["rendered_visible_content"] and not frozen
    result["machine_status"] = (
        "REVIEW_REQUIRED" if result["error"] is None and result["tas_outcome"] == "COMPLETE"
        and result["first_frame"]
        and ((not args.baseline and result["static_backend_active"])
             or (args.baseline and not result["static_backend_active"]))
        and result["max_jit_transitions"] == 0 and result["rendered_live_content"]
        and (not args.expected_final_image or result.get("final_image_match") is True)
        and result["stopped_cleanly"] else "FAILED"
    )
    # Equal-work timing. The script is a fixed number of commands, so the first
    # sample that reports the last command is the finish line, and the seconds
    # to reach it are directly comparable between backends - unlike an FPS
    # median over an attract sequence, which depends on which phase each arm
    # happened to catch.
    result.pop("_t0", None)
    result.pop("_allow_jit", None)
    total = result.get("tas_completed_commands") or 0
    eof_t = None
    if total:
        for s in result["states"]:
            if (s.get("tas_completed_commands") or 0) >= total and s.get("t") is not None:
                eof_t = s["t"]
                break
    result["eof_seconds"] = eof_t
    # 60 Hz is the recording rate, so this says how far off real time the run
    # was. Above 1.0 is faster than the console, below is slower.
    result["eof_realtime_ratio"] = (
        round((total / 60.0) / eof_t, 4) if eof_t else None)

    (output / "observation.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if result["machine_status"] == "REVIEW_REQUIRED" else 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] in (
            "select-fixture", "restore-fixture", "describe-fixture"):
        raise SystemExit(fixture_command(sys.argv[1:]))
    raise SystemExit(main())
