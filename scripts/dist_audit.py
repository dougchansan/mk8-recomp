#!/usr/bin/env python3
"""Check repository and candidate source archives for prohibited distribution content."""
import argparse
import re
import stat
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path, PurePosixPath

LIMIT = 24 * 1024 * 1024
TOTAL_LIMIT = 128 * 1024 * 1024
BLOCK_DIR = {"keys", "firmware", "generated", "traces", "targets", "exefs", "romfs"}
BLOCK_EXT = {".xci", ".nsp", ".nca", ".nso", ".nro", ".ncz", ".nsz",
             ".xcz", ".tik", ".kip", ".keys", ".iso", ".wad", ".cia", ".3ds"}
BIN_EXT = {".exe", ".dll", ".so", ".dylib", ".o", ".obj", ".lib", ".a", ".elf",
           ".wasm", ".pdb", ".bin", ".dat", ".pak", ".rom"}
SRC_EXT = {".md", ".txt", ".rst", ".py", ".ps1", ".sh", ".bat", ".cmd",
           ".c", ".cc", ".cpp", ".h", ".hpp", ".in", ".cmake", ".yml",
           ".yaml", ".json", ".toml", ".xml", ".csv", ".patch", ".diff",
           ".gitignore", ".gitattributes", ".gitmodules", ".license"}
SRC_NAMES = {"license", "copying", "notice", "makefile", "cmakelists.txt",
             "readme", ".gitignore", ".gitattributes", ".gitmodules"}
KEY = re.compile(rb"(?im)^\s*(?:master_key_[0-9a-f]{2}|titlekek_[0-9a-f]{2}|"
                 rb"header_key|key_area_key_\w+|title_key_\w+)\s*[:=]\s*[0-9a-f]{32,128}\b")
POLICY = "Public game-linked binaries are prohibited."


def check_name(name, source_only=False):
    cleaned = name.replace("\\", "/")
    path = PurePosixPath(cleaned)
    parts = [p.lower() for p in path.parts]
    errors = []
    if not cleaned or path.is_absolute() or "\x00" in cleaned or ".." in cleaned.split("/"):
        errors.append("unsafe path")
    if any(p in BLOCK_DIR for p in parts[:-1]):
        errors.append("private game/generated directory")
    base = parts[-1] if parts else ""
    ext = PurePosixPath(base).suffix.lower()
    if base in {"prod.keys", "title.keys", "main.npdm", "romfs.bin"} or ext in BLOCK_EXT:
        errors.append("console/game/key file")
    if source_only:
        if ext in BIN_EXT:
            errors.append("compiled or opaque binary payload")
        elif ext not in SRC_EXT and base not in SRC_NAMES:
            errors.append("non-source file in public artifact")
    return errors


def check_data(data):
    errors = []
    if len(data) > LIMIT:
        return ["over 24 MiB inspection limit"]
    if (data.startswith((b"NSO0", b"PFS0", b"\x7fELF", b"MZ"))
            or data[0x200:0x204] in (b"NCA2", b"NCA3")
            or data[0x10:0x14] == b"NRO0"):
        errors.append("binary or game container signature")
    if KEY.search(data):
        errors.append("key-shaped assignment")
    return errors


