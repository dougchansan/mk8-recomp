// Compile generator, redirect stdout to C, then compile and run the C.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main() {
    // Other SYS operations must not be translated as zeroing stores.
    for (unsigned word : {0xd50b7440u, 0xd50b7420u ^ 0x10000u,
                          0xd50b7420u ^ 0x1000u, 0xd50b7520u}) {
        std::string out; bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(out.find("recomp_store64")!=std::string::npos) {
            std::fprintf(stderr,"neighbor zeroed memory %08x\n",word);return 1;
        }
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t x[32],vreg[32][2],nzcv,fpcr,fpsr; } GuestContext;
typedef void(*Fn)(GuestContext*);
static unsigned writes,bad;
static uint64_t addresses[8],values[8];
static void recomp_store64(GuestContext*c,uint64_t address,uint64_t value){
 (void)c;if(writes>=8){bad=1;return;}addresses[writes]=address;values[writes++]=value;
}
)C");
    for(unsigned rt=0;rt<32;rt++) {
        std::string out; bool miss=false;
        suyu::recomp::Translate(0xd50b7420u|rt,0x1000,out,&miss);
        if(miss){std::fprintf(stderr,"DC ZVA missing rt=%u\n",rt);return 1;}
        std::printf("static void f%u(GuestContext*c){%s}\n",rt,out.c_str());
    }
    std::puts("static Fn fn[]={");for(unsigned rt=0;rt<32;rt++)std::printf("f%u,\n",rt);
    std::puts(R"C(};
int main(void){unsigned checks=0;
 const uint64_t bases[]={0,64,UINT64_C(0x123456780000),UINT64_MAX-63};
 for(unsigned rt=0;rt<32;rt++)for(unsigned b=0;b<4;b++)for(unsigned off=0;off<64;off++){
  GuestContext c,before;memset(&c,0xa5,sizeof(c));c.x[rt]=bases[b]+off;before=c;
  writes=bad=0;memset(addresses,0xff,sizeof(addresses));memset(values,0xff,sizeof(values));fn[rt](&c);
  uint64_t va=rt==31?0:before.x[rt],base=(va/64)*64;
  if(writes!=8||bad||memcmp(&c,&before,sizeof(c))){printf("state/count failure rt=%u offset=%u\n",rt,off);return 1;}
  unsigned seen=0;for(unsigned j=0;j<8;j++){
   if(values[j]!=0||addresses[j]%8||addresses[j]/64!=base/64){puts("write bounds/value failure");return 1;}
   unsigned lane=(unsigned)((addresses[j]-base)/8);if(seen&(1u<<lane)){puts("duplicate write");return 1;}seen|=1u<<lane;
  }
  if(seen!=255){puts("incomplete zero block");return 1;}checks++;
 }printf("%u DC ZVA generated-C checks passed\n",checks);return 0;}
)C");
}
