#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts(R"C(#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
unsigned b[]={0x0e217820,0x4e217820,0x0e216820,0x4e216820,0x1e624020,0x1e22c020,0x1e23c020,0x1e63c020,0x1ee24020,0x1ee2c020};
for(unsigned j=0;j<10;j++)for(unsigned a=0;a<2;a++){std::string s;bool bad=false;suyu::recomp::Translate(b[j]|a,0x1000,s,&bad);if(bad)return 1;printf("void f%u%u(GuestContext*c){%s}\n",j,a,s.c_str());}
puts("typedef void(*Fn)(GuestContext*);Fn f[10][2]={");for(unsigned j=0;j<10;j++)printf("{f%u0,f%u1},",j,j);puts("};");
puts(R"C(
unsigned checks,failures;
uint64_t bits(double x,unsigned bytes){uint64_t b=0;if(bytes==8)memcpy(&b,&x,8);else{float s=(float)x;memcpy(&b,&s,4);}return b;}
void test(unsigned op,unsigned alias,uint64_t in,uint64_t expect,uint64_t flags,uint64_t fpcr){
unsigned from[]={2,2,4,4,8,4,4,8,2,2},to[]={4,4,2,2,4,8,2,2,4,8};
GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=fpcr;c.fpsr=0x8000000;
for(unsigned j=0;j<16/from[op];j++)memcpy((char*)c.vreg[1]+j*from[op],&in,from[op]);
unsigned char out[16]={0};if(op==3)memcpy(out,c.vreg[alias],8);
for(unsigned j=0;j<(op<4?4:1);j++)memcpy(out+(op==3?8:0)+j*to[op],&expect,to[op]);
f[op][alias](&c);checks++;if(memcmp(out,c.vreg[alias],16)||c.fpsr!=(flags|0x8000000)){failures++;if(failures<12)printf("FAIL op%u a%u in%llx got%llx exp%llx flags%llx want%llx\n",op,alias,(unsigned long long)in,(unsigned long long)c.vreg[alias][0],(unsigned long long)expect,(unsigned long long)c.fpsr,(unsigned long long)flags);}
}
double halfvalue(unsigned h){unsigned e=(h>>10)&31,f=h&1023;return ldexp(e?1024+f:f,e?(int)e-25:-24);}
int main(){
// Exhaustive mathematical half expansion, both IEEE and AHP. FZ and FZ16
// deliberately set: these conversions must preserve half subnormals.
for(unsigned ahp=0;ahp<2;ahp++)for(unsigned h=0;h<65536;h++){
unsigned mag=h&32767,e=mag>>10,frac=mag&1023;uint64_t flags=(!ahp&&e==31&&frac&&!(frac&512))?1:0;
for(unsigned op=0;op<10;op++){if(op!=0&&op!=1&&op!=8&&op!=9)continue;unsigned bytes=op==9?8:4;uint64_t expected;
if(!ahp&&e==31){expected=(bytes==8?0x7ff0000000000000ULL:0x7f800000)|((uint64_t)(h>>15)<<(bytes*8-1));if(frac)expected|=(bytes==8?0x8000000000000ULL:0x400000)|((uint64_t)frac<<(bytes==8?42:13));}
else {double v=halfvalue(mag);if(h&32768)v=-v;expected=bits(v,bytes);}
test(op,h&1,h,expected,flags,((uint64_t)ahp<<26)|(1ULL<<24)|(1ULL<<19));
}}
// Every finite adjacent half midpoint, both signs and four guest modes.
for(unsigned h=0;h<0x7bff;h++){double mid=(halfvalue(h)+halfvalue(h+1))*0.5;
for(unsigned sign=0;sign<2;sign++)for(unsigned mode=0;mode<4;mode++){
unsigned result=(mode==0?(h+(h&1)):mode==1?h+!sign:mode==2?h+sign:h)|(sign<<15);
unsigned flags=16|(h<1024?8:0);double x=sign?-mid:mid;
for(unsigned op=2;op<8;op++){if(op==4||op==5)continue;test(op,h&1,bits(x,op==7?8:4),result,flags,(uint64_t)mode<<22);}
}}
for(unsigned op=2;op<8;op++){if(op==4||op==5)continue;unsigned bytes=op==7?8:4;
test(op,0,bits(INFINITY,bytes),0x7fff,1,1ULL<<26);
test(op,1,bits(-INFINITY,bytes),0xffff,1,1ULL<<26);
test(op,0,bytes==8?0x7ff8000000000000ULL:0x7fc00000,0,1,1ULL<<26);
test(op,0,bits(65536,bytes),0x7c00,0,1ULL<<26);
test(op,0,bits(131072,bytes),0x7fff,1,1ULL<<26);
test(op,0,bits(1.0/16777216,bytes),1,0,(1ULL<<24)|(1ULL<<19));
}
for(unsigned a=0;a<2;a++){test(4,a,0x3ff0000000000000ULL,0x3f800000,0,0);test(5,a,0x3f800000,0x3ff0000000000000ULL,0,0);}
printf("%u/%u half/scalar precision checks passed\n",checks-failures,checks);return failures!=0;
}
)C");}
