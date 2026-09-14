// Compile with -I third_party/suyu/src; compile and execute the emitted C.
// --assembly emits mnemonic/encoding pairs for independent GNU verification.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(int argc,char**argv) {
    const unsigned words[]={0x4ea0e802u,0x4ea0f801u,0x6ea1d823u};
    const char* assembly[]={"fcmlt v2.4s,v0.4s,#0.0","fabs v1.4s,v0.4s","frsqrte v3.4s,v1.4s"};
    if(argc>1&&std::string(argv[1])=="--assembly") {
        puts(".text");for(unsigned j=0;j<3;j++)printf(".inst 0x%08x\n%s\n",words[j],assembly[j]);
        return 0;
    }
    puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n"
         "typedef struct {uint64_t x[32],vreg[32][2],fpcr,fpsr;} GuestContext;");
    puts(suyu::recomp::EstimateRuntimeC());
    puts("static void sequence(GuestContext*c){");
    for(unsigned word:words) {
        std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
        if(miss)return 2;
        puts(out.c_str());
    }
    puts(R"C(}
int main(void){
 const uint32_t input[4]={0x3f800000,0xc0800000,0x41800000,0xc2800000};
 const uint32_t masks[4]={0,0xffffffffu,0,0xffffffffu};
 const uint32_t magnitudes[4]={0x3f800000,0x40800000,0x41800000,0x42800000};
 /* These estimate bits were verified against the software semantic oracle. */
 const uint32_t estimates[4]={0x3f7f8000,0x3eff8000,0x3e7f8000,0x3dff8000};
 unsigned failures=0;
 for(unsigned control=0;control<16;control++){
  GuestContext actual,expected;memset(&actual,0xa5,sizeof actual);
  actual.fpcr=(uint64_t)control<<22;actual.fpsr=0x08000000;
  memcpy(actual.vreg[0],input,16);expected=actual;
  memcpy(expected.vreg[1],magnitudes,16);memcpy(expected.vreg[2],masks,16);
  memcpy(expected.vreg[3],estimates,16);
  sequence(&actual);
  if(memcmp(&actual,&expected,sizeof actual)){failures++;printf("FAIL FPCR=%llx\n",(unsigned long long)actual.fpcr);}
 }
 printf("normalization sequence: %u/16 passed\n",16-failures);return failures!=0;
})C");
}
