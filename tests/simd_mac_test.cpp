// Synthetic MLA/MLS decoder checks and generated-C execution test.
// Compile this generator against src, redirect stdout to a .c file, compile/run it.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"

int main() {
    // GNU as/objdump verified these neighboring encodings are undefined.
    for (unsigned word : {0x4EE29420u, 0x0EE29420u, 0x5E229420u, 0xCE229420u}) {
        std::string out;
        bool miss = false;
        suyu::recomp::Translate(word, 0x1000, out, &miss);
        if (!miss) { std::fprintf(stderr, "reserved MAC decoded: %08X\n", word); return 1; }
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t vreg[32][2]; uint64_t sentinel[4]; } GuestContext;
typedef void (*Fn)(GuestContext*);
)C");
    unsigned index = 0;
    for (unsigned q = 0; q < 2; ++q) for (unsigned size = 0; size < 3; ++size)
        for (unsigned sub = 0; sub < 2; ++sub) for (unsigned alias = 0; alias < 5; ++alias) {
            const unsigned rd = alias == 4 ? 31 : alias == 3 ? 1 : alias;
            const unsigned rn = alias == 4 ? 31 : 1;
            const unsigned rm = alias == 4 ? 31 : alias == 3 ? 1 : 2;
            const unsigned word = 0x0E209400 | (q << 30) | (sub << 29) | (size << 22) |
                                  (rm << 16) | (rn << 5) | rd;
            std::string out;
            bool miss = false;
            suyu::recomp::Translate(word, 0x1000, out, &miss);
            if (miss) { std::fprintf(stderr, "MAC not decoded: %08X\n", word); return 1; }
            std::printf("static void f%u(GuestContext* c) {\n%s}\n", index++, out.c_str());
        }
    std::puts("static Fn functions[] = {");
    for (unsigned i = 0; i < index; ++i) std::printf("f%u,\n", i);
    std::puts(R"C(};
static uint64_t rng = UINT64_C(0x716e249835a1cd7b);
static uint64_t random_word(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }
static uint64_t lane(const unsigned char* p, unsigned bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= (uint64_t)p[i] << (8*i);
    return value;
}
// Independent modular shift/add oracle: no multiplication expression.
static uint64_t product(uint64_t a, uint64_t b, unsigned bits) {
    uint64_t result = 0;
    const uint64_t mask = (UINT64_C(1) << bits) - 1;
    for (unsigned i = 0; i < bits; ++i) {
        if ((b >> i) & 1) result = (result + a) & mask;
        a = (a << 1) & mask;
    }
    return result;
}
int main(void) {
    unsigned index = 0, checks = 0;
    for (unsigned q = 0; q < 2; ++q) for (unsigned size = 0; size < 3; ++size)
    for (unsigned sub = 0; sub < 2; ++sub) for (unsigned alias = 0; alias < 5; ++alias) {
        const unsigned rd = alias == 4 ? 31 : alias == 3 ? 1 : alias;
        const unsigned rn = alias == 4 ? 31 : 1;
        const unsigned rm = alias == 4 ? 31 : alias == 3 ? 1 : 2;
        const unsigned bytes = 1u << size, active = q ? 16 : 8;
        for (unsigned sample = 0; sample < 103; ++sample) {
            GuestContext actual, expected;
            for (unsigned r = 0; r < 32; ++r) for (unsigned h = 0; h < 2; ++h)
                actual.vreg[r][h] = sample == 0 ? UINT64_MAX : sample == 1 ? 0 :
                    sample == 2 ? UINT64_C(0x8000800080008000) : random_word();
            for (unsigned i = 0; i < 4; ++i) actual.sentinel[i] = random_word();
            expected = actual;
            memset(expected.vreg[rd], 0, 16);
            for (unsigned offset = 0; offset < active; offset += bytes) {
                const uint64_t a = lane((unsigned char*)actual.vreg[rn] + offset, bytes);
                const uint64_t b = lane((unsigned char*)actual.vreg[rm] + offset, bytes);
                const uint64_t d = lane((unsigned char*)actual.vreg[rd] + offset, bytes);
                const uint64_t p = product(a, b, bytes*8);
                const uint64_t result = sub ? d - p : d + p;
                for (unsigned j = 0; j < bytes; ++j)
                    ((unsigned char*)expected.vreg[rd])[offset+j] = (unsigned char)(result >> (8*j));
            }
            functions[index](&actual);
            ++checks;
            if (memcmp(&actual, &expected, sizeof actual)) {
                printf("FAIL function=%u sample=%u\n", index, sample);
                return 1;
            }
        }
        ++index;
    }
    printf("%u generated-C MLA/MLS checks passed\n", checks);
    return 0;
}
)C");
}
