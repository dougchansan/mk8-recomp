#!/usr/bin/env python3
"""Name the emitter's decode gap, by mnemonic, over a real instruction stream.

The emitter reports what it missed as encoding-group buckets and raw signatures,
which sizes the gap but does not say what is in it. This runs every distinct
encoding in a title's .text through the emitter (tests/decode_sweep.cpp) and
disassembles the ones that fall through, so the answer comes out as "ld1 2105,
tbl 658, ..." rather than "load/store 2591".

The denominator is the binary rather than the whole instruction set on purpose:
an encoding the title never contains is not a gap worth closing, and the ISA has
far more of those than it has real ones.

  python scripts/decode-coverage.py <exefs-dir> [--sweep build/decode_sweep.exe]

Static counts are not execution counts, and the two orders differ sharply - an
instruction appearing once in a hot loop costs more than a thousand in cold
setup code. Use this to decide what is *missing*; use the runtime coverage
report to decide what is *expensive*.
"""

import argparse
import collections
import pathlib
import struct
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from nso import Nso  # noqa: E402

try:
    import capstone
except ImportError:
    sys.exit("capstone is required: pip install capstone")
try:
    import numpy as np
except ImportError:
    sys.exit("numpy is required: pip install numpy")

MODULES = ("main", "sdk", "subsdk0", "subsdk1", "subsdk2", "subsdk3", "subsdk4", "rtld")


def distinct_encodings(exefs):
    counts = collections.Counter()
    found = []
    for name in MODULES:
        path = pathlib.Path(exefs) / name
        if not path.is_file():
            continue
        found.append(name)
        words = np.frombuffer(Nso(path).text.data, dtype="<u4")
        values, seen = np.unique(words, return_counts=True)
        for enc, n in zip(values.tolist(), seen.tolist()):
            counts[enc] += n
    if not found:
        sys.exit(f"no modules found under {exefs}")
    return counts, found


def sweep(sweeper, encodings):
    payload = "".join(f"{e:08X}\n" for e in encodings)
    result = subprocess.run([str(sweeper)], input=payload, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"{sweeper} failed: {result.stderr.strip()}")
    handled = {}
    for line in result.stdout.splitlines():
        enc, ok = line.split("\t")
        handled[int(enc, 16)] = ok == "1"
    return handled


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exefs", help="directory holding the decompressed NSO modules")
    ap.add_argument("--sweep", default="build/decode_sweep.exe",
                    help="path to the built tests/decode_sweep binary")
    ap.add_argument("--top", type=int, default=25)
    args = ap.parse_args()

    counts, modules = distinct_encodings(args.exefs)
    total = sum(counts.values())
    print(f"modules            : {', '.join(modules)}")
    print(f"instructions       : {total:,}")
    print(f"distinct encodings : {len(counts):,}")

    handled = sweep(args.sweep, counts)

    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    by_mnemonic = collections.Counter()
    for enc, n in counts.items():
        if handled.get(enc, False):
            continue
        decoded = list(md.disasm(struct.pack("<I", enc), 0x1000))
        # Anything the disassembler also rejects is data sitting in .text -
        # literal pools and padding - not a gap in the emitter.
        by_mnemonic[decoded[0].mnemonic if decoded else "(not an instruction)"] += n

    gap = sum(by_mnemonic.values())
    print(f"unhandled          : {gap:,} ({100.0 * gap / total:.3f}%)")
    print(f"distinct mnemonics : {len(by_mnemonic)}")

    running = 0
    for i, (_, n) in enumerate(by_mnemonic.most_common(), 1):
        running += n
        if gap and running / gap >= 0.90:
            print(f"90% of the gap is   : {i} mnemonics")
            break

    print(f"\n{'mnemonic':<16}{'count':>9}{'share':>8}")
    for mnemonic, n in by_mnemonic.most_common(args.top):
        print(f"{mnemonic:<16}{n:>9,}{100.0 * n / gap:>7.1f}%")


if __name__ == "__main__":
    main()
