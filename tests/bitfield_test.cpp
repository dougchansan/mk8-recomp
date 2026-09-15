// SBFM boundary regressions: generated C must never shift a 64-bit value by 64.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"

int main() {
    const unsigned words[] = {
        0x93607ED6u, // sbfiz x22, x22, #32, #32: destination field reaches bit 63
        0x9340FC20u, // asr x0, x1, #0: extracted field is all 64 bits
    };
    for (unsigned word : words) {
        std::string output;
        bool miss = false;
        suyu::recomp::Translate(word, 0x1000, output, &miss);
        if (miss || output.find("1ULL << 64") != std::string::npos) {
            std::fprintf(stderr, "invalid SBFM output for %08X: %s\n", word, output.c_str());
            return 1;
        }
    }
    std::puts("2/2 SBFM boundary checks passed");
}