def archive_audit(path):
    errors, total, count = [], 0, 0

    def member(name, size, reader, special=False):
        nonlocal total, count
        count += 1
        total += size
        if count > 5000 or total > TOTAL_LIMIT:
            raise ValueError("archive inspection budget exceeded")
        label = str(path) + ":" + name
        for issue in check_name(name, source_only=True):
            errors.append(label + ": " + issue)
        if special:
            errors.append(label + ": link or special archive member")
        elif size > LIMIT:
            errors.append(label + ": payload over 24 MiB")
        else:
            data = reader(LIMIT + 1)
            if len(data) != size:
                errors.append(label + ": incomplete or inconsistent size")
            for issue in check_data(data):
                errors.append(label + ": " + issue)

    try:
        if zipfile.is_zipfile(path):
            with zipfile.ZipFile(path) as package:
                for info in package.infolist():
                    if info.is_dir():
                        continue
                    mode = (info.external_attr >> 16) & 0xFFFF
                    if stat.S_ISLNK(mode) or info.file_size > LIMIT:
                        member(info.filename, info.file_size, lambda size: b"",
                               stat.S_ISLNK(mode))
                    else:
                        with package.open(info) as stream:
                            member(info.filename, info.file_size, stream.read)
        elif tarfile.is_tarfile(path):
            with tarfile.open(path, "r:*") as package:
                for info in package:
                    if info.isdir():
                        continue
                    if not info.isfile() or info.size > LIMIT:
                        member(info.name, info.size, lambda size: b"", not info.isfile())
                    else:
                        stream = package.extractfile(info)
                        if stream is None:
                            errors.append(str(path) + ":" + info.name + ": unreadable")
                        else:
                            with stream:
                                member(info.name, info.size, stream.read)
        else:
            errors.append(str(path) + ": unsupported archive format")
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile, tarfile.TarError) as exc:
        errors.append(str(path) + ": failed closed: " + str(exc))
    return errors


def candidate_audit(path, base=None):
    # Preserve directory components: "dist/generated/main.cpp" must be rejected
    # even though "main.cpp" alone has a permitted source extension.
    if base is None:
        base = path if path.is_dir() else path.parent
    if path.is_symlink():
        return [str(path) + ": symlink not allowed"]
    if path.is_dir():
        errors = []
        for child in sorted(path.rglob("*")):
            if child.is_file() or child.is_symlink():
                errors.extend(candidate_audit(child, base))
        return errors
    if not path.is_file():
        return [str(path) + ": candidate missing"]
    relative = path.relative_to(base).as_posix()
    if path.suffix.lower() in {".zip", ".tar", ".tgz"} or path.name.endswith(".tar.gz"):
        errors = [str(path) + ": " + x for x in check_name(relative)]
        return errors + archive_audit(path)
    errors = [str(path) + ": " + x for x in check_name(relative, source_only=True)]
    if not errors:
        errors.extend(str(path) + ": " + x for x in check_data(path.read_bytes()))
    return errors


def repo_audit(root):
    raw = subprocess.run(["git", "-C", str(root), "ls-files", "-z"],
                         check=True, capture_output=True).stdout
    errors = []
    for item in raw.split(b"\0"):
        if not item:
            continue
        name = item.decode("utf-8", "surrogateescape")
        path = root / name
        errors.extend(name + ": " + x for x in check_name(name))
        if path.is_symlink():
            errors.append(name + ": tracked symlink requires human review")
        elif path.is_file():
            if path.stat().st_size > LIMIT:
                errors.append(name + ": oversize tracked file")
            else:
                errors.extend(name + ": " + x for x in check_data(path.read_bytes()))
    for file in ["README.md", "LEGAL.md", "docs/native-builds.md"]:
        p = root / file
        if not p.is_file() or POLICY not in p.read_text("utf-8"):
            errors.append(file + ": canonical distribution policy missing")
    return errors


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", type=Path, default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--artifact", type=Path, action="append", default=[])
    args = ap.parse_args()
    root = args.repo.resolve()
    try:
        errors = repo_audit(root)
        candidates = args.artifact or ([root / "dist"] if (root / "dist").exists() else [])
        for candidate in candidates:
            errors.extend(candidate_audit(candidate))
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        errors = ["audit could not complete: " + str(exc)]
    if errors:
        for error in errors:
            print("FAIL:", error, file=sys.stderr)
        print("FAIL: public distribution audit", file=sys.stderr)
        return 1
    print("PASS: tracked source and supplied candidates satisfy source-only checks.")
    print("Manual license/provenance review and historical-object checks remain necessary.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
