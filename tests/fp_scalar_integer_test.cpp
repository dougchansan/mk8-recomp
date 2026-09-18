#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts("#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\ntypedef struct{uint64_t vreg[32][2],fpcr,fpsr;}GuestContext;");
for(unsigned op=0;op<4;op++)for(unsigned d=0;d<2;d++)for(unsigned a=0;a<2;a++){std::string s;bool bad=false;suyu::recomp::Translate((op&2?0x5e21c820:0x5e21a820)|(op&1?0x20000000:0)|(d<<22)|a,0x1000,s,&bad);if(bad)return 1;printf("void f%u%u%u(GuestContext*c){%s}\n",op,d,a,s.c_str());}
puts("typedef void(*Fn)(GuestContext*);Fn f[4][2][2]={");for(unsigned o=0;o<4;o++)printf("{{f%u00,f%u01},{f%u10,f%u11}},",o,o,o,o);puts("};");
puts(R"C(
int main(){unsigned checks=0,fail=0;double input[]={0.,-0.,0.5,-0.5,1.5,-1.5,2.5,-2.5,8.,-8.};
int expect[2][10]={{0,0,0,0,2,-2,2,-2,8,-8},{0,0,1,-1,2,-2,3,-3,8,-8}};
for(unsigned o=0;o<4;o++)for(unsigned d=0;d<2;d++)for(unsigned a=0;a<2;a++)for(unsigned j=0;j<10;j++){
GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=0;c.fpsr=0;if(d)memcpy(c.vreg[1],&input[j],8);else{float x=(float)input[j];memcpy(c.vreg[1],&x,4);}
int value=expect[o>>1][j];unsigned flags=j>=2&&j<8?16:0;if((o&1)&&value<0){value=0;flags=1;}
uint64_t expected=(uint64_t)(int64_t)value;if(!d)expected=(uint32_t)expected;
f[o][d][a](&c);checks++;if(c.vreg[a][0]!=expected||c.vreg[a][1]||c.fpsr!=flags)fail++;
}printf("%u/%u scalar integer checks passed\n",checks-fail,checks);return fail!=0;}
)C");}
