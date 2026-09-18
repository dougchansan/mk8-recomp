// Generate a standalone C semantic test, then compile that C with -lm.
// g++ -std=c++20 -I third_party/suyu/src tests/fma_semantics_test.cpp -o fma-gen
// ./fma-gen > fma-test.c && cc -std=c11 -O2 -fno-fast-math fma-test.c -lm -o fma-test
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main() {
    std::puts(R"C(#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
typedef struct { uint64_t vreg[32][2],fpcr,fpsr; } GuestContext;
)C");
    for (unsigned d=0;d<2;++d) for(unsigned op=0;op<4;++op)
        for(unsigned alias=0;alias<5;++alias) {
            const unsigned rd=alias==4?31:alias;
            const unsigned n=alias==4?31:1,m=alias==4?31:2,a=alias==4?31:3;
            const unsigned insn=0x1f000000 | (d<<22) | ((op>>1)<<21) |
                ((op&1)<<15) | (m<<16) | (a<<10) | (n<<5) | rd;
            std::string out; bool unhandled=false;
            suyu::recomp::Translate(insn,0x1000,out,&unhandled);
            if(unhandled) return 1;
            std::printf("static void f%u%u%u(GuestContext *c) {\n%s}\n",d,op,alias,out.c_str());
        }
    std::puts("typedef void (*Fn)(GuestContext*);\nstatic Fn functions[2][4][5]={");
    for(unsigned d=0;d<2;++d) {
        std::puts("{");
        for(unsigned op=0;op<4;++op)
            std::printf("{f%u%u0,f%u%u1,f%u%u2,f%u%u3,f%u%u4},\n",d,op,d,op,d,op,d,op,d,op);
        std::puts("},");
    }
    std::puts(R"C(};
static uint64_t bits(double x,int d){uint64_t v=0;if(d)memcpy(&v,&x,8);else {float s=(float)x;memcpy(&v,&s,4);}return v;}
static unsigned failures=0,checks=0;
static void check(unsigned d,unsigned op,unsigned alias,uint64_t n,uint64_t m,uint64_t a,uint64_t expected) {
 GuestContext c;memset(&c,0xA5,sizeof c);c.fpcr=0;c.fpsr=0;
 const unsigned rd=alias==4?31:alias;
 if(alias==4)c.vreg[31][0]=n;
 else {c.vreg[1][0]=n;c.vreg[2][0]=m;c.vreg[3][0]=a;}
 functions[d][op][alias](&c);++checks;
 if(c.vreg[rd][0]!=expected||c.vreg[rd][1]!=0){++failures;printf("FAIL d=%u op=%u alias=%u got=%016llx upper=%016llx expected=%016llx\n",d,op,alias,(unsigned long long)c.vreg[rd][0],(unsigned long long)c.vreg[rd][1],(unsigned long long)expected);}
}
int main(void) {
 // Exact integer arithmetic: n*m=6, a=5. No host fma used as the oracle.
 const double expected[4]={11,-1,-11,1},same[4]={6,-2,-6,2};
 for(unsigned d=0;d<2;++d)for(unsigned op=0;op<4;++op){
  for(unsigned alias=0;alias<4;++alias)check(d,op,alias,bits(2,d),bits(3,d),bits(5,d),bits(expected[op],d));
  check(d,op,4,bits(2,d),0,0,bits(same[op],d));
 }
 // (1+2^-p)*(1-2^-p)-1 = -2^-2p, exactly representable.
 // An intermediate rounded multiplication incorrectly loses this residue.
 for(unsigned d=0;d<2;++d)for(unsigned op=0;op<4;++op) {
  uint64_t sign=d?UINT64_C(0x8000000000000000):UINT64_C(0x80000000);
  uint64_t n=d?UINT64_C(0x3ff0000000000001):UINT64_C(0x3f800001);
  uint64_t m=d?UINT64_C(0x3feffffffffffffe):UINT64_C(0x3f7ffffe);
  uint64_t a=bits(-1,d),r=d?UINT64_C(0xb970000000000000):UINT64_C(0xa8800000);
  // Adjust input signs so every opcode computes the same exact expression.
  if(((op>>1)^(op&1)))n^=sign;
  if(op>>1)a^=sign;
  check(d,op,0,n,m,a,r);
 }
 // Exact cancellation under RN is +0, including FNMADD (not -fma).
 for(unsigned d=0;d<2;++d) {
  check(d,0,0,bits(1,d),bits(1,d),bits(-1,d),0);
  check(d,2,0,bits(1,d),bits(1,d),bits(-1,d),0);
  check(d,0,0,bits(-0.,d),bits(1,d),bits(-0.,d),bits(-0.,d));
  check(d,0,0,bits(INFINITY,d),bits(1,d),bits(2,d),bits(INFINITY,d));
  check(d,1,0,bits(INFINITY,d),bits(1,d),bits(2,d),bits(-INFINITY,d));
  check(d,2,0,bits(INFINITY,d),bits(1,d),bits(2,d),bits(-INFINITY,d));
  check(d,3,0,bits(INFINITY,d),bits(1,d),bits(2,d),bits(INFINITY,d));
 }
 printf("%u/%u generated-C FMA checks passed\n",checks-failures,checks);
 return failures!=0;
}
)C");
}
