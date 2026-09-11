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
    // Encodings taken from the recorded examples in the target title's
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
        Case{"fcvtms w18, s3", 0x1E300072u},
        Case{"fcvtps w1, s2", 0x1E280041u},
        Case{"fcvtns w1, s2", 0x1E200041u},
        Case{"fcvtas w1, s2", 0x1E240041u},
        Case{"fcvtau w1, s2", 0x1E250041u},
        Case{"fcvtmu w1, s2", 0x1E310041u},
        Case{"fcvtpu w1, s2", 0x1E290041u},
        Case{"fcvtms x1, d2", 0x9E700041u},
        Case{"neg v4.4s, v4.4s", 0x6EA0B884u},
        Case{"abs v1.4s, v1.4s", 0x4EA0B821u},
        Case{"neg v1.16b, v1.16b", 0x6E20B821u},
        Case{"neg v1.8h, v1.8h", 0x6E60B821u},
        Case{"neg v1.2d, v1.2d", 0x6EE0B821u},
        Case{"abs v1.2d, v1.2d", 0x4EE0B821u},
        Case{"neg d1, d1", 0x7EE0B821u},
        Case{"abs v1.8b, v1.8b", 0x0E20B821u},
        Case{"fmla v2.4s, v7.4s, v0.s[1]", 0x4FA010E2u},
        Case{"fmls v2.4s, v7.4s, v0.s[1]", 0x4FA050E2u},
        Case{"fmul v2.4s, v7.4s, v0.s[1]", 0x4FA090E2u},
        Case{"fmla v2.2d, v7.2d, v0.d[1]", 0x4FC018E2u},
        Case{"fccmp s1, s8, #4, ls", 0x1E289424u},
        Case{"fccmpe s1, s8, #4, ls", 0x1E289434u},
        Case{"fccmp d1, d8, #7, eq", 0x1E680427u},
        Case{"fcmp s1, s8", 0x1E282020u},
        Case{"fcmp s1, #0.0", 0x1E202028u},
        Case{"ld1r {v0.4s}, [x8]", 0x4D40C900u},
        Case{"ld1r {v0.2d}, [x9]", 0x4D40CD20u},
        Case{"ld1r {v0.16b}, [x8]", 0x4D40C100u},
        Case{"ld1r {v0.8h}, [x8]", 0x4D40C500u},
        Case{"ld1r {v0.8b}, [x8]", 0x0D40C100u},
        Case{"ushl v2.4s, v2.4s, v4.4s", 0x6EA44442u},
        Case{"sshl v2.4s, v2.4s, v4.4s", 0x4EA44442u},
        Case{"ushl v0.16b, v1.16b, v2.16b", 0x6E224420u},
        Case{"ushl v0.2d, v1.2d, v2.2d", 0x6EE24420u},
        Case{"sshl v0.8h, v1.8h, v2.8h", 0x4E624420u},
        Case{"movi v1.4s, #0x10", 0x4F000601u},
        Case{"movi v24.4s, #0xbf, lsl #24", 0x4F0567F8u},
        Case{"mvni v2.4s, #0x10", 0x6F000602u},
        Case{"movi v3.8b, #0xff", 0x0F07E7E3u},
        Case{"movi v4.2d, #0xffffffffffffffff", 0x6F07E7E4u},
        Case{"movi v5.8h, #0x20", 0x4F018405u},
        Case{"movi v6.4s, #0x7f, msl #8", 0x4F03C7E6u},
    };

    std::printf("emitter decode coverage\n\n");
    for (const auto& c : cases) {
        Check(c);
    }

    const std::vector<Case> must_fall_back = {
        Case{"dc zva, x10", 0xD50B742Au},
        Case{"ic ivau, x10", 0xD50B752Au},
        Case{"cmlt v1.4s, v1.4s, #0", 0x4EA0A821u},
        Case{"mul v0.4s, v1.4s, v2.s[0]", 0x4F828020u},
        Case{"mla v0.4s, v1.4s, v2.s[0]", 0x6F820020u},
        Case{"smlal v0.4s, v1.4h, v2.h[0]", 0x0F422020u},
        Case{"sqdmulh v0.4s, v1.4s, v2.s[0]", 0x4F82C020u},
        Case{"fmulx v2.4s, v7.4s, v0.s[1]", 0x6FA090E2u},
        Case{"ld2r {v0.4s, v1.4s}, [x8]", 0x4D60C900u},
        Case{"ld1 {v1.s}[0], [x8]", 0x0D408101u},
        Case{"uqshl v2.4s, v2.4s, v4.4s", 0x6EA44C42u},
        Case{"urshl v2.4s, v2.4s, v4.4s", 0x6EA45442u},
        Case{"ushl d0, d1, d2", 0x7EE24420u},
        Case{"orr v7.4s, #0x10", 0x4F001607u},
        Case{"bic v8.4s, #0x10", 0x6F001608u},
        Case{"fmov v9.4s, #1.0", 0x4F03F609u},
        Case{"sqshl v5.4s, v3.4s, #3", 0x4F237465u},
        Case{"ushr v5.4s, v3.4s, #3", 0x6F3D0465u},
        Case{"sshr v5.4s, v3.4s, #3", 0x4F3D0465u},
        Case{"shrn v5.4h, v3.4s, #3", 0x0F1D8465u},
        Case{"shl v5.4s, v3.4s, #3", 0x4F235465u},
        Case{"shl v0.16b, v1.16b, #1", 0x4F095420u},
        Case{"shl v0.2d, v1.2d, #40", 0x4F685420u},
        Case{"shl v0.8h, v1.8h, #5", 0x4F155420u},
        Case{"shl v0.8b, v1.8b, #2", 0x0F0A5420u},
        // Deliberately not translated: measurably slower than the JIT.
        Case{"fcvtzu w8, s10, #30", 0x1E198948u},
        Case{"fcvtzs w8, s0, #8", 0x1E18E008u},
        Case{"fcvtzu x8, d10, #40", 0x9E596148u},
        Case{"fcvtzs x1, d2, #1", 0x9E58FC41u},
        Case{"scvtf s0, w1, #4", 0x1E02F020u},
        Case{"ucvtf d0, x1, #12", 0x9E43D020u},
    };

    std::printf("\nencodings that must NOT be translated\n\n");
    for (const auto& c : must_fall_back) {
        CheckFallsBack(c);
    }

    const std::size_t total = cases.size() + must_fall_back.size();
    std::printf("\n%d/%zu ok\n", int(total) - failures, total);
    return failures == 0 ? 0 : 1;
}
