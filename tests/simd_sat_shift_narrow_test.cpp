// Saturating immediate narrow shifts: exact value and sticky FPSR.QC checks.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Spec{unsigned word,bits,shift,scalar,q,kind,round,alias;};
int main(){
 for(unsigned word:{0x5f409420u,0x0f409420u,0x4f889420u,0x5f488420u,0x7f408420u}){
  std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
  if(!miss){std::fprintf(stderr,"reserved narrow decoded %08x\n",word);return 1;}
 }
 std::vector<Spec> specs;
 for(unsigned scalar=0;scalar<2;scalar++)for(unsigned q=0;q<2;q++)for(unsigned bits:{8u,16u,32u})
 for(unsigned kind=0;kind<3;kind++)for(unsigned round=0;round<2;round++)for(unsigned alias=0;alias<2;alias++)
 for(unsigned shift=1;shift<=bits;shift++){
  if(scalar&&!q)continue;
  unsigned word=0x0f000400|(scalar<<28)|(q<<30)|((kind!=0)<<29)|((kind==2?16:18)<<11)|(round<<11)|((bits*2-shift)<<16)|(1<<5)|alias;
  std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);if(miss){std::fprintf(stderr,"missing %08x\n",word);return 1;}
  specs.push_back({word,bits,shift,scalar,q,kind,round,alias});
 }
 std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\ntypedef struct{uint64_t vreg[32][2];uint64_t fpsr;uint64_t guard;}GuestContext;");
 unsigned id=0;for(auto s:specs){std::string out;bool miss=false;suyu::recomp::Translate(s.word,0x1000,out,&miss);std::printf("static void f%u(GuestContext*c){%s}\n",id++,out.c_str());}
 std::puts("static void(*fn[])(GuestContext*)={");for(unsigned n=0;n<id;n++)std::printf("f%u,",n);std::puts("};\nstatic const unsigned specs[][8]={");
 for(auto s:specs)std::printf("{0x%x,%u,%u,%u,%u,%u,%u,%u},\n",s.word,s.bits,s.shift,s.scalar,s.q,s.kind,s.round,s.alias);
 std::puts(R"C(};
static uint64_t seed=0xfeedbad123ULL;
static uint64_t rand64(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static uint64_t load(const unsigned char*p,unsigned bytes){uint64_t v=0;for(unsigned j=0;j<bytes;j++)v|=(uint64_t)p[j]<<(j*8);return v;}
int main(void){unsigned checks=0;for(unsigned n=0;n<sizeof(specs)/sizeof(specs[0]);n++){
 const unsigned*s=specs[n];unsigned bits=s[1],shift=s[2],scalar=s[3],q=s[4],kind=s[5],round=s[6],rd=s[7],sb=bits*2;
 uint64_t max=(1ULL<<bits)-1,sm=sb==64?~0ULL:(1ULL<<sb)-1,div=1ULL<<shift;
 for(unsigned sample=0;sample<35;sample++){
  GuestContext a,e;for(unsigned r=0;r<32;r++)for(unsigned h=0;h<2;h++)a.vreg[r][h]=sample==0?~0ULL:sample==1?0:sample==2?0x8000800080008000ULL:rand64();
  if(sample>=3&&sample<=10){
   uint64_t edge=((kind?max:max/2)<<shift),v;
   switch(sample){case 3:v=edge;break;case 4:v=edge-1;break;case 5:v=edge+div/2-1;break;
    case 6:v=edge+div/2;break;case 7:v=(0-(1ULL<<(bits-1)))<<shift;break;
    case 8:v=((0-(1ULL<<(bits-1)))<<shift)-1;break;case 9:v=div/2-1;break;default:v=0-div/2;break;}
   for(unsigned lane=0;lane<(scalar?1:64/bits);lane++)for(unsigned j=0;j<sb/8;j++)
    ((unsigned char*)a.vreg[1])[lane*sb/8+j]=(unsigned char)(v>>(j*8));
  }
  a.fpsr=(sample%2?1ULL<<27:0)|0x80;a.guard=rand64();e=a;if(scalar||!q)memset(e.vreg[rd],0,16);
  for(unsigned lane=0;lane<(scalar?1:64/bits);lane++){
   uint64_t v=load((unsigned char*)a.vreg[1]+lane*sb/8,sb/8),result;int sat=0;
   if(kind==1){uint64_t x=v/div;if(round&&v%div>=div/2)x++;sat=x>max;result=sat?max:x;}
   else{int64_t value=(v>>(sb-1))?-1-(int64_t)(~v&sm):(int64_t)v;
    int64_t x=value/(int64_t)div,rem=value%(int64_t)div;if(rem<0){x--;rem+=(int64_t)div;}if(round&&(uint64_t)rem>=div/2)x++;
    int64_t hi=kind==2?(int64_t)max:(int64_t)(max/2),lo=kind==2?0:-hi-1;
    if(x>hi){x=hi;sat=1;}if(x<lo){x=lo;sat=1;}result=(uint64_t)x;
   }
   if(sat)e.fpsr|=1ULL<<27;
   for(unsigned j=0;j<bits/8;j++)((unsigned char*)e.vreg[rd])[lane*bits/8+(!scalar&&q?8:0)+j]=(unsigned char)(result>>(j*8));
  }
  fn[n](&a);checks++;if(memcmp(&a,&e,sizeof a)){printf("FAIL %08x sample %u\n",s[0],sample);return 1;}
 }
 }printf("%u generated-C saturating immediate narrow checks passed\n",checks);return 0;}
)C");
}
