"""Synthetic checks for exact encoding aggregation and decoder protocol."""
import io
import pathlib
import runpy
import subprocess
import unittest
from unittest.mock import patch

MODULE = runpy.run_path(str(pathlib.Path(__file__).resolve().parents[1] /
                           "scripts/rank-decode-encodings.py"))


class RankDecodeTests(unittest.TestCase):
    def test_counts_and_titles(self):
        sites = MODULE["load_sites"](io.StringIO(
            "0\t4EA29420\t4\ta\n0\t4EA29420\t7\tb\n0\t00000000\t2\tb\n"))
        result = list(MODULE["rank_sites"](sites))
        self.assertEqual((result[0]["Mnemonic"], result[0]["Instructions"], result[0]["Titles"]),
                         ("mla", 11, 2))
        self.assertEqual(list(MODULE["rank_sites"](sites, {0}))[0]["Mnemonic"], "udf")
        self.assertEqual(list(MODULE["rank_sites"](sites, set())), [])

    def test_malformed_sites(self):
        for text in ("0\t0\t0\ta", "0\t100000000\t1\ta", "0\t0\t1", "0\t0\t1\t"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                MODULE["load_sites"](io.StringIO(text))

    def test_categories_do_not_hide_residual_words(self):
        sites = [("a", 0, 3), ("a", 0xFFFFFFFF, 2), ("a", 0x4EA29420, 4)]
        row = MODULE["title_summary"](sites, {0, 0xFFFFFFFF, 0x4EA29420})[0]
        self.assertEqual((row["TrapWords"], row["UndefinedWords"], row["BaselineUnhandled"]), (3, 2, 4))
        clean = MODULE["title_summary"](sites, set())[0]
        self.assertEqual(sum(v for k, v in clean.items() if k != "Path"), 0)

    def test_should_be_one_violation_remains_visible(self):
        row = MODULE["title_summary"]([("synthetic", 0x08000000, 1)], {0x08000000})[0]
        self.assertEqual(row["ConstrainedUnpredictableWords"], 1)
        self.assertEqual(row["BaselineUnhandled"], 0)

    def test_decoder_protocol(self):
        with patch.object(MODULE["subprocess"], "run") as run:
            run.return_value = subprocess.CompletedProcess([], 0, "00000000\t0\n4EA29420\t1\n")
            self.assertEqual(MODULE["remaining_words"]("decoder", [0, 0x4EA29420]), {0})
            self.assertEqual(run.call_args.args[0], ["decoder", "--all"])

    def test_incomplete_or_ambiguous_decoder_results_fail(self):
        for output in ("", "00000000\t0\n00000000\t1\n", "00000000\t2\n", "00000001\t0\n"):
            with self.subTest(output=output), patch.object(MODULE["subprocess"], "run") as run:
                run.return_value = subprocess.CompletedProcess([], 0, output)
                with self.assertRaises(ValueError):
                    MODULE["remaining_words"]("decoder", [0])


if __name__ == "__main__":
    unittest.main()
