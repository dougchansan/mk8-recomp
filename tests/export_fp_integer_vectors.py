"""Export synthetic integer-conversion oracles on Linux x86 with GCC.

From the repository root:
  python3 tests/export_fp_integer_vectors.py local/fp-integer-vectors.txt

On Windows generate C from fp_integer_more_test.cpp as usual, then compile that
C with /DFP_INTEGER_REPLAY. Run the resulting executable with the vectors path
as its sole argument. The replay contains no floating-point reference code.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    cxx = shlex.split(os.environ.get("CXX", "g++"))
    cc = shlex.split(os.environ.get("CC", "gcc"))
    with tempfile.TemporaryDirectory(prefix="fp-integer-oracle-") as directory:
        work = Path(directory)
        generator, source = work / "generator", work / "test.c"
        subprocess.run(cxx + ["-std=c++20", "-I" + str(root / "third_party/suyu/src"),
                              str(root / "tests/fp_integer_more_test.cpp"),
                              "-o", str(generator)], check=True)
        with source.open("wb") as output:
            subprocess.run([str(generator)], stdout=output, check=True)
        common = cc + ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                       "-frounding-math", "-fsanitize=undefined", str(source), "-lm"]
        exporter, replay, vectors = work / "export", work / "replay", work / "vectors.txt"
        subprocess.run(common + ["-DFP_INTEGER_VECTOR_EXPORT", "-o", str(exporter)], check=True)
        with vectors.open("wb") as output:
            subprocess.run([str(exporter)], stdout=output, check=True)
        subprocess.run(common + ["-DFP_INTEGER_REPLAY", "-o", str(replay)], check=True)
        subprocess.run([str(replay), str(vectors)], check=True)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(vectors, args.output)
    print(f"{args.output}: SHA256 {hashlib.sha256(args.output.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
