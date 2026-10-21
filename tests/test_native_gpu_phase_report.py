import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "phase", Path(__file__).parents[1] / "scripts/native_gpu_phase_report.py"
)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


def group(submit=10, part=0, ready=1, complete=1, dropped=0):
    prefix = f"submit {submit} present 0 slot 2 part {part} sample 3"
    if not ready:
        return f"measure gpu phase: {prefix} ready 0 complete 0 segments 0 dropped 0 duration 0 ticks 0\n"
    return (
        f"measure gpu phase: {prefix} ready 1 complete {complete} segments 2 dropped {dropped} duration 300 ticks 30\n"
        f"measure gpu phase segment: {prefix} index 0 duration 100 ticks 10 label 1280x720 L0 fs first-shader\n"
        f"measure gpu phase segment: {prefix} index 1 duration 200 ticks 20 label texture uploads\n"
    )


def coarse(submit=10, part=0, ready=1):
    return f"measure gpu: submit {submit} present 0 slot 2 part {part} ready {ready} duration 400\n"


class PhaseReportTests(unittest.TestCase):
    def test_counter_components_remain_separate(self):
        text = (
            (coarse() + group())
            .replace("1280x720 L0 fs first-shader", "counter query copy")
            .replace("texture uploads", "counter reduction")
        )
        result = m.analyze(text)
        self.assertEqual(result["status"], "available")
        self.assertEqual(
            {row["label"] for row in result["categories"]},
            {"counter_copy", "counter_reduce"},
        )

    def test_windows_line_endings_preserve_headers_and_labels(self):
        text = (coarse() + group()).replace("\n", "\r\n")
        result = m.analyze(text)
        self.assertEqual(result["status"], "available")
        self.assertEqual(result["orphan_segments"], 0)
        self.assertEqual(result["qualified_sampled_submits"], 1)
        self.assertEqual(result["categories"][0]["label"], "upload")

    def test_cli_reads_windows_bytes(self):
        with tempfile.TemporaryDirectory(prefix="native-phase-report-") as folder:
            case = Path(folder)
            (case / "native.log").write_bytes(
                (coarse() + group()).replace("\n", "\r\n").encode()
            )
            output = subprocess.run(
                [sys.executable, str(Path(m.__file__)), str(case)],
                capture_output=True,
                text=True,
                check=True,
            )
            result = json.loads(output.stdout)
            self.assertEqual(result["status"], "available")
            self.assertEqual(result["qualified_sampled_submits"], 1)

    def test_missing_is_not_zero(self):
        r = m.analyze(coarse())
        self.assertEqual(r["status"], "missing")
        self.assertIsNone(r["sampled_submit_ms"])
        self.assertEqual(r["categories"], [])

    def test_complete_sample_uses_sample_count(self):
        r = m.analyze(coarse() + group() + coarse(11))
        self.assertEqual(r["status"], "available")
        self.assertEqual(r["qualified_sampled_submits"], 1)
        self.assertEqual(r["sampled_submit_ms"]["mean"], 0.0003)
        self.assertEqual(r["categories"][0]["label"], "upload")
        self.assertEqual(r["categories"][0]["mean_ms_per_sampled_submit"], 0.0002)

    def test_delayed_retirement_stays_in_selected_submit(self):
        r = m.analyze(coarse() + coarse(11) + group(11) + group(), {10})
        self.assertEqual(r["reported_parts"], 1)
        self.assertEqual(r["status"], "available")

    def test_unavailable_is_not_zero(self):
        r = m.analyze(coarse() + group(ready=0))
        self.assertEqual(r["status"], "incomplete")
        self.assertEqual(r["unavailable_parts"], 1)
        self.assertIsNone(r["sampled_submit_ms"])

    def test_capped_parts_do_not_become_full_samples(self):
        r = m.analyze(coarse() + group(complete=0, dropped=7))
        self.assertEqual(r["capped_parts"], 1)
        self.assertEqual(r["status"], "incomplete")
        self.assertEqual(r["qualified_sampled_submits"], 0)

    def test_missing_segment(self):
        r = m.analyze(coarse() + "\n".join(group().splitlines()[:-1]))
        self.assertEqual(r["invalid_parts"], 1)
        self.assertEqual(r["labels"], [])

    def test_duplicate_segment_not_double_counted(self):
        line = group().splitlines()[1] + "\n"
        r = m.analyze(coarse() + group() + line)
        self.assertEqual(r["status"], "incomplete")
        self.assertEqual(r["qualified_sampled_submits"], 0)

    def test_duplicate_header_not_double_counted(self):
        r = m.analyze(coarse() + group() + group().splitlines()[0] + "\n")
        self.assertEqual(r["invalid_parts"], 1)
        self.assertIsNone(r["sampled_submit_ms"])

    def test_orphan_segment_explicit(self):
        r = m.analyze(group().splitlines()[1] + "\n")
        self.assertEqual(r["orphan_segments"], 1)
        self.assertEqual(r["status"], "incomplete")

    def test_nonconserving_ticks(self):
        r = m.analyze(coarse() + group().replace("ticks 30", "ticks 31"))
        self.assertTrue(any(e["kind"] == "tick_nonconservation" for e in r["errors"]))
        self.assertEqual(r["qualified_sampled_submits"], 0)

    def test_all_parts_required(self):
        r = m.analyze(coarse() + coarse(part=1) + group())
        self.assertEqual(r["missing_sample_part_sets"], 1)
        self.assertEqual(r["qualified_sampled_submits"], 0)
        full = m.analyze(coarse() + coarse(part=1) + group() + group(part=1))
        self.assertEqual(full["qualified_sampled_submits"], 1)
        self.assertEqual(full["sampled_submit_ms"]["mean"], 0.0006)

    def test_ambiguous_coarse_context_rejected(self):
        r = m.analyze(coarse() + coarse() + group())
        self.assertEqual(r["qualified_sampled_submits"], 0)
        self.assertTrue(
            any(e["kind"] == "ambiguous_coarse_records" for e in r["errors"])
        )

    def test_unavailable_coarse_rejected(self):
        r = m.analyze(coarse(ready=0) + group())
        self.assertEqual(r["status"], "incomplete")
        self.assertIsNone(r["sampled_submit_ms"])

    def test_stream_cap_explicit(self):
        r = m.analyze(coarse() + group() + "measure: truncated limit 2\n")
        self.assertTrue(r["truncated"])
        self.assertEqual(r["status"], "incomplete")


if __name__ == "__main__":
    unittest.main()
