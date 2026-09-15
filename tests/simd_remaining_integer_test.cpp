// Non-saturating variable shifts and scalar arithmetic, synthetic generated-C tests.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Spec{unsigned word,scalar,q,bits,u,round,kind,rd;};
int main(){
 for(unsigned word:{0x5ea24420u,0x0ee24420u,0x5ea25420u,0x5ea28420u,0x5ea28c20u,0xdee24420u,0x5ec24420u}){
  std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
  if(!miss){std::fprintf(stderr,"reserved integer decoded %08x\n",word);return 1;}
 }
 std::vector<Spec> specs;
 for(unsigned scalar=0;scalar<2;scalar++)for(unsigned q=0;q<2;q++)for(unsigned size=0;size<4;size++)
 for(unsigned u=0;u<2;u++)for(unsigned round=0;round<2;round++)for(unsigned rd=0;rd<3;rd++){
  if(scalar?(!q||size!=3):(!q&&size==3))continue;
  specs.push_back({0x0e224420|(scalar<<28)|(q<<30)|(size<<22)|(u<<29)|(round<<12)|rd,scalar,q,8u<<size,u,round,0,rd});
 }
 for(unsigned compare=0;compare<2;compare++)for(unsigned u=0;u<2;u++)for(unsigned rd=0;rd<3;rd++)
  specs.push_back({0x5ee28420|(compare<<11)|(u<<29)|rd,1,1,64,u,0,compare?2u:1u,rd});
 std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\ntypedef struct{uint64_t vreg[32][2];uint64_t guard[4];}GuestContext;");
 unsigned id=0;for(auto s:specs){std::string out;bool miss=false;suyu::recomp::Translate(s.word,0x1000,out,&miss);if(miss){std::fprintf(stderr,"missing %08x\n",s.word);return 1;}std::printf("static void f%u(GuestContext*c){%s}\n",id++,out.c_str());}
 std::puts("static void(*fn[])(GuestContext*)={");for(unsigned n=0;n<id;n++)std::printf("f%u,",n);std::puts("};\nstatic const unsigned specs[][8]={");
 for(auto s:specs)std::printf("{0x%x,%u,%u,%u,%u,%u,%u,%u},\n",s.word,s.scalar,s.q,s.bits,s.u,s.round,s.kind,s.rd);
 std::puts(R"C(};
static uint64_t seed=0x992381721badULL;
static uint64_t random64(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static uint64_t load(const unsigned char*p,unsigned n){uint64_t v=0;for(unsigned j=0;j<n;j++)v|=(uint64_t)p[j]<<(j*8);return v;}
int main(void){unsigned checks=0;for(unsigned n=0;n<sizeof(specs)/sizeof(specs[0]);n++){
 const unsigned*s=specs[n];unsigned scalar=s[1],q=s[2],bits=s[3],u=s[4],round=s[5],kind=s[6],rd=s[7],bytes=scalar?8:q?16:8;
 for(unsigned shiftcode=0;shiftcode<256;shiftcode++)for(unsigned sample=0;sample<15;sample++){
  GuestContext a,e;for(unsigned r=0;r<32;r++)for(unsigned h=0;h<2;h++)a.vreg[r][h]=sample==0?~0ULL:sample==1?0:sample==2?0x8000800080008000ULL:sample==3?1:random64();
  for(unsigned j=0;j<4;j++)a.guard[j]=random64();
  if(kind==0)for(unsigned lane=0;lane<bytes*8/bits;lane++)((unsigned char*)a.vreg[2])[lane*bits/8]=(unsigned char)(shiftcode+lane);
  if(kind==2&&sample%3==0)memcpy(a.vreg[2],a.vreg[1],16);
  e=a;memset(e.vreg[rd],0,16);
  for(unsigned lane=0;lane<bytes*8/bits;lane++){
   uint64_t v=load((unsigned char*)a.vreg[1]+lane*bits/8,bits/8),m=load((unsigned char*)a.vreg[2]+lane*bits/8,bits/8),z=v;
   if(kind==1){uint64_t carry=u?1:0,other=u?~m:m;z=0;for(unsigned j=0;j<64;j++){uint64_t sum=((v>>j)&1)+((other>>j)&1)+carry;z|=(sum&1)<<j;carry=sum/2;}}
   else if(kind==2){int same=1,common=0;for(unsigned j=0;j<64;j++){if(((v>>j)&1)!=((m>>j)&1))same=0;if(((v>>j)&1)&&((m>>j)&1))common=1;}z=(u?same:common)?~0ULL:0;}
   else{int shift=(int)(m&255);if(shift>=128)shift-=256;unsigned lost=0,sign=!u&&(v>>(bits-1));
    if(shift>=0)for(int j=0;j<shift;j++){z*=2;if(bits<64)z&=(1ULL<<bits)-1;}
    else{for(int j=0;j<-shift;j++){lost=z&1;z/=2;if(sign)z|=1ULL<<(bits-1);}if(round)z+=lost;}
   }
   for(unsigned j=0;j<bits/8;j++)((unsigned char*)e.vreg[rd])[lane*bits/8+j]=(unsigned char)(z>>(j*8));
  }
  fn[n](&a);checks++;if(memcmp(&a,&e,sizeof a)){printf("FAIL %08x shift %u sample %u\n",s[0],shiftcode,sample);return 1;}
 }
 }printf("%u generated-C remaining integer checks passed\n",checks);return 0;}
)C");
}
