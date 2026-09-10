// Decode-coverage test for the AArch64 -> C emitter.
//
// Asserts that specific encodings are translated rather than falling through to
// recomp_unhandled, and prints the emitted C so the semantics can be reviewed.
// Mask errors are the easiest mistake to make when adding an instruction and
// the hardest to notice: a too-broad mask silently swallows a neighbouring
// encoding, and a too-narrow one leaves the instruction unhandled while the
// coverage number still improves.
//
// Build (from the repo root, inside a VS dev environment):
//   cl /nologo /EHsc /std:c++20 /I third_party\suyu\src tests\translate_test.cpp
//       /Fe:build\translate_test.exe

#include <cstdio>
#include <string>
#include <vector>

#include "core/recompiler/arm64_to_c.h"

namespace {

struct Case {
    const char* name;
    unsigned insn;
};

int failures = 0;

void Check(const Case& c) {
    std::string out;
    bool unhandled = false;
    suyu::recomp::Translate(c.insn, 0x1000, out, &unhandled);

    // Trim for single-line reporting.
    std::string body = out;
    while (!body.empty() && (body.back() == '\n' || body.back() == ' ')) {
        body.pop_back();
    }
    size_t start = body.find_first_not_of(" \t");
    if (start != std::string::npos) {
        body = body.substr(start);
    }

    if (unhandled) {
        std::printf("  FAIL  %-34s %08X  -> unhandled\n", c.name, c.insn);
        ++failures;
    } else {
        std::printf("  ok    %-34s %08X\n", c.name, c.insn);
        std::printf("            %s\n", body.c_str());
    }
}

} // namespace

int main() {
    // Encodings taken from the recorded examples in the the target title
    // coverage histogram, so these are instructions the title actually runs.
    const std::vector<Case> cases = {
        Case{"ror w21, w9, #0x1d", 0x13897535u},
        Case{"ror x20, x19, #8", 0x93D32274u},
        Case{"extr x0, x1, x2, #16", 0x93C24020u},
        Case{"adc x0, xzr, xzr", 0x9A1F03E0u},
        Case{"adcs w0, w1, w2", 0x3A020020u},
        Case{"sbc x3, x4, x5", 0xDA050083u},
        Case{"sbcs w6, w7, w8", 0x7A0800E6u},
        Case{"ldpsw x10, x8, [x8, #0x40]", 0x6948210Au},
        Case{"ldpsw x1, x2, [x3], #8", 0x68C10861u},
        Case{"ldpsw x1, x2, [x3, #8]!", 0x69C10861u},
    };

    std::printf("emitter decode coverage\n\n");
    for (const auto& c : cases) {
        Check(c);
    }

    std::printf("\n%d/%zu translated\n", int(cases.size()) - failures, cases.size());
    return failures == 0 ? 0 : 1;
}
