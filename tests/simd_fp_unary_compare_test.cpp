// Compile this generator against the emitter, then compile and run its C output.
// GNU as verifies the mnemonic/.inst pairs emitted by --assembly independently.
#include <cstdio>
#include <cstring>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(int argc,char** argv) {
    const bool assembly=argc>1 && !std::strcmp(argv[1],"--assembly");
    if(assembly) puts(".text");
    else puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
    for(unsigned op=0;op<3;++op)for(unsigned shape=0;shape<3;++shape)
    for(unsigned alias=0;alias<3;++alias) {
        unsigned rd=alias==2?31:alias,rn=alias==2?31:1;
        unsigned word=(op==2?0x0ea0e800:op==1?0x2ea0f800:0x0ea0f800)|
            (shape?0x40000000:0)|(shape==2?0x400000:0)|(rn<<5)|rd;
        if(assembly) {
            printf(".inst 0x%08x\n%s v%u.%s,v%u.%s%s\n",word,
                op==2?"fcmlt":op==1?"fneg":"fabs",rd,shape==2?"2d":shape?"4s":"2s",
                rn,shape==2?"2d":shape?"4s":"2s",op==2?",#0.0":"");
        } else {
            std::string s;bool miss=false;suyu::recomp::Translate(word,0x1000,s,&miss);
            if(miss)return 2;
            printf("static void f%u%u%u(GuestContext*c){%s}\n",op,shape,alias,s.c_str());
        }
    }
    if(assembly)return 0;
    // Unallocated vector 1D unary forms, and bit31 outside the SIMD class.
    unsigned negatives_failed=0;
    for(unsigned word:{0x0ee0f820u,0x2ee0f820u,0xcea0f820u,0xeea0f820u}) {
        std::string s;bool miss=false;suyu::recomp::Translate(word,0x1000,s,&miss);
        if(!miss){++negatives_failed;std::fprintf(stderr,"reserved unary decoded %08x\n",word);}
    }
    printf("static const unsigned negatives_failed=%u;\n",negatives_failed);
    puts("typedef void(*Fn)(GuestContext*);static Fn fn[3][3][3]={");
    for(unsigned op=0;op<3;++op){puts("{");for(unsigned shape=0;shape<3;++shape)
        printf("{f%u%u0,f%u%u1,f%u%u2},",op,shape,op,shape,op,shape);puts("},");}
    puts(R"C(};
int main(void){
 unsigned checks=0,failures=negatives_failed;
 const uint64_t values[2][10]={
  {0,0x80000000,0x3f800000,0xbf800000,0x40800000,0xc0800000,0x7fc12345,0xffc12345,0x7f812345,0xff812345},
  {0,0x8000000000000000ULL,0x3ff0000000000000ULL,0xbff0000000000000ULL,0x4010000000000000ULL,0xc010000000000000ULL,0x7ff8123456789abcULL,0xfff8123456789abcULL,0x7ff0123456789abcULL,0xfff0123456789abcULL}};
 for(unsigned op=0;op<3;++op)for(unsigned shape=0;shape<3;++shape)
 for(unsigned alias=0;alias<3;++alias)for(unsigned sample=0;sample<(op==2?6:10);++sample){
  unsigned dbl=shape==2,width=dbl?8:4,active=shape?16/width:2,rd=alias==2?31:alias,rn=alias==2?31:1;
  uint64_t sign=dbl?0x8000000000000000ULL:0x80000000ULL;
  GuestContext c,expected;memset(&c,0xa5,sizeof c);c.fpcr=0;c.fpsr=0x8000010;
  for(unsigned j=0;j<16/width;++j){uint64_t in=values[dbl][(sample+j)%(op==2?6:10)];memcpy((char*)c.vreg[rn]+j*width,&in,width);}
  expected=c;memset(expected.vreg[rd],0,16);
  for(unsigned j=0;j<active;++j){
   uint64_t in=values[dbl][(sample+j)%(op==2?6:10)];
   uint64_t out=op==0?in&~sign:op==1?in^sign:(in&sign)&&(in&~sign)?UINT64_MAX:0;
   memcpy((char*)expected.vreg[rd]+j*width,&out,width);
  }
  fn[op][shape][alias](&c);++checks;
  if(memcmp(&c,&expected,sizeof c)){if(failures++<5)printf("FAIL op=%u shape=%u alias=%u sample=%u got=%llx expected=%llx\n",op,shape,alias,sample,(unsigned long long)c.vreg[rd][0],(unsigned long long)expected.vreg[rd][0]);}
 }
 printf("%u/%u unary/compare checks passed\n",checks-failures,checks);return failures!=0;
}
)C");
}
