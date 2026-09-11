#!/usr/bin/env python3
"""Install an NSP's NCAs into suyu's NAND, the way "Install Files to NAND" does.

suyu exposes no CLI or MCP tool for this - only firmware and keys have one - so
this replicates the on-disk result directly.

An NSP is a PFS0 container and the container is not encrypted, so its file table
parses without keys. The NCAs inside stay encrypted and are handed to suyu's
KeyManager exactly as before; nothing here decrypts anything.

Layout comes from RegisteredCache::GetRelativePathFromNcaID
(registered_cache.cpp:65-84):

    registered/000000{SHA256(nca_id_bytes)[0]:02X}/{nca_id_hex}.nca

where nca_id_bytes is the 16 raw bytes the hex filename encodes.

  python scripts/install-nsp.py <file.nsp> [--dry-run]
"""

import hashlib
import os
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from importlib import import_module

parse_pfs0 = import_module("nsp-extract".replace("-", "_")) if False else None

# nsp-extract.py has a hyphen, so import it by path rather than by name.
import importlib.util

_spec = importlib.util.spec_from_file_location(
    "nsp_extract", pathlib.Path(__file__).parent / "nsp-extract.py"
)
_mod = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
parse_pfs0 = _mod.parse_pfs0


def suyu_data_dir():
    """Where suyu keeps its NAND, per platform.

    Windows uses %APPDATA%\\suyu; Linux follows XDG, so ~/.local/share/suyu
    unless XDG_DATA_HOME says otherwise. Hardcoding APPDATA meant this script
    could only ever run on the machine it was written on.
    """
    if os.name == "nt":
        return pathlib.Path(os.environ["APPDATA"]) / "suyu"
    xdg = os.environ.get("XDG_DATA_HOME")
    base = pathlib.Path(xdg) if xdg else pathlib.Path.home() / ".local" / "share"
    return base / "suyu"


def registered_dir():
    return suyu_data_dir() / "nand" / "user" / "Contents" / "registered"


def nca_relative_path(nca_id_hex):
    raw = bytes.fromhex(nca_id_hex)
    if len(raw) != 16:
        raise ValueError(f"bad NCA id: {nca_id_hex}")
    bucket = hashlib.sha256(raw).digest()[0]
    return pathlib.Path(f"000000{bucket:02X}") / f"{nca_id_hex}.nca"


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    nsp = pathlib.Path(sys.argv[1])
    dry = "--dry-run" in sys.argv

    root = registered_dir()
    print(f"nsp       : {nsp.name}")
    print(f"registered: {root}")
    print()

    files = parse_pfs0(nsp)
    ncas = [(n, o, s) for (n, o, s) in files if n.endswith(".nca")]
    others = [(n, o, s) for (n, o, s) in files if not n.endswith(".nca")]

    with open(nsp, "rb") as f:
        for name, off, size in ncas:
            # ".cnmt.nca" keeps that suffix in the stored name; the id is the stem.
            nca_id_hex = name.split(".")[0]
            rel = nca_relative_path(nca_id_hex)
            if name.endswith(".cnmt.nca"):
                rel = rel.with_name(f"{nca_id_hex}.cnmt.nca")
            dst = root / rel
            print(f"{size:>13,}  {name}  ->  {rel}")
            if dry:
                continue
            if dst.exists() and dst.stat().st_size == size:
                print("               already installed, skipping")
                continue
            dst.parent.mkdir(parents=True, exist_ok=True)
            tmp = dst.with_suffix(".nca.part")
            f.seek(off)
            remaining = size
            with open(tmp, "wb") as g:
                while remaining:
                    chunk = f.read(min(1 << 24, remaining))
                    if not chunk:
                        raise IOError(f"short read on {name}")
                    g.write(chunk)
                    remaining -= len(chunk)
            shutil.move(tmp, dst)

    if others:
        print("\nnot installed (ticket/cert - suyu reads title keys from title.keys):")
        for name, _, size in others:
            print(f"  {size:>9,}  {name}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
