// Emit an executable C regression for SIMD pair loads using synthetic memory.
// g++ -std=c++20 -I third_party/suyu/src tests/simd_pair_load_test.cpp -o /tmp/simd-pair-gen
// /tmp/simd-pair-gen > /tmp/simd-pair.c
// gcc -std=c11 -O2 /tmp/simd-pair.c -o /tmp/simd-pair && /tmp/simd-pair
#include <iostream>
#include "core/recompiler/arm64_to_c.h"

int main() {
    std::cout << R"C(
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t x[32]; uint64_t vreg[32][2]; } GuestContext;
uint64_t recomp_load32(GuestContext*c,uint64_t a) {
    uint32_t v; (void)c; memcpy(&v,(void*)(uintptr_t)a,4); return v;
}
uint64_t recomp_load64(GuestContext*c,uint64_t a) {
    uint64_t v; (void)c; memcpy(&v,(void*)(uintptr_t)a,8); return v;
}
void recomp_ldp32(GuestContext*c,uint64_t a,uint64_t*l,uint64_t*h) {
    *l=recomp_load32(c,a); *h=recomp_load32(c,a+4);
}
void recomp_ldp64(GuestContext*c,uint64_t a,uint64_t*l,uint64_t*h) {
    *l=recomp_load64(c,a); *h=recomp_load64(c,a+8);
}
)C";
    // opc selects S/D/Q. Modes are post-index, signed offset, and pre-index.
    // imm7=2 advances by two registers; rn=2, rt=0, rt2=1.
    for (unsigned opc = 0; opc < 3; ++opc) {
        for (unsigned mode = 1; mode <= 3; ++mode) {
            const unsigned insn = 0x2c400000u | (opc << 30) | (mode << 23) |
                                  (2u << 15) | (1u << 10) | (2u << 5);
            std::string body;
            bool unhandled = false;
            if (!suyu::recomp::Translate(insn, 0x1000, body, &unhandled) || unhandled) {
                std::cerr << "LDP decode failed: " << std::hex << insn << '\n';
                return 1;
            }
            std::cout << "void run" << opc << mode << "(GuestContext*c){" << body << "}\n";
        }
    }
    std::cout << R"C(
int main(void) {
    void(*fn[3][3])(GuestContext*)={
        {run01,run02,run03},{run11,run12,run13},{run21,run22,run23}
    };
    unsigned char memory[64];
    int failures=0;
    for(unsigned i=0;i<sizeof memory;i++) memory[i]=(unsigned char)(i*37+11);
    for(unsigned opc=0;opc<3;opc++) {
        unsigned bytes=4u<<opc;
        for(unsigned mode=1;mode<=3;mode++) {
            GuestContext actual,expected;
            memset(&actual,0xff,sizeof actual);
            actual.x[2]=(uintptr_t)memory;
            expected=actual;
            // Scalar loads replace the entire vector, with zero extension.
            memset(expected.vreg[0],0,sizeof expected.vreg[0]);
            memset(expected.vreg[1],0,sizeof expected.vreg[1]);
            unsigned offset=mode==1?0:2*bytes;
            memcpy(expected.vreg[0],memory+offset,bytes);
            memcpy(expected.vreg[1],memory+offset+bytes,bytes);
            if(mode!=2) expected.x[2]+=2*bytes;
            fn[opc][mode-1](&actual);
            // Whole-context comparison also checks every untouched register.
            int good=memcmp(&actual,&expected,sizeof actual)==0;
            printf("LDP %c mode=%u: %s\n","SDQ"[opc],mode,good?"PASS":"FAIL");
            failures+=!good;
        }
    }
    return failures!=0;
}
)C";
}
