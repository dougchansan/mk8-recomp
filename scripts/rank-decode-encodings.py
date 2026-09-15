#!/usr/bin/env python3
"""Rank the local probe's exact encoding TSV by mnemonic.

Optional --decoder rechecks captured misses with the current emitter. This is
not a full resurvey: newly introduced misses in previously decoded instructions
require a fresh library sweep. Undefined words and UDF stay visible.

Write outputs under local/; this report contains guest instruction encodings.
"""
import argparse
import csv
import re
import struct
import subprocess
import sys

import capstone


def load_sites(source):
    sites = []
    for fields in csv.reader(source, delimiter="\t"):
        if not fields:
            continue
        if len(fields) != 4:
            raise ValueError("Expected title ID, encoding, count, path")
        title, word, count, path = fields
        word, count = int(word, 16), int(count)
        if not 0 <= word <= 0xFFFFFFFF or count <= 0 or not path:
            raise ValueError("Invalid encoding site")
        sites.append((path, word, count))
    return sites


def remaining_words(decoder, words):
    words = set(words)
    result = subprocess.run([decoder, "--all"],
                            input="".join(f"{word:08X}\n" for word in sorted(words)),
                            text=True, capture_output=True, check=True)
    flags = {}
    for line in result.stdout.splitlines():
        word, translated = line.split()
        word = int(word, 16)
        if word in flags or translated not in ("0", "1"):
            raise ValueError("Invalid decoder output")
        flags[word] = translated == "1"
    if flags.keys() != words:
        raise ValueError("Incomplete decoder output")
    return {word for word, translated in flags.items() if not translated}


def rank_sites(sites, remaining=None):
    disassembler = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    decoded = {}
    groups = {}
    for path, word, count in sites:
        if remaining is not None and word not in remaining:
            continue
        if word not in decoded:
            instruction = next(disassembler.disasm(struct.pack("<I", word), 0), None)
            decoded[word] = ((instruction.mnemonic, instruction.op_str)
                             if instruction else ("undefined", ""))
        mnemonic, operands = decoded[word]
        group = groups.setdefault(mnemonic, {"count": 0, "titles": set(), "examples": {}})
        group["count"] += count
        group["titles"].add(path)
        group["examples"][word] = operands
    for mnemonic, group in sorted(groups.items(), key=lambda item: (-item[1]["count"], item[0])):
        word = min(group["examples"])
        yield {"Mnemonic": mnemonic, "Instructions": group["count"],
               "Titles": len(group["titles"]),
               "Example": f"{word:08X} {mnemonic} {group['examples'][word]}".strip()}


def title_summary(sites, remaining):
    """Keep traps and unsupported target extensions visible, not counted as implemented.

    The emulator targets Cortex-A57 (Armv8.0); its Dynarmic decoder disables
    LSE and has no SVE register state. SVE register operands identify the
    observed SVE encodings here; this is not a general ISA feature detector.
    """
    decoder = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_LITTLE_ENDIAN)
    categories = {}
    titles = {}
    lse = re.compile(r"^(?:casp?|swp|ld(?:add|clr|eor|set|smax|smin|umax|umin))(?:al|a|l)?[bh]?$")
    sve_register = re.compile(r"\b(?:z\d+|p\d+|za\d*)\b")
    fields = ("BaselineUnhandled", "TargetExtensionWords", "ConstrainedUnpredictableWords",
              "TrapWords", "UndefinedWords")
    for path, word, count in sites:
        row = titles.setdefault(path, {"Path": path, **dict.fromkeys(fields, 0)})
        if word not in remaining:
            continue
        if word not in categories:
            instruction = next(decoder.disasm(struct.pack("<I", word), 0), None)
            if instruction is None:
                category = "UndefinedWords"
            elif instruction.mnemonic == "udf":
                category = "TrapWords"
            elif instruction.mnemonic == "stxrb" and ((word >> 10) & 31) != 31:
                # Rt2 is Should-Be-One; violating it is CONSTRAINED
                # UNPREDICTABLE, with UNDEFINED an allowed outcome (Arm C2.2.2).
                category = "ConstrainedUnpredictableWords"
            elif lse.fullmatch(instruction.mnemonic) or sve_register.search(instruction.op_str):
                category = "TargetExtensionWords"
            else:
                category = "BaselineUnhandled"
            categories[word] = category
        row[categories[word]] += count
    return sorted(titles.values(), key=lambda row: (-row["BaselineUnhandled"], row["Path"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("histogram")
    parser.add_argument("--decoder")
    parser.add_argument("--per-title", action="store_true", help="show separate residual categories by title")
    args = parser.parse_args()
    with open(args.histogram, encoding="utf-8-sig", newline="") as source:
        sites = load_sites(source)
    remaining = remaining_words(args.decoder, (site[1] for site in sites)) if args.decoder else None
    if args.per_title:
        rows = title_summary(sites, remaining if remaining is not None else {site[1] for site in sites})
        writer = csv.DictWriter(sys.stdout, ["Path", "BaselineUnhandled", "TargetExtensionWords",
                                             "ConstrainedUnpredictableWords", "TrapWords",
                                             "UndefinedWords"], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
        return
    writer = csv.DictWriter(sys.stdout, ["Mnemonic", "Instructions", "Titles", "Example"],
                            lineterminator="\n")
    writer.writeheader()
    writer.writerows(rank_sites(sites, remaining))


if __name__ == "__main__":
    main()
