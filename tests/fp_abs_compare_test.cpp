#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts(R"C(#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
for(unsigned op=0;op<2;op++)for(unsigned s=0;s<5;s++)for(unsigned a=0;a<3;a++){
unsigned i=(s<3?0x2e22ec20:0x7e22ec20)|(op<<23)|((s==2||s==4)?0x400000:0)|((s==1||s==2)?0x40000000:0)|a;
std::string out;bool bad=false;suyu::recomp::Translate(i,0x1000,out,&bad);if(bad)return 1;printf("void f%u%u%u(GuestContext*c){%s}\n",op,s,a,out.c_str());}
for(unsigned i:{0x2e62ec20U,0x2ee2ec20U}){std::string out;bool bad=false;suyu::recomp::Translate(i,0x1000,out,&bad);if(!bad)return 2;}
puts("typedef void(*Fn)(GuestContext*);Fn f[2][5][3]={");for(unsigned op=0;op<2;op++){puts("{");for(unsigned s=0;s<5;s++)printf("{f%u%u0,f%u%u1,f%u%u2},",op,s,op,s,op,s);puts("},");}puts("};");
puts(R"C(
unsigned checks,failures;
int main(){
for(unsigned op=0;op<2;op++)for(unsigned s=0;s<5;s++)for(unsigned a=0;a<3;a++)for(unsigned flush=0;flush<2;flush++){
int d=s==2||s==4;unsigned bytes=d?8:4,active=s>=3?1:s?16/bytes:2;
uint64_t sign=d?0x8000000000000000ULL:0x80000000,exp=d?0x7ff0000000000000ULL:0x7f800000,quiet=d?0x8000000000000ULL:0x400000;
uint64_t v[]={0,sign,1,sign|1,d?0x4000000000000000ULL:0x40000000,exp,exp|sign,exp|1,exp|quiet|1};
for(unsigned n=0;n<9;n++)for(unsigned m=0;m<9;m++){
GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=(uint64_t)flush<<24;c.fpsr=0;
for(unsigned j=0;j<16/bytes;j++){memcpy((char*)c.vreg[1]+j*bytes,&v[n],bytes);memcpy((char*)c.vreg[2]+j*bytes,&v[m],bytes);}
unsigned flags=0;int truth=0;if(n>=7||m>=7)flags|=1;
uint64_t nb=v[n],mb=v[m];if(flush){if(n==2||n==3){nb&=sign;flags|=128;}if(m==2||m==3){mb&=sign;flags|=128;}}
if(n<7&&m<7){double x,y;if(d){memcpy(&x,&nb,8);memcpy(&y,&mb,8);}else{float xx,yy;memcpy(&xx,&nb,4);memcpy(&yy,&mb,4);x=xx;y=yy;}truth=op?fabs(x)>fabs(y):fabs(x)>=fabs(y);}
unsigned char expected[16]={0};if(truth)memset(expected,255,active*bytes);
f[op][s][a](&c);checks++;if(memcmp(expected,c.vreg[a],16)||c.fpsr!=flags){failures++;}
}}
printf("%u/%u absolute compare checks passed\n",checks-failures,checks);return failures!=0;
}
)C");}
