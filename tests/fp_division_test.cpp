#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts("#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\n#include <math.h>\n#include <fenv.h>\ntypedef struct{uint64_t vreg[32][2],fpcr,fpsr;}GuestContext;");for(unsigned shape=0;shape<5;shape++)for(unsigned alias=0;alias<3;alias++){unsigned bits=shape<2?0x1e221820:0x2e22fc20;if(shape==1||shape==4)bits|=1<<22;if(shape==3||shape==4)bits|=1<<30;std::string s;bool bad=false;suyu::recomp::Translate(bits|alias,0x1000,s,&bad);if(bad)return 1;printf("void f%u%u(GuestContext*c){%s}\n",shape,alias,s.c_str());}puts("typedef void(*Fn)(GuestContext*);Fn f[5][3]={");for(unsigned s=0;s<5;s++)printf("{f%u0,f%u1,f%u2},",s,s,s);puts("};");puts(R"C(
unsigned checks,fail;
void test(unsigned s,unsigned alias,uint64_t a,uint64_t b,uint64_t expected,unsigned flags,uint64_t fpcr){unsigned bytes=s==1||s==4?8:4,lanes=s<2?1:s==2?2:16/bytes;GuestContext c;memset(&c,0,sizeof c);c.fpcr=fpcr;c.fpsr=0x8000000;for(unsigned j=0;j<16/bytes;j++){memcpy((char*)c.vreg[1]+j*bytes,&a,bytes);memcpy((char*)c.vreg[2]+j*bytes,&b,bytes);}f[s][alias](&c);unsigned char want[16]={0};for(unsigned j=0;j<lanes;j++)memcpy(want+j*bytes,&expected,bytes);checks++;if(memcmp(want,c.vreg[alias],16)||c.fpsr!=(flags|0x8000000)){fail++;if(fail<12)printf("FAIL s%u a%u in%llx/%llx got%llx expected%llx flags%llx want%x\n",s,alias,(unsigned long long)a,(unsigned long long)b,(unsigned long long)c.vreg[alias][0],(unsigned long long)expected,(unsigned long long)c.fpsr,flags);}}
int main(){uint64_t seed=0x47179ace;int modes[]={FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO};for(unsigned s=0;s<5;s++){int d=s==1||s==4;uint64_t one=d?0x3ff0000000000000ULL:0x3f800000,two=d?0x4000000000000000ULL:0x40000000,sign=d?0x8000000000000000ULL:0x80000000,exp=d?0x7ff0000000000000ULL:0x7f800000,quiet=d?0x8000000000000ULL:0x400000,min=d?0x10000000000000ULL:0x800000;
for(unsigned mode=0;mode<4;mode++){uint64_t fpcr=(uint64_t)mode<<22;fesetround(modes[mode]);
test(s,0,one,0,exp,2,fpcr);test(s,1,0,0,exp|quiet,1,fpcr);test(s,2,exp,exp,exp|quiet,1,fpcr);
test(s,0,sign,one,sign,0,fpcr);test(s,1,min,two,min>>1,0,fpcr);test(s,2,min,two,0,8,fpcr|(1ULL<<24));
test(s,0,1,one,0,128,fpcr|(1ULL<<24));test(s,1,exp|1,one,exp|quiet|1,1,fpcr);test(s,2,exp|quiet|3,one,exp|quiet,0,fpcr|(1ULL<<25));
for(unsigned j=0;j<2000;j++){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;uint64_t a=seed;seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;uint64_t b=seed;if(!d){a=(uint32_t)a;b=(uint32_t)b;}if((a&exp)==exp||(b&exp)==exp)continue;uint64_t expected=0;int flags;
if(d){volatile double x,y,z;memcpy((void*)&x,&a,8);memcpy((void*)&y,&b,8);feclearexcept(FE_ALL_EXCEPT);z=x/y;flags=fetestexcept(FE_ALL_EXCEPT);memcpy(&expected,(const void*)&z,8);}else{volatile float x,y,z;memcpy((void*)&x,&a,4);memcpy((void*)&y,&b,4);feclearexcept(FE_ALL_EXCEPT);z=x/y;flags=fetestexcept(FE_ALL_EXCEPT);memcpy(&expected,(const void*)&z,4);}
unsigned fpsr=(flags&FE_INVALID?1:0)|(flags&FE_DIVBYZERO?2:0)|(flags&FE_OVERFLOW?4:0)|(flags&FE_UNDERFLOW?8:0)|(flags&FE_INEXACT?16:0);test(s,j%3,a,b,expected,fpsr,fpcr);
}
}}fesetround(FE_TONEAREST);printf("%u/%u division checks passed\n",checks-fail,checks);return fail!=0;}
)C");}
