#!/usr/bin/env python3
"""Find the code that references a given address in an AArch64 NSO.

  python scripts/aarch64_xref.py <module.bin> --string "foo"
  python scripts/aarch64_xref.py <module.bin> --addr 0xb56000
"""

import argparse
import pathlib
import struct
import sys

import numpy as np


def sext(value, bits):
    sign = 1 << (bits - 1)
    return (value ^ sign) - sign


def build_xrefs(text, text_vaddr):
    words = np.frombuffer(text, dtype="<u4")
    n = len(words)

    is_adrp = (words & 0x9F000000) == 0x90000000
    is_addi = (words & 0x7F800000) == 0x11000000      # ADD (immediate), any size
    is_ldru = (words & 0x3B000000) == 0x39000000      # LDR/STR unsigned offset

    adrp_idx = np.flatnonzero(is_adrp)
    immhi = (words[adrp_idx] >> 5) & 0x7FFFF
    immlo = (words[adrp_idx] >> 29) & 0x3
    imm = ((immhi.astype(np.int64) << 2) | immlo)
    imm = np.where(imm >= (1 << 20), imm - (1 << 21), imm) << 12
    adrp_pc = text_vaddr + adrp_idx.astype(np.int64) * 4
    adrp_target = (adrp_pc & ~0xFFF) + imm
    adrp_rd = words[adrp_idx] & 0x1F

    page_of = {}
    for i, rd, tgt in zip(adrp_idx.tolist(), adrp_rd.tolist(), adrp_target.tolist()):
        page_of[i] = (rd, tgt)

    xrefs = {}
    live = {}
    use_idx = np.flatnonzero(is_addi | is_ldru)
    interesting = sorted(set(adrp_idx.tolist()) | set(use_idx.tolist()))
    for i in interesting:
        w = int(words[i])
        if i in page_of:
            rd, tgt = page_of[i]
            live[rd] = (i, tgt)
            continue
        rn = (w >> 5) & 0x1F
        base = live.get(rn)
        if base is None or i - base[0] > 24:
            continue
        if (w & 0x7F800000) == 0x11000000:            # ADD (immediate)
            shift = (w >> 22) & 1
            imm12 = (w >> 10) & 0xFFF
            target = base[1] + (imm12 << (12 if shift else 0))
            rd = w & 0x1F
        else:                                          # LDR/STR unsigned offset
            size = (w >> 30) & 3
            imm12 = (w >> 10) & 0xFFF
            target = base[1] + (imm12 << size)
            rd = w & 0x1F
        xrefs.setdefault(int(target), []).append(text_vaddr + i * 4)
        live.pop(rd, None)
    return xrefs


def main():
    sys.path.insert(0, str(pathlib.Path(__file__).parent))
    from nso import Nso

    ap = argparse.ArgumentParser()
    ap.add_argument("module")
    ap.add_argument("--string", action="append", default=[])
    ap.add_argument("--addr", action="append", default=[])
    args = ap.parse_args()

    nso = Nso(args.module)
    xrefs = build_xrefs(nso.text.data, nso.text.vaddr)
    print(f"{args.module}: build {nso.build_id[:32]}, {len(xrefs)} distinct referenced addresses")

    targets = []
    for s in args.string:
        needle = s.encode() + bytes(1)
        for seg in (nso.rodata, nso.data):
            start = 0
            while True:
                at = seg.data.find(needle, start)
                if at < 0:
                    break
                targets.append((f"{s!r} @ {seg.name}", seg.vaddr + at))
                start = at + 1
    for a in args.addr:
        targets.append((a, int(a, 0)))

    for label, addr in targets:
        hits = xrefs.get(addr, [])
        print(f"{label}: {addr:#x} -> {len(hits)} xref(s)")
        for h in hits[:20]:
            print(f"    {h:#x}")


if __name__ == "__main__":
    main()
