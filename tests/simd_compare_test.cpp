// Generate C, then compile and execute it: g++ -std=c++20 -I third_party/suyu/src
// tests/simd_compare_test.cpp -o compare-gen && ./compare-gen > compare.c
// cc -std=c11 -O2 -fsanitize=undefined compare.c -o compare-test && ./compare-test
// Arm CMGT/CMGE/CMHI/CMHS pseudocode: signed/unsigned >/>=, all-one true lanes.
// GNU as/objdump verified all eight arrangements for each opcode independently.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"

int main() {
    // Seven vector arrangements, followed by scalar D. Fixed operands v0,v1,v2.
    const unsigned forms[] = {0x0e223420,0x4e223420,0x0e623420,0x4e623420,
                              0x0ea23420,0x4ea23420,0x4ee23420,0x5ee23420};
    unsigned negatives = 0;
    for (unsigned op=0;op<4;++op) {
        const unsigned extra=((op>>1)<<29)|((op&1)<<11);
        for (unsigned word : {0x0ee23420u,0x5e223420u,0x5e623420u,0x5ea23420u,
                              0x1ee23420u,0xde223420u}) {
            std::string out; bool unhandled=false;
            suyu::recomp::Translate(word|extra,0x1000,out,&unhandled);
            if (!unhandled) { std::fprintf(stderr,"negative accepted %08x\n",word|extra);return 1; }
            ++negatives;
        }
    }
    std::fprintf(stderr,"%u reserved/mask negatives passed\n",negatives);
    std::puts("#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\ntypedef struct { uint64_t vreg[32][2]; } GuestContext;");
    for(unsigned f=0;f<8;++f)for(unsigned op=0;op<4;++op)for(unsigned alias=0;alias<5;++alias){
        unsigned rd=alias==4?31:alias==3?1:alias,n=alias==4?31:1,m=alias>=3?n:2;
        unsigned word=(forms[f]&~0x1f03ffu)|(m<<16)|(n<<5)|rd|((op>>1)<<29)|((op&1)<<11);
        std::string out;bool unhandled=false;suyu::recomp::Translate(word,0x1000,out,&unhandled);
        if(unhandled){std::fprintf(stderr,"unhandled %08x\n",word);return 1;}
        std::printf("static void f%u%u%u(GuestContext*c){%s}\n",f,op,alias,out.c_str());
    }
    std::puts("typedef void(*Fn)(GuestContext*); static Fn fn[8][4][5]={");
    for(unsigned f=0;f<8;++f){std::puts("{");for(unsigned op=0;op<4;++op){std::puts("{");for(unsigned a=0;a<5;++a)std::printf("f%u%u%u,",f,op,a);std::puts("},");}std::puts("},");}
    std::puts(R"C(};
static unsigned long checks,failures;
static void check(unsigned f,unsigned op,unsigned alias,uint64_t x,uint64_t y){
 const unsigned sizes[8]={1,1,2,2,4,4,8,8},bytes[8]={8,16,8,16,8,16,16,8};
 unsigned esz=sizes[f],rd=alias==4?31:alias==3?1:alias,n=alias==4?31:1,m=alias>=3?n:2;
 uint64_t mask=UINT64_MAX>>(64-8*esz),sign=UINT64_C(1)<<(8*esz-1);
 GuestContext c,before;memset(&c,0xa5,sizeof c);unsigned char expected[16]={0};
 for(unsigned lane=0;lane<bytes[f]/esz;++lane){
  // Alternate direction per lane to expose lane-order and size mistakes.
  uint64_t a=(lane&1?y:x)&mask,b=(lane&1?x:y)&mask;
  memcpy((char*)c.vreg[n]+lane*esz,&a,esz);if(m!=n)memcpy((char*)c.vreg[m]+lane*esz,&b,esz);else b=a;
  // Independent unsigned ordering oracle: flip sign bit for signed order.
  uint64_t ka=a^(op<2?sign:0),kb=b^(op<2?sign:0);
  int yes=(ka>kb)||((op&1)&&ka==kb);
  memset(expected+lane*esz,yes?255:0,esz);
 }
 before=c;fn[f][op][alias](&c);++checks;
 if(memcmp(c.vreg[rd],expected,16)){if(failures++<5)printf("FAIL f=%u op=%u alias=%u x=%llx y=%llx\n",f,op,alias,(unsigned long long)x,(unsigned long long)y);}
 for(unsigned r=0;r<32;++r)if(r!=rd&&memcmp(c.vreg[r],before.vreg[r],16)){++failures;break;}
}
int main(void){
 // Exhaust every byte pair, every opcode, both vector widths and all aliases.
 for(unsigned f=0;f<2;++f)for(unsigned op=0;op<4;++op)for(unsigned a=0;a<5;++a)
  for(unsigned x=0;x<256;++x)for(unsigned y=0;y<256;++y)check(f,op,a,x,y);
 for(unsigned f=2;f<8;++f){
  const unsigned bits[8]={8,8,16,16,32,32,64,64};uint64_t sign=UINT64_C(1)<<(bits[f]-1),mask=UINT64_MAX>>(64-bits[f]);
  uint64_t v[]={0,1,2,sign-2,sign-1,sign,sign+1,mask-1,mask};
  for(unsigned op=0;op<4;++op)for(unsigned a=0;a<5;++a)for(unsigned x=0;x<9;++x)for(unsigned y=0;y<9;++y)check(f,op,a,v[x],v[y]);
 }
 printf("%lu/%lu generated-C SIMD comparison checks passed\n",checks-failures,checks);return failures!=0;
}
)C");
}
