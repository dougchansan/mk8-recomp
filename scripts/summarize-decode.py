#!/usr/bin/env python3
"""Rank survey-decode CSV gaps and disassemble their examples (local output only).

A signature can contain several families. An example names only that encoding;
the title count means affected titles, not titles guaranteed to become runnable.
Usage: python scripts/summarize-decode.py local/library-decode.csv > local/gaps.csv
"""
import argparse
import csv
import struct
import sys

import capstone


def summarize(rows):
    groups = {}
    for row in rows:
        if row["Status"] not in ("GAPS", "CLEAN"):
            continue
        for gap in row["Gaps"].split():
            if gap == "-":
                continue
            parts = gap.split(":")
            if len(parts) != 3:
                raise ValueError("Survey lacks examples; rebuild the probe and re-sweep")
            signature, count, example = int(parts[0], 16), int(parts[1]), int(parts[2], 16)
            if example & 0xFFC00000 != signature or count <= 0:
                raise ValueError("Invalid gap signature, count or example")
            group = groups.setdefault(signature, {"count": 0, "titles": set(), "examples": set()})
            group["count"] += count
            group["titles"].add(row["Path"])
            group["examples"].add(example)
    decoder = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    for signature, group in sorted(groups.items(), key=lambda pair: (-pair[1]["count"], pair[0])):
        examples = []
        for word in sorted(group["examples"]):
            decoded = list(decoder.disasm(struct.pack("<I", word), 0))
            name = f"{decoded[0].mnemonic} {decoded[0].op_str}".strip() if decoded else "undefined"
            examples.append(f"{word:08X} {name}")
        yield {"Signature": f"{signature:08X}", "Titles": len(group["titles"]),
               "Instructions": group["count"], "Examples": "; ".join(examples)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("survey")
    args = parser.parse_args()
    with open(args.survey, encoding="utf-8-sig", newline="") as source:
        rows = list(summarize(csv.DictReader(source)))
    writer = csv.DictWriter(sys.stdout, fieldnames=["Signature", "Titles", "Instructions", "Examples"])
    writer.writeheader()
    writer.writerows(rows)


if __name__ == "__main__":
    main()
