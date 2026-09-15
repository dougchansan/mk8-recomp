"""Run a built code_guard_harness against real success and fatal paths."""
import subprocess
import sys

for mode in ("hosted", "standalone", "mutated", "chain", "unmapped", "missing", "legacy", "v1",
             "hosted-unmapped-zero", "mapped-zero", "special-mapped-zero"):
    result = subprocess.run([sys.argv[1], mode], capture_output=True, text=True)
    if mode in ("hosted", "standalone", "mapped-zero", "special-mapped-zero"):
        assert result.returncode == 0 and "passed" in result.stdout, (mode, result)
    else:
        assert result.returncode != 0, (mode, "unexpected success")
        expected = ("requires a guard-v2 host" if mode in ("missing", "legacy", "v1")
                    else "unsupported code change or unavailable code")
        assert expected in result.stderr, (mode, result.stderr)
        if mode == "mutated":
            assert "0x1008" in result.stderr
        if mode == "chain":
            assert "0x101c" in result.stderr
        if mode == "hosted-unmapped-zero":
            assert result.returncode == 86 and "guard abort before any block effect" in result.stderr
    print(f"{mode}: passed")
