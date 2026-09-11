#!/usr/bin/env bash
# Copy files to the Linux box and strip CRs. Run from the repo root.
#
# A stray CR makes a shebang read as `bash\r` and the script will not run at all;
# inside a C string literal it is a compile error on GCC. Both have cost a run.
#
#   ./scripts/push-to-box.sh scripts/run-hybrid.sh tests/translate_test.cpp
set -euo pipefail

HOST="${HOST:-vast}"
DEST="${DEST:-\$HOME/mk8-recomp}"

[ $# -gt 0 ] || { echo "usage: $0 <path> [path...]" >&2; exit 2; }

for f in "$@"; do
    [ -f "$f" ] || { echo "no such file: $f" >&2; exit 1; }
    ssh "$HOST" "mkdir -p $DEST/$(dirname "$f")"
    scp -q "$f" "$HOST:$DEST/$f"
    ssh "$HOST" "sed -i 's/\r\$//' $DEST/$f; case \"$f\" in *.sh) chmod +x $DEST/$f;; esac"
    echo "sent $f"
done
