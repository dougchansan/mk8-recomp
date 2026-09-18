// Estimate instructions share opcode 0x1D with integer-to-FP conversions.
// Estimates must use their native helper; adjacent conversions must retain their
// own semantics. Full estimate values/state are tested by fp_estimate_integration_test.cpp.
#include <cstdio>
#include <cstring>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(int argc,char**argv){
 bool assembly=argc>1&&!std::strcmp(argv[1],"--assembly");
 unsigned failures=0;
 if(assembly)puts(".text");
 else puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
 for(unsigned estimate=0;estimate<2;++estimate)for(unsigned op=0;op<2;++op)
 for(unsigned shape=0;shape<5;++shape)for(unsigned alias=0;alias<3;++alias){
  unsigned rd=alias==2?31:alias,rn=alias==2?31:1;
  bool scalar=shape>=3,dbl=shape==2||shape==4;
  unsigned word=(scalar?0x5e21d800:0x0e21d800)|(op<<29)|(estimate<<23)|
      (!scalar&&shape?0x40000000:0)|(dbl?0x400000:0)|(rn<<5)|rd;
  if(assembly){
   const char*name=estimate?(op?"frsqrte":"frecpe"):(op?"ucvtf":"scvtf");
   printf(".inst 0x%08x\n%s ",word,name);
   if(scalar)printf("%c%u,%c%u\n",dbl?'d':'s',rd,dbl?'d':'s',rn);
   else printf("v%u.%s,v%u.%s\n",rd,dbl?"2d":shape?"4s":"2s",rn,dbl?"2d":shape?"4s":"2s");
   continue;
  }
  std::string s;bool miss=false;suyu::recomp::Translate(word,0x1000,s,&miss);
  if(estimate){
   if(miss||s.find("recomp_fp_estimate(")==std::string::npos){
    ++failures;std::fprintf(stderr,"FP estimate missing native helper %08x\n",word);
   }
   continue;
  }
  if(miss){std::fprintf(stderr,"conversion missing %08x\n",word);return 2;}
  if(s.find("recomp_fp_estimate(")!=std::string::npos){
   std::fprintf(stderr,"conversion decoded as estimate %08x\n",word);return 2;
  }
  printf("static void f%u%u%u(GuestContext*c){%s}\n",op,shape,alias,s.c_str());
 }
 if(assembly)return 0;
 printf("static const unsigned decode_failures=%u;\n",failures);
 puts("typedef void(*Fn)(GuestContext*);static Fn fn[2][5][3]={");
 for(unsigned op=0;op<2;++op){puts("{");for(unsigned shape=0;shape<5;++shape)
  printf("{f%u%u0,f%u%u1,f%u%u2},",op,shape,op,shape,op,shape);puts("},");}
 puts(R"C(};
int main(void){unsigned checks=0,failures=decode_failures;
 for(unsigned op=0;op<2;++op)for(unsigned shape=0;shape<5;++shape)
 for(unsigned alias=0;alias<3;++alias)for(unsigned sample=0;sample<4;++sample){
  unsigned scalar=shape>=3,dbl=shape==2||shape==4,width=dbl?8:4;
  unsigned active=scalar?1:shape?16/width:2,rd=alias==2?31:alias,rn=alias==2?31:1;
  GuestContext c,expected;memset(&c,0xa5,sizeof c);c.fpcr=0;c.fpsr=0;
  for(unsigned lane=0;lane<16/width;++lane){
   int64_t n=(sample+lane)%4==0?0:(sample+lane)%4==1?1:(sample+lane)%4==2?1024:op?4096:-4096;
   memcpy((char*)c.vreg[rn]+lane*width,&n,width);
  }
  expected=c;memset(expected.vreg[rd],0,16);
  for(unsigned lane=0;lane<active;++lane){
   int64_t n=(sample+lane)%4==0?0:(sample+lane)%4==1?1:(sample+lane)%4==2?1024:op?4096:-4096;
   if(dbl){double value=(double)n;memcpy((char*)expected.vreg[rd]+lane*width,&value,width);}
   else {float value=(float)n;memcpy((char*)expected.vreg[rd]+lane*width,&value,width);}
  }
  fn[op][shape][alias](&c);++checks;if(memcmp(&c,&expected,sizeof c)){++failures;printf("conversion mismatch op%u shape%u alias%u sample%u\n",op,shape,alias,sample);}
 }
 printf("%u conversion cases, 30 FP estimate decode cases, %u failures\n",checks,failures);return failures!=0;
}
)C");
}
