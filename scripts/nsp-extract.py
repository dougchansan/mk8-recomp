#!/usr/bin/env python3
"""Extract the NCAs from an NSP.

An NSP is a PFS0 container. The container itself is not encrypted - only the
NCAs inside it are - so the file table can be parsed without any keys, which is
what "Install to NAND" does before handing the NCAs to KeyManager.

  python scripts/nsp-extract.py <file.nsp> [out_dir]

With no out_dir it only lists the contents.
"""
import pathlib
import struct
import sys


def parse_pfs0(path):
    with open(path, "rb") as f:
        magic, count, strtab_size, _ = struct.unpack("<4sIII", f.read(0x10))
        if magic != b"PFS0":
            raise ValueError(f"not a PFS0 container: magic={magic!r}")
        entries = [struct.unpack("<QQII", f.read(0x18)) for _ in range(count)]
        strtab = f.read(strtab_size)
        data_start = 0x10 + count * 0x18 + strtab_size
        out = []
        for offset, size, name_off, _ in entries:
            end = strtab.find(b"\0", name_off)
            name = strtab[name_off:end].decode("utf-8")
            out.append((name, data_start + offset, size))
        return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    nsp = pathlib.Path(sys.argv[1])
    files = parse_pfs0(nsp)
    print(f"{nsp.name}: {len(files)} entries")
    for name, off, size in files:
        print(f"  {size:>13,}  {name}")

    if len(sys.argv) < 3:
        return 0

    out_dir = pathlib.Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    with open(nsp, "rb") as f:
        for name, off, size in files:
            dst = out_dir / name
            f.seek(off)
            remaining = size
            with open(dst, "wb") as g:
                while remaining:
                    chunk = f.read(min(1 << 24, remaining))
                    if not chunk:
                        raise IOError(f"short read on {name}")
                    g.write(chunk)
                    remaining -= len(chunk)
            print(f"wrote {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
