"""Mock-host checks for fixture isolation and exact-EOF versus delayed evidence."""

import contextlib
import importlib.util
import io
import json
import pathlib
import tempfile
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "playtest_tas", pathlib.Path(__file__).resolve().parents[1] / "scripts/playtest-tas.py")
TAS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TAS)


class FixtureTests(unittest.TestCase):
    def test_selection_preserves_scripts_and_restores_only_owned_settings(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            original = root / "active"
            fixture = root / "static"
            original.mkdir()
            fixture.mkdir()
            (original / "script0-1.txt").write_bytes(b"0 KEY_A 0;0 0;0\r\n")
            (fixture / "script0-1.txt").write_bytes(b"0 NONE 0;0 0;0\r\n")
            before = TAS.fixture_manifest(original), TAS.fixture_manifest(fixture)
            config = root / "config.ini"
            config.write_text(f"[Data%20Storage]\ntas_directory={original.as_posix()}\n"
                              "tas_directory\\default=true\nunrelated=old\n", encoding="utf-8")
            restore = root / "restore.json"
            TAS.select_fixture(config, fixture, restore)
            self.assertEqual(TAS.tas_directory(config, root), fixture)
            self.assertIn("tas_directory\\default=false", config.read_text())
            config.write_text(config.read_text().replace("unrelated=old", "unrelated=new"))
            TAS.restore_fixture(restore)
            self.assertEqual(TAS.tas_directory(config, root), original)
            self.assertIn("unrelated=new", config.read_text())
            self.assertIn("tas_directory\\default=true", config.read_text())
            self.assertEqual(before, (TAS.fixture_manifest(original), TAS.fixture_manifest(fixture)))
            self.assertTrue(json.loads(restore.read_text())["restored"])

    def test_missing_script_and_conflicting_restore_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            with self.assertRaises(ValueError):
                TAS.fixture_manifest(root)
            config = root / "config.ini"
            config.write_text("[Data Storage]\ntas_directory=old\n")
            restore = root / "restore.json"
            TAS.select_fixture(config, root, restore)
            with self.assertRaises(ValueError):
                TAS.select_fixture(config, root, restore)
            config.write_text("[Data Storage]\ntas_directory=external-change\n")
            with self.assertRaises(RuntimeError):
                TAS.restore_fixture(restore)
            self.assertIn("external-change", config.read_text())
            self.assertTrue(restore.is_file())

    def test_missing_keys_and_final_line_without_newline_restore(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for suffix in ("", "tas_directory=original"):
                with self.subTest(suffix=suffix):
                    config = root / "config.ini"
                    config.write_text("[Data%20Storage]\nkeep=true\n" + suffix)
                    restore = root / ("empty.json" if not suffix else "present.json")
                    TAS.select_fixture(config, root, restore)
                    TAS.restore_fixture(restore)
                    self.assertIn("keep=true", config.read_text().splitlines())
                    self.assertEqual("tas_directory=" in config.read_text(), bool(suffix))
                    if suffix:
                        self.assertIn(suffix, config.read_text().splitlines())


class ReplayTests(unittest.TestCase):
    def run_replay(self, observe=0, bad_final=False, finish="COMPLETE", post_update=None):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            fixture = root / "fixture"
            fixture.mkdir()
            (fixture / "script0-1.txt").write_text("0 NONE 0;0 0;0\n")
            expected = root / "expected.png"
            expected.touch()
            ready = {"game_running": True, "tas_total_frames": 4, "tas_frame": 0,
                     "tas_running": True, "tas_completion_generation": 0,
                     "tas_completed_commands": 0, "tas_completion_looping": False,
                     "first_frame_displayed": True, "static_backend_active": True,
                     "static_blocks": 10, "jit_transitions": 0,
                     "jit_available": False, "guard_v2_ready": True}
            complete = dict(ready, tas_running=False, tas_completion_generation=1,
                            tas_completed_commands=4)
            if finish == "LOOPED":
                complete["tas_completion_looping"] = True
            elif finish == "STOPPED_EARLY":
                complete["tas_completion_generation"] = 0
            elif finish == "ENDED_EARLY":
                complete["tas_completed_commands"] = 3
            states = iter([ready, ready, dict(ready, tas_frame=1), complete])
            idle = dict(complete, **(post_update or {}))
            names, launches = [], []

            def call(name, args=None, timeout=None):
                names.append(name)
                if name == "get_emulator_state":
                    return next(states, idle)
                if name == "launch_game_path":
                    launches.append(args)
                return {}

            def capture(path):
                return {"path": str(path), "visibly_nonuniform": True}

            now = [0.0]

            def sleep(seconds):
                now[0] += seconds

            def image_hash(path):
                return 0 if path == expected or "post-eof" in path.name else 0xFFFF if bad_final else 0

            args = ["synthetic.rom", str(root / "evidence"), "--fixture-directory", str(fixture),
                    "--expected-commands", "4", "--expected-final-image", str(expected),
                    "--observe-after-seconds", str(observe), "--observe-capture-interval", "2"]
            with patch.object(TAS, "call", side_effect=call), \
                    patch.object(TAS, "capture", side_effect=capture), \
                    patch.object(TAS, "difference_hash", side_effect=image_hash), \
                    patch.object(TAS.time, "sleep", side_effect=sleep), \
                    patch.object(TAS.time, "monotonic", side_effect=lambda: now[0]), \
                    contextlib.redirect_stdout(io.StringIO()):
                code = TAS.main(args)
            result = json.loads((root / "evidence/observation.json").read_text())
            self.assertEqual(launches, [{"path": "synthetic.rom", "tas_mode": "playback"}])
            self.assertIn("stop_emulation", names)
            return code, result

    def test_observation_is_opt_in(self):
        code, result = self.run_replay()
        self.assertEqual(code, 0)
        self.assertEqual(result["post_eof"]["outcome"], "NOT_REQUESTED")
        self.assertEqual(result["post_eof"]["screenshots"], [])

    def test_later_good_render_does_not_erase_exact_eof_image_failure(self):
        code, result = self.run_replay(observe=5, bad_final=True)
        self.assertEqual(code, 1)
        self.assertEqual(result["tas_outcome"], "COMPLETE")
        self.assertFalse(result["final_image_match"])
        self.assertEqual(result["post_eof"]["outcome"], "COMPLETE")
        self.assertEqual(result["post_eof"]["elapsed_seconds"], 5)
        self.assertEqual(len(result["post_eof"]["screenshots"]), 3)
        self.assertEqual(len([s for s in result["screenshots"] if s.get("final")]), 1)

    def test_observation_cannot_mask_early_or_looped_completion(self):
        for finish in ("STOPPED_EARLY", "LOOPED", "ENDED_EARLY"):
            with self.subTest(finish=finish):
                code, result = self.run_replay(observe=5, finish=finish)
                self.assertEqual(code, 1)
                self.assertEqual(result["tas_outcome"], finish)
                self.assertEqual(result["post_eof"]["outcome"], "NOT_STARTED")

    def test_jit_transition_during_idle_is_still_a_failure(self):
        code, result = self.run_replay(observe=5, post_update={"jit_transitions": 1})
        self.assertEqual(code, 1)
        self.assertEqual(result["max_jit_transitions"], 1)
        self.assertEqual(result["post_eof"]["outcome"], "FAILED")

    def test_playback_restart_during_idle_is_failure(self):
        code, result = self.run_replay(observe=5, post_update={"tas_running": True})
        self.assertEqual(code, 1)
        self.assertEqual(result["post_eof"]["outcome"], "INTERRUPTED")

    def test_observation_time_is_bounded(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            TAS.main(["synthetic.rom", "unused", "--observe-after-seconds", "181"])


if __name__ == "__main__":
    unittest.main()
