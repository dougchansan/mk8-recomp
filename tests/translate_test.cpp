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

void CheckFallsBack(const Case& c) {
    std::string out;
    bool unhandled = false;
    suyu::recomp::Translate(c.insn, 0x1000, out, &unhandled);

    // The mirror of Check. Some encodings must keep reaching the fallback
    // engine, and a mask that is too broad swallows them silently while the
    // coverage number improves - the exact failure this file exists to catch.
    // DC ZVA writes memory and IC IVAU invalidates the instruction cache, so
    // translating either as a no-op would be a wrong answer, not a gap.
    if (unhandled) {
        std::printf("  ok    %-34s %08X  -> falls back, as required\n", c.name, c.insn);
    } else {
        std::printf("  FAIL  %-34s %08X  -> translated, must not be\n", c.name, c.insn);
        ++failures;
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
        Case{"ldaxr w8, [x19]", 0x885FFE68u},
        Case{"ldxr x9, [x8]", 0xC85F7D09u},
        Case{"ldaxr w8, [x0]", 0x885FFC08u},
        Case{"stxr w1, w2, [x3]", 0x88017C62u},
        Case{"stlxr w4, x5, [x6]", 0xC804FCC5u},
        Case{"ldxrb w1, [x2]", 0x085F7C41u},
        Case{"clrex", 0xD5033F5Fu},
        Case{"msr fpcr, x1", 0xD51B4401u},
        Case{"mrs x1, fpcr", 0xD53B4401u},
        Case{"msr fpsr, x2", 0xD51B4422u},
        Case{"mrs x2, fpsr", 0xD53B4422u},
        Case{"prfm pstl1keep, [x19]", 0xF9800260u},
        Case{"ldr x0, [x1, #8]", 0xF9400420u},
        Case{"str x0, [x1, #8]", 0xF9000420u},
        Case{"mrs x0, cntpct_el0", 0xD53BE020u},
        Case{"mrs x20, cntpct_el0", 0xD53BE034u},
        Case{"mrs x1, cntvct_el0", 0xD53BE041u},
        Case{"mrs x2, cntfrq_el0", 0xD53BE002u},
        Case{"ldxp x10, x9, [x0]", 0xC87F240Au},
        Case{"ldaxp x8, x10, [x0]", 0xC87FA808u},
        Case{"stxp w1, x10, x9, [x0]", 0xC821240Au},
        Case{"stlxp w1, x10, x9, [x0]", 0xC821A40Au},
        Case{"ldxp w10, w9, [x0]", 0x887F240Au},
        Case{"stxp w1, w10, w9, [x0]", 0x8821240Au},
        Case{"ldxp wzr, w9, [sp]", 0x887F27FFu},
        Case{"mrs x9, ctr_el0", 0xD53B0029u},
        Case{"mrs xzr, ctr_el0", 0xD53B003Fu},
        Case{"msr ctr_el0, x9", 0xD51B0029u},
        Case{"dc cvac, x10", 0xD50B7A2Au},
        Case{"dc cvau, x10", 0xD50B7B2Au},
        Case{"dc civac, x10", 0xD50B7E2Au},
    };

    std::printf("emitter decode coverage\n\n");
    for (const auto& c : cases) {
        Check(c);
    }

    const std::vector<Case> must_fall_back = {
        Case{"dc zva, x10", 0xD50B742Au},
        Case{"ic ivau, x10", 0xD50B752Au},
    };

    std::printf("\nencodings that must NOT be translated\n\n");
    for (const auto& c : must_fall_back) {
        CheckFallsBack(c);
    }

    const std::size_t total = cases.size() + must_fall_back.size();
    std::printf("\n%d/%zu ok\n", int(total) - failures, total);
    return failures == 0 ? 0 : 1;
}
