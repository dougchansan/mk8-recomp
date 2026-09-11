#!/usr/bin/env python3
"""Tests for the NSO reader and the AArch64 xref scanner.

Fixtures are synthesised, not committed.

  python tests/test_tools.py
"""

import hashlib
import pathlib
import struct
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))

import numpy as np

from aarch64_xref import build_xrefs
from nso import Nso, lz4_block_decompress


def lz4_literal_block(payload: bytes) -> bytes:
    """The trivial LZ4 encoding: one token, all literals, no matches."""
    out = bytearray()
    n = len(payload)
    if n < 15:
        out.append(n << 4)
    else:
        out.append(0xF0)
        rest = n - 15
        while rest >= 255:
            out.append(255)
            rest -= 255
        out.append(rest)
    out += payload
    return bytes(out)


def build_nso(text: bytes, rodata: bytes, data: bytes, build_id: bytes) -> bytes:
    """A minimal but header-accurate NSO0."""
    segs = [text, rodata, data]
    vaddrs = [0x0, 0x10000, 0x20000]
    blobs = [lz4_literal_block(s) for s in segs]

    header = bytearray(0x100)
    header[0x00:0x04] = b"NSO0"
    struct.pack_into("<I", header, 0x0C, 0x3F)  # all compressed, all hashed
    header[0x40:0x60] = build_id

    file_off = 0x100
    for i, (seg, blob, vaddr) in enumerate(zip(segs, blobs, vaddrs)):
        struct.pack_into("<III", header, 0x10 + i * 0x10, file_off, vaddr, len(seg))
        struct.pack_into("<I", header, 0x60 + i * 4, len(blob))
        header[0xA0 + i * 0x20:0xC0 + i * 0x20] = hashlib.sha256(seg).digest()
        file_off += len(blob)

    return bytes(header) + b"".join(blobs)


class TestLz4(unittest.TestCase):
    def test_round_trips_literals(self):
        payload = bytes(range(256)) * 4
        self.assertEqual(lz4_block_decompress(lz4_literal_block(payload), len(payload)),
                         payload)

    def test_overlapping_match_repeats_the_source(self):
        block = bytes([(1 << 4) | 5]) + b"A" + struct.pack("<H", 1)
        self.assertEqual(lz4_block_decompress(block, 10), b"A" * 10)

    def test_short_output_is_an_error(self):
        with self.assertRaises(ValueError):
            lz4_block_decompress(lz4_literal_block(b"12345"), 9)


class TestNso(unittest.TestCase):
    def setUp(self):
        self.build_id = bytes(range(32))
        self.text = b"\x1f\x20\x03\xd5" * 8          # NOP x8
        self.rodata = b"hello world\x00padding\x00"
        self.data = b"\xaa" * 64
        self.path = pathlib.Path(__file__).with_name("_fixture.nso")
        self.path.write_bytes(build_nso(self.text, self.rodata, self.data, self.build_id))

    def tearDown(self):
        self.path.unlink(missing_ok=True)

    def test_segments_decompress_with_correct_addresses(self):
        nso = Nso(self.path)
        self.assertEqual(nso.build_id, self.build_id.hex())
        self.assertEqual(nso.text.data, self.text)
        self.assertEqual(nso.rodata.vaddr, 0x10000)
        self.assertEqual(nso.read(0x10000, 5), b"hello")

    def test_a_corrupt_segment_is_rejected(self):
        blob = bytearray(self.path.read_bytes())
        blob[0xA0] ^= 0xFF                            # break the .text hash
        self.path.write_bytes(blob)
        with self.assertRaises(ValueError):
            Nso(self.path)

    def test_flat_image_is_addressed_by_vaddr(self):
        flat = Nso(self.path).flat()
        self.assertEqual(flat[0x10000:0x10005], b"hello")


class TestXref(unittest.TestCase):
    def encode_adrp(self, rd, pc, target_page):
        delta = (target_page & ~0xFFF) - (pc & ~0xFFF)
        imm = (delta >> 12) & 0x1FFFFF
        return (0x90000000 | ((imm & 3) << 29) | (((imm >> 2) & 0x7FFFF) << 5) | rd)

    def encode_add_imm(self, rd, rn, imm12):
        return 0x91000000 | (imm12 << 10) | (rn << 5) | rd

    def test_finds_an_adrp_add_pair(self):
        pc = 0x1000
        target = 0x40123
        words = [
            self.encode_adrp(2, pc, target),
            self.encode_add_imm(2, 2, target & 0xFFF),
        ]
        text = b"".join(struct.pack("<I", w) for w in words)
        xrefs = build_xrefs(text, pc)
        self.assertIn(target, xrefs)
        self.assertEqual(xrefs[target], [pc + 4])

    def test_handles_a_negative_page_delta(self):
        pc = 0x800000
        target = 0x1000
        words = [
            self.encode_adrp(9, pc, target),
            self.encode_add_imm(9, 9, target & 0xFFF),
        ]
        text = b"".join(struct.pack("<I", w) for w in words)
        self.assertIn(target, build_xrefs(text, pc))

    def test_ignores_an_add_on_an_unrelated_register(self):
        pc = 0x1000
        target = 0x40123
        words = [
            self.encode_adrp(2, pc, target),
            self.encode_add_imm(5, 7, target & 0xFFF),
        ]
        text = b"".join(struct.pack("<I", w) for w in words)
        self.assertEqual(build_xrefs(text, pc), {})


if __name__ == "__main__":
    unittest.main(verbosity=2)
