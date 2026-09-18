#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts("#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\ntypedef struct{uint64_t vreg[32][2],fpcr,fpsr;}GuestContext;");for(unsigned d=0;d<2;d++)for(unsigned mul=0;mul<2;mul++){std::string s;bool bad=false;suyu::recomp::Translate((mul?0x1e220820:0x1f020c20)|(d<<22),0x1000,s,&bad);if(bad)return 1;printf("void f%u%u(GuestContext*c){%s}\n",d,mul,s.c_str());}puts("typedef void(*Fn)(GuestContext*);Fn f[2][2]={{f00,f01},{f10,f11}};");puts(R"C(
unsigned checks,fail;
void test(unsigned d,unsigned mul,uint64_t a,uint64_t b,uint64_t z,uint64_t expected,uint64_t flags,uint64_t fpcr){GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=fpcr;c.fpsr=0x8000000;c.vreg[1][0]=a;c.vreg[2][0]=b;c.vreg[3][0]=z;f[d][mul](&c);checks++;if(c.vreg[0][0]!=expected||c.vreg[0][1]||c.fpsr!=(flags|0x8000000)){fail++;printf("FAIL d%u mul%u got%llx expected%llx fpsr%llx flags%llx\n",d,mul,(unsigned long long)c.vreg[0][0],(unsigned long long)expected,(unsigned long long)c.fpsr,(unsigned long long)flags);}}
int main(){for(unsigned d=0;d<2;d++){uint64_t one=d?0x3ff0000000000000ULL:0x3f800000,half=d?0x3fe0000000000000ULL:0x3f000000,sign=d?0x8000000000000000ULL:0x80000000,exp=d?0x7ff0000000000000ULL:0x7f800000,quiet=d?0x8000000000000ULL:0x400000,min=d?0x10000000000000ULL:0x800000,half_ulp=d?0x3ca0000000000000ULL:0x33800000;
for(unsigned mode=0;mode<4;mode++){uint64_t fpcr=(uint64_t)mode<<22;
test(d,0,one,one,half_ulp,one+(mode==1),16,fpcr);
test(d,0,one,one,one|sign,mode==2?sign:0,0,fpcr);
test(d,1,sign,one,0,sign,0,fpcr);
test(d,1,exp-1,one+((d?0x10000000000000ULL:0x800000)),0,mode<2?exp:exp-1,20,fpcr);
for(unsigned mul=0;mul<2;mul++){
test(d,mul,min,half,0,min>>1,0,fpcr);
test(d,mul,min,half,0,0,8,fpcr|(1ULL<<24));
test(d,mul,1,one,0,0,128,fpcr|(1ULL<<24));
test(d,mul,exp,0,0,exp|quiet,1,fpcr);
test(d,mul,exp|1,one,0,exp|quiet|1,1,fpcr);
test(d,mul,exp|quiet|35,one,0,exp|quiet,0,fpcr|(1ULL<<25));
}
test(d,0,exp,one,exp|sign,exp|quiet,1,fpcr);
test(d,0,exp,0,exp|quiet|35,exp|quiet,1,fpcr);
test(d,0,exp|quiet|3,exp|quiet|4,exp|quiet|5,exp|quiet|5,0,fpcr);
test(d,0,exp|1,one,exp|quiet|5,exp|quiet|1,1,fpcr);
}}
printf("%u/%u FMA state checks passed\n",checks-fail,checks);return fail!=0;}
)C");}
