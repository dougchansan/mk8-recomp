#!/usr/bin/env bash
# Disassemble the top unimplemented opcodes from a recomp coverage report.
#   ./scripts/disasm-fallbacks.sh [coverage.txt] [n]
set -euo pipefail

COV="${1:-$HOME/.local/share/suyu/log/recomp_coverage.txt}"
N="${2:-15}"
OD="${OD:-aarch64-linux-gnu-objdump}"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

mapfile -t rows < <(awk '/unimplemented opcodes by execution count/,/SVCs by call count/' "$COV" \
    | grep -oP '^\s+\K[0-9A-F]{8}\s+[0-9]+\s+[0-9.]+%' | head -"$N")

: > "$tmp/a.s"
for r in "${rows[@]}"; do
    enc=$(echo "$r" | awk '{print $1}')
    echo ".inst 0x$enc" >> "$tmp/a.s"
done

"${OD%objdump}as" -o "$tmp/a.o" "$tmp/a.s" 2>/dev/null \
    || aarch64-linux-gnu-as -o "$tmp/a.o" "$tmp/a.s"

mapfile -t dis < <("$OD" -d "$tmp/a.o" | grep -oP '^\s+[0-9a-f]+:\s+[0-9a-f]{8}\s+\K.*')

for i in "${!rows[@]}"; do
    printf '%-34s %s\n' "${rows[$i]}" "${dis[$i]:-<undecoded>}"
done
