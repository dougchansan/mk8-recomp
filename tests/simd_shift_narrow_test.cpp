// Synthetic immediate SIMD shifts, inserts and narrowing: generated-C exact checks.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Case { unsigned op,u,q,scalar,bits,shift,alias,word; };
int main() {
 { std::string out; bool miss=false; suyu::recomp::Translate(0x4f095420,0x1000,out,&miss);
   if(!miss){std::fprintf(stderr,"hybrid SHL gate lost\n");return 1;} }
 suyu::recomp::g_translate_all=true;
 // GNU as/objdump confirms these malformed widths and neighboring masks undefined.
 for(unsigned word: {0x0f402420u,0x5f082420u,0x0f408c20u,0x2ee13820u,0xae213820u,0x4f084420u,
                     0x4f880420u,0x0f888420u}) {
  std::string out; bool miss=false; suyu::recomp::Translate(word,0x1000,out,&miss);
  if(!miss){std::fprintf(stderr,"reserved shift decoded: %08x\n",word);return 1;}
 }
 std::vector<Case> cases;
 for(unsigned scalar=0;scalar<2;scalar++) for(unsigned q=0;q<2;q++)
 for(unsigned bits: {8u,16u,32u,64u}) for(unsigned op: {0u,2u,4u,6u,8u,10u,16u,17u,32u})
 for(unsigned u=0;u<2;u++) for(unsigned alias=0;alias<2;alias++) {
  bool narrow=op==16||op==17, left=op==10;
  if((scalar&&(!q||bits!=64||narrow||op==32))||(!scalar&&bits==64&&(!q||narrow||op==32))||
     (op==8&&!u)||(narrow&&u)||(op==32&&!u)) continue;
  for(unsigned step=0;step<3;step++) {
   unsigned shift=op==32?bits:left?(step==0?0:step==1?1:bits-1):(step==0?1:step==1?bits/2:bits);
   unsigned imm=left?bits+shift:bits*2-shift;
   unsigned word=op==32?0x2E213800|((bits==8?0:bits==16?1:2)<<22):
       0x0F000400|(scalar<<28)|(u<<29)|(op<<11)|(imm<<16);
   word|=(q<<30)|(1<<5)|(alias?1:0);
   std::string out; bool miss=false; suyu::recomp::Translate(word,0x1000,out,&miss);
   if(miss){std::fprintf(stderr,"missing %08x\n",word);return 1;}
   cases.push_back({op,u,q,scalar,bits,shift,alias,word});
  }
 }
 std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\ntypedef struct { uint64_t vreg[32][2]; uint64_t guard[4]; } GuestContext;");
 unsigned id=0;
 for(auto c:cases){std::string out;bool miss=false;suyu::recomp::Translate(c.word,0x1000,out,&miss);std::printf("static void f%u(GuestContext*c){%s}\n",id++,out.c_str());}
 std::puts("static void(*fn[])(GuestContext*)={");for(unsigned n=0;n<id;n++)std::printf("f%u,",n);std::puts("};");
 std::puts("static const unsigned specs[][8]={");for(auto c:cases)std::printf("{%u,%u,%u,%u,%u,%u,%u,0x%x},\n",c.op,c.u,c.q,c.scalar,c.bits,c.shift,c.alias,c.word);std::puts("};");
 std::puts(R"C(
static uint64_t seed=0x716e249835a1cd7bULL;
static uint64_t random_word(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static uint64_t load(const unsigned char*p,unsigned bytes){uint64_t r=0;for(unsigned j=0;j<bytes;j++)r|=(uint64_t)p[j]<<(j*8);return r;}
int main(void){unsigned checks=0;
 for(unsigned n=0;n<sizeof(specs)/sizeof(specs[0]);n++){
  const unsigned *s=specs[n];unsigned op=s[0],u=s[1],q=s[2],scalar=s[3],bits=s[4],shift=s[5],rd=s[6];
  unsigned narrow=op==16||op==17,srcbits=narrow?bits*2:bits,dstbits=op==32?bits*2:bits;
  unsigned bytes=scalar?8:q?16:8,lanes=narrow||op==32?64/bits:bytes*8/bits;
  for(unsigned sample=0;sample<103;sample++){
   GuestContext a,e;for(unsigned r=0;r<32;r++)for(unsigned h=0;h<2;h++)a.vreg[r][h]=sample==0?~0ULL:sample==1?0:sample==2?0x8000800080008000ULL:random_word();
   for(unsigned g=0;g<4;g++)a.guard[g]=random_word();e=a;
   if(!narrow||!q)memset(e.vreg[rd],0,16);
   for(unsigned lane=0;lane<lanes;lane++){
    unsigned srcOff=lane*srcbits/8+(op==32&&q?8:0),dstOff=lane*dstbits/8+(narrow&&q?8:0);
    uint64_t v=load((unsigned char*)a.vreg[1]+srcOff,srcbits/8),d=load((unsigned char*)a.vreg[rd]+lane*bits/8,bits/8),z=v;
    if(op==10||op==32){for(unsigned j=0;j<shift;j++)z*=2;if(op==10&&u)for(unsigned j=0;j<shift;j++)if((d>>j)&1)z|=1ULL<<j;}
    else {
     unsigned lost=0,sign=!u&&!narrow&&(v>>(bits-1));
     for(unsigned j=0;j<shift;j++){lost=z&1;z/=2;if(sign)z|=1ULL<<(bits-1);}
     if(op==4||op==6||op==17)z+=lost;
     if(op==2||op==6)z+=d;
     if(op==8)for(unsigned j=bits-shift;j<bits;j++)if((d>>j)&1)z|=1ULL<<j;
    }
    for(unsigned j=0;j<dstbits/8;j++)((unsigned char*)e.vreg[rd])[dstOff+j]=(unsigned char)(z>>(j*8));
   }
   fn[n](&a);checks++;if(memcmp(&a,&e,sizeof a)){printf("FAIL %08x sample %u\n",s[7],sample);return 1;}
  }
 }
 printf("%u generated-C shift/insert/narrow checks passed\n",checks);return 0;
}
)C");
}
