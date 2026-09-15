#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts("#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\ntypedef struct{uint64_t vreg[32][2],fpcr,fpsr;}GuestContext;");for(unsigned d=0;d<2;d++)for(unsigned a=0;a<3;a++){std::string s;bool bad=false;suyu::recomp::Translate(0x1e228820|(d<<22)|a,0x1000,s,&bad);if(bad)return 1;printf("void f%u%u(GuestContext*c){%s}\n",d,a,s.c_str());}puts("typedef void(*Fn)(GuestContext*);Fn f[2][3]={{f00,f01,f02},{f10,f11,f12}};");puts(R"C(
unsigned checks,fail;
void test(unsigned d,unsigned alias,uint64_t a,uint64_t b,uint64_t expected,unsigned flags,uint64_t fpcr){GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=fpcr;c.fpsr=0x8000000;c.vreg[1][0]=a;c.vreg[2][0]=b;f[d][alias](&c);checks++;if(c.vreg[alias][0]!=expected||c.vreg[alias][1]||c.fpsr!=(flags|0x8000000)){fail++;printf("FAIL d%u alias%u got%llx expected%llx flags%llx want%x\n",d,alias,(unsigned long long)c.vreg[alias][0],(unsigned long long)expected,(unsigned long long)c.fpsr,flags);}}
int main(){for(unsigned d=0;d<2;d++){uint64_t one=d?0x3ff0000000000000ULL:0x3f800000,sign=d?0x8000000000000000ULL:0x80000000,exp=d?0x7ff0000000000000ULL:0x7f800000,q=d?0x8000000000000ULL:0x400000,min=d?0x10000000000000ULL:0x800000,half=d?0x3fe0000000000000ULL:0x3f000000;
for(unsigned alias=0;alias<3;alias++)for(unsigned mode=0;mode<4;mode++){uint64_t fpcr=(uint64_t)mode<<22;
// (1+ulp)^2=1+2ulp+ulp^2: rounding upward adds a third ulp,
// then the final sign flip makes it MORE negative, unlike negated-input mul.
test(d,alias,one+1,one+1,(one+2+(mode==1))|sign,16,fpcr);
test(d,alias,(one+1)|sign,one+1,one+2+(mode==2),16,fpcr);
test(d,alias,0,one,sign,0,fpcr);test(d,alias,sign,one,0,0,fpcr);
test(d,alias,exp,one,exp|sign,0,fpcr);test(d,alias,exp,0,exp|q|sign,1,fpcr);
test(d,alias,exp|q|7,one,exp|q|7|sign,0,fpcr);
test(d,alias,exp|1,one,exp|q|1|sign,1,fpcr);
test(d,alias,exp|q|7|sign,one,exp|q|sign,0,fpcr|(1ULL<<25));
test(d,alias,min,half,(min>>1)|sign,0,fpcr);
test(d,alias,min,half,sign,8,fpcr|(1ULL<<24));
test(d,alias,1,one,sign,128,fpcr|(1ULL<<24));
}}
printf("%u/%u FNMUL state checks passed\n",checks-fail,checks);return fail!=0;}
)C");}
