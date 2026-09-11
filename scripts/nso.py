#!/usr/bin/env python3
"""Read an NSO0 module into a flat image addressed by module-relative VA.
"""

import hashlib
import struct
import sys


def lz4_block_decompress(src: bytes, out_size: int) -> bytes:
    """LZ4 block format (not frame). Raises on a malformed stream."""
    dst = bytearray(out_size)
    s = 0
    d = 0
    n = len(src)
    while s < n:
        token = src[s]
        s += 1

        lit_len = token >> 4
        if lit_len == 15:
            while True:
                b = src[s]
                s += 1
                lit_len += b
                if b != 255:
                    break
        if lit_len:
            dst[d:d + lit_len] = src[s:s + lit_len]
            s += lit_len
            d += lit_len

        if s >= n:
            break

        offset = src[s] | (src[s + 1] << 8)
        s += 2
        if offset == 0:
            raise ValueError(f"zero match offset at src {s}")

        match_len = token & 0x0F
        if match_len == 15:
            while True:
                b = src[s]
                s += 1
                match_len += b
                if b != 255:
                    break
        match_len += 4

        # Overlapping copies are legal, so this cannot be a slice.
        m = d - offset
        for i in range(match_len):
            dst[d + i] = dst[m + i]
        d += match_len

    if d != out_size:
        raise ValueError(f"decompressed {d} bytes, header said {out_size}")
    return bytes(dst)


class Segment:
    def __init__(self, name, vaddr, data):
        self.name = name
        self.vaddr = vaddr
        self.data = data

    @property
    def end(self):
        return self.vaddr + len(self.data)

    def __repr__(self):
        return f"<{self.name} {self.vaddr:#x}..{self.end:#x} ({len(self.data)} B)>"


class Nso:
    """A decompressed NSO, addressable by module-relative VA."""

    def __init__(self, path):
        self.path = str(path)
        with open(path, "rb") as f:
            blob = f.read()

        if blob[:4] != b"NSO0":
            raise ValueError(f"{path}: not an NSO0 (magic {blob[:4]!r})")

        flags, = struct.unpack_from("<I", blob, 0x0C)
        self.build_id = blob[0x40:0x60].hex()

        segs = []
        for i, (name, hdr_off, csize_off) in enumerate((
            ("text", 0x10, 0x60),
            ("rodata", 0x20, 0x64),
            ("data", 0x30, 0x68),
        )):
            file_off, vaddr, dsize = struct.unpack_from("<III", blob, hdr_off)
            csize, = struct.unpack_from("<I", blob, csize_off)
            raw = blob[file_off:file_off + csize]
            if flags & (1 << i):
                data = lz4_block_decompress(raw, dsize)
            else:
                data = raw[:dsize]
            if flags & (1 << (i + 3)):
                want = blob[0xA0 + i * 0x20:0xC0 + i * 0x20]
                got = hashlib.sha256(data).digest()
                if want != got:
                    raise ValueError(f"{path}: {name} hash mismatch")
            segs.append(Segment(name, vaddr, data))

        self.text, self.rodata, self.data = segs
        self.segments = segs
        self.bss_size, = struct.unpack_from("<I", blob, 0x3C)

    def read(self, vaddr, size):
        for s in self.segments:
            if s.vaddr <= vaddr and vaddr + size <= s.end:
                off = vaddr - s.vaddr
                return s.data[off:off + size]
        return None

    def flat(self):
        """One contiguous buffer from VA 0 to the end of .data."""
        end = max(s.end for s in self.segments)
        buf = bytearray(end)
        for s in self.segments:
            buf[s.vaddr:s.end] = s.data
        return bytes(buf)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    for path in argv[1:]:
        nso = Nso(path)
        print(f"{path}")
        print(f"  build id {nso.build_id}")
        for s in nso.segments:
            print(f"  {s}")
        print(f"  bss {nso.bss_size:#x}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
