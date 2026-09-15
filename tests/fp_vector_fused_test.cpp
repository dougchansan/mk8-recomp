#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts("#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\ntypedef struct{uint64_t vreg[32][2],fpcr,fpsr;}GuestContext;");unsigned base[]={0x6e22dc20,0x4e22cc20,0x4ea2cc20,0x4f829020,0x4f821020,0x4f825020};for(unsigned o=0;o<6;o++)for(unsigned d=0;d<2;d++){std::string s;bool bad=false;suyu::recomp::Translate(base[o]|(d<<22),0x1000,s,&bad);if(bad)return 1;printf("void f%u%u(GuestContext*c){%s}\n",o,d,s.c_str());}puts("typedef void(*Fn)(GuestContext*);Fn f[6][2]={");for(unsigned o=0;o<6;o++)printf("{f%u0,f%u1},",o,o);puts("};");puts(R"C(
unsigned checks,fail;
void test(unsigned o,unsigned d,uint64_t a,uint64_t b,uint64_t z,uint64_t expected,uint64_t flags,uint64_t fpcr){GuestContext c;memset(&c,0,sizeof c);c.fpcr=fpcr;c.fpsr=0x8000000;unsigned bytes=d?8:4;for(unsigned j=0;j<16/bytes;j++){memcpy((char*)c.vreg[1]+j*bytes,&a,bytes);memcpy((char*)c.vreg[2]+j*bytes,&b,bytes);memcpy((char*)c.vreg[0]+j*bytes,&z,bytes);}f[o][d](&c);unsigned char want[16];for(unsigned j=0;j<16/bytes;j++)memcpy(want+j*bytes,&expected,bytes);checks++;if(memcmp(want,c.vreg[0],16)||c.fpsr!=(flags|0x8000000)){fail++;printf("FAIL o%u d%u got%llx exp%llx flags%llx want%llx\n",o,d,(unsigned long long)c.vreg[0][0],(unsigned long long)expected,(unsigned long long)c.fpsr,(unsigned long long)flags);}}
int main(){for(unsigned o=0;o<6;o++)for(unsigned d=0;d<2;d++){unsigned kind=o%3;uint64_t sign=d?0x8000000000000000ULL:0x80000000,one=d?0x3ff0000000000000ULL:0x3f800000,half=d?0x3fe0000000000000ULL:0x3f000000,min=d?0x10000000000000ULL:0x800000,exp=d?0x7ff0000000000000ULL:0x7f800000,q=d?0x8000000000000ULL:0x400000;
for(unsigned mode=0;mode<4;mode++){uint64_t fpcr=(uint64_t)mode<<22;
test(o,d,min^(kind==2?sign:0),half,0,min>>1,0,fpcr);
test(o,d,min^(kind==2?sign:0),half,0,0,8,fpcr|(1ULL<<24));
test(o,d,exp,0,0,exp|q,1,fpcr);
if(kind){uint64_t n=(one+1)^(kind==2?sign:0),m=one-2,z=one|sign,r=d?0xb970000000000000ULL:0xa8800000;test(o,d,n,m,z,r,0,fpcr);}
}}
printf("%u/%u vector fused checks passed\n",checks-fail,checks);return fail!=0;}
)C");}
