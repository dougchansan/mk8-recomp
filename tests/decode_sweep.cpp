// Decode-coverage sweep for the AArch64 -> C emitter.
//
// Reads 32-bit encodings on stdin, one hex word per line, and writes
// "<hex>\t<0|1>" per line - 1 meaning the emitter translated it rather than
// falling through to recomp_unhandled.
//
// This exists to supply a denominator. The emitter's own coverage report
// buckets what it missed by encoding group and by signature, which says how
// much was missed but not what any of it is, so "what should we implement
// next" has been answered by reading hex. Pairing this with a disassembler on
// the caller's side (scripts/decode-coverage.py) turns the same data into a
// list of mnemonics.
//
// Build (from the repo root, inside a VS dev environment):
//   cl /nologo /EHsc /std:c++20 /O2 /I third_party\suyu\src tests\decode_sweep.cpp
//       /Fe:build\decode_sweep.exe
//
// or with clang/gcc:
//   c++ -std=c++20 -O2 -I third_party/suyu/src tests/decode_sweep.cpp -o build/decode_sweep

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/recompiler/arm64_to_c.h"

int main() {
    char line[64];
    while (std::fgets(line, sizeof(line), stdin)) {
        const unsigned long insn = std::strtoul(line, nullptr, 16);
        if (insn == 0 && line[0] != '0') {
            continue;  // blank or malformed; a real zero word reads as "00000000"
        }
        std::string out;
        bool unhandled = false;
        // The PC is fixed: nothing here depends on it except PC-relative
        // operands, and those are translated or not regardless of its value.
        suyu::recomp::Translate(static_cast<unsigned>(insn), 0x1000, out, &unhandled);
        std::printf("%08lX\t%d\n", insn, unhandled ? 0 : 1);
    }
    return 0;
}
