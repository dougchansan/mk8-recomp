"""Reject workstation drive and user-home paths in public tracked text and patches."""
import re
import subprocess
from pathlib import Path

LOCAL_PATH = re.compile(r"(?<![A-Za-z0-9])[A-Za-z]:[\\/]|/(?:Users|home)/[^/\s]+/|/mnt/[a-z]/", re.IGNORECASE)


def violations(text):
    return [(number, line.strip()) for number, line in enumerate(text.splitlines(), 1)
            if LOCAL_PATH.search(line)]


def main():
    root = Path(__file__).resolve().parent.parent
    names = subprocess.check_output(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"], cwd=root
    ).decode().split("\0")
    failures = []
    for name in sorted(set(filter(None, names))):
        path = root / name
        if not path.is_file():
            continue
        data = path.read_bytes()
        if b"\0" in data:
            continue
        for line, text in violations(data.decode("utf-8", errors="replace")):
            failures.append(f"{name}:{line}: {text[:200]}")
    if failures:
        print("Replace local paths with checkout-relative paths, environment variables, or explicit arguments:")
        print("\n".join(failures))
        return 1
    print("Portable path audit passed (including embedded patches).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
