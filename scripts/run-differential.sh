#!/usr/bin/env bash
# Run in an isolated directory containing copied tests/ and emitter header.
# Arguments are explicit existing Dynarmic source and suyu build paths.
set -euo pipefail
if [ "$#" -lt 3 ]; then
  echo "usage: $0 DYNARMIC_SOURCE SUYU_BUILD FMT_INCLUDE [runner arguments]" >&2
  exit 2
fi
dynarmic_source=$(realpath "$1")
suyu_build=$(realpath "$2")
fmt_include=$(realpath "$3")
suyu_source=$(realpath "$dynarmic_source/../..")
shift 3
dynarmic_library="$suyu_build/src/dynarmic/src/dynarmic/libdynarmic.a"
fmt_library="$suyu_build/_deps/fmt-build/libfmt.a"
test -f "$dynarmic_library"
test -f "$fmt_library"
test -f "$dynarmic_source/src/dynarmic/interface/A64/a64.h"
# This fork also contains an inactive externals/dynarmic with incompatible ABI.
# Require the header source to match the source used by the existing library.
grep -Fq "$dynarmic_source/src/dynarmic/backend/x64/a64_interface.cpp" "$suyu_build/compile_commands.json"
g++ -std=c++20 -O0 -I. tests/differential_generate.cpp -o differential-generate
./differential-generate > differential-generated.c
./differential-generate --assembly > differential-encoding.s
aarch64-linux-gnu-as differential-encoding.s -o differential-encoding.o
aarch64-linux-gnu-objcopy -O binary differential-encoding.o differential-encoding.bin
python3 tests/differential_verify_assembly.py differential-encoding.bin
gcc -std=c11 -O2 -fsanitize=undefined -Itests -c differential-generated.c -o differential-generated.o
g++ -std=c++20 -O2 -Itests -I"$dynarmic_source/src" -I"$suyu_source/src" -I"$fmt_include" \
  tests/differential_dynarmic.cpp tests/differential_logging.cpp \
  differential-generated.o "$dynarmic_library" "$fmt_library" -fsanitize=undefined -pthread -lm -o differential-run
./differential-run "$@"
