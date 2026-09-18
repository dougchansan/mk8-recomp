#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){
 puts(R"C(#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fenv.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
 for(unsigned w=0;w<2;w++)for(unsigned q=0;q<2;q++)for(unsigned a=0;a<2;a++){
 std::string s;bool bad=false;unsigned b=(w?0x0e617800:0x0e616800)|(q<<30)|32|a;
 suyu::recomp::Translate(b,0x1000,s,&bad);if(bad)return 1;printf("void f%u%u%u(GuestContext*c){%s}\n",w,q,a,s.c_str());}
 for(unsigned b:{0x2e616820U}){std::string s;bool bad=false;suyu::recomp::Translate(b,0x1000,s,&bad);if(!bad)return 2;}
 puts("typedef void(*Fn)(GuestContext*);Fn f[2][2][2]={{{f000,f001},{f010,f011}},{{f100,f101},{f110,f111}}};");
 puts(R"C(
unsigned checks,failures;
void test(unsigned w,unsigned q,unsigned a,uint64_t in,uint64_t expect,uint64_t flags,uint64_t fpcr,int checkflags){
 GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=fpcr;c.fpsr=0x8000000;
 unsigned bytes=w?4:8;for(unsigned j=0;j<16/bytes;j++)memcpy((char*)c.vreg[1]+j*bytes,&in,bytes);
 unsigned char out[16];memcpy(out,c.vreg[a],16);
 if(w){memcpy(out,&expect,8);memcpy(out+8,&expect,8);}else{memcpy(out+(q?8:0),&expect,4);memcpy(out+(q?12:4),&expect,4);if(!q)memset(out+8,0,8);}
 f[w][q][a](&c);checks++;if(memcmp(c.vreg[a],out,16)||(checkflags&&c.fpsr!=(flags|0x8000000))){failures++;if(failures<15)printf("FAIL w%u q%u a%u in%llx got%llx want%llx flags%llx expected%llx\n",w,q,a,(unsigned long long)in,(unsigned long long)c.vreg[a][0],(unsigned long long)expect,(unsigned long long)c.fpsr,(unsigned long long)flags);}
}
int main(){
const uint64_t wide[][3]={{0,0,0},{0x80000000,0x8000000000000000ULL,0},{0x3f800000,0x3ff0000000000000ULL,0},{1,0x36a0000000000000ULL,0},{0x007fffff,0x380fffffc0000000ULL,0},{0x7f800000,0x7ff0000000000000ULL,0},{0x7fc00001,0x7ff8000020000000ULL,0},{0xff800001,0xfff8000020000000ULL,1}};
const uint64_t narrow[][3]={{0,0,0},{0x8000000000000000ULL,0x80000000,0},{0x3ff0000000000000ULL,0x3f800000,0},{0x36a0000000000000ULL,1,0},{0x380fffffc0000000ULL,0x007fffff,0},{0x7ff0000000000000ULL,0x7f800000,0},{0x7ff8000000000001ULL,0x7fc00000,0},{0xfff0000000000001ULL,0xffc00000,1},{0x3ff0000010000000ULL,0x3f800000,16},{0x3ff0000030000000ULL,0x3f800002,16},{0x3690000000000000ULL,0,24},{0x380fffffe0000000ULL,0x00800000,24},{0x47f0000000000000ULL,0x7f800000,20}};
for(unsigned q=0;q<2;q++)for(unsigned a=0;a<2;a++){
 for(unsigned j=0;j<sizeof wide/sizeof *wide;j++)test(1,q,a,wide[j][0],wide[j][1],wide[j][2],0,1);
 for(unsigned j=0;j<sizeof narrow/sizeof *narrow;j++)test(0,q,a,narrow[j][0],narrow[j][1],narrow[j][2],0,1);
 test(1,q,a,1,0,128,1ULL<<24,1);test(0,q,a,1,0,128,1ULL<<24,1);
 test(0,q,a,0x36a0000000000000ULL,0,8,1ULL<<24,1);
 test(1,q,a,0xff800001,0x7ff8000000000000ULL,1,1ULL<<25,1);
 test(0,q,a,0xfff0000000000001ULL,0x7fc00000,1,1ULL<<25,1);
 for(unsigned m=0;m<4;m++){
  test(0,q,a,0x3ff0000010000000ULL,m==1?0x3f800001:0x3f800000,16,(uint64_t)m<<22,1);
  test(0,q,a,0xbff0000010000000ULL,m==2?0xbf800001:0xbf800000,16,(uint64_t)m<<22,1);
  test(0,q,a,0x7fefffffffffffffULL,(m==0||m==1)?0x7f800000:0x7f7fffff,20,(uint64_t)m<<22,1);
  test(0,q,a,1,m==1?1:0,24,(uint64_t)m<<22,1);
 }
}
// Independent host hardware conversion reference for finite random inputs. Guest
// implementation is integer-only; test compiler honors dynamic host rounding.
uint64_t seed=0x193baaa5;int modes[]={FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO};
for(unsigned m=0;m<4;m++){fesetround(modes[m]);for(unsigned j=0;j<20000;j++){
 seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;
 uint64_t in=seed;if(((in>>52)&2047)==2047)continue;volatile double d;memcpy((void*)&d,&in,8);volatile float s=(float)d;uint32_t expected;memcpy(&expected,(const void*)&s,4);
 test(0,j&1,(j>>1)&1,in,expected,0,(uint64_t)m<<22,0);
 uint32_t si=(uint32_t)in;if(((si>>23)&255)==255)continue;memcpy((void*)&s,&si,4);d=(double)s;uint64_t de;memcpy(&de,(const void*)&d,8);
 test(1,j&1,(j>>1)&1,si,de,0,(uint64_t)m<<22,0);
}}fesetround(FE_TONEAREST);
printf("%u/%u precision checks passed\n",checks-failures,checks);return failures!=0;
}
)C");
}
