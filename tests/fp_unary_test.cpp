#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){
 puts(R"C(#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
 unsigned base[]={0x0e218800,0x0ea18800,0x0e219800,0x0ea19800,0x2e218800,0x2e219800,0x2ea19800};
 for(unsigned op=0;op<7;op++)for(unsigned shape=0;shape<3;shape++)for(unsigned alias=0;alias<2;alias++){
  unsigned bits=base[op]|(shape?0x40000000:0)|(shape==2?0x400000:0)|32|alias;
  std::string s;bool bad=false;suyu::recomp::Translate(bits,0x1000,s,&bad);if(bad)return 1;
  printf("void f%u%u%u(GuestContext*c){%s}\n",op,shape,alias,s.c_str());
 }
 for(auto b:base){std::string s;bool bad=false;suyu::recomp::Translate(b|0x400000,0x1000,s,&bad);if(!bad)return 2;}
 puts("typedef void(*Fn)(GuestContext*);Fn f[7][3][2]={");
 for(unsigned o=0;o<7;o++){puts("{");for(unsigned s=0;s<3;s++)printf("{f%u%u0,f%u%u1},",o,s,o,s);puts("},");}puts("};");
 puts(R"C(
unsigned checks,failures;
uint64_t bits(double v,int d){uint64_t b=0;if(d)memcpy(&b,&v,8);else{float x=(float)v;memcpy(&b,&x,4);}return b;}
void test(unsigned op,unsigned shape,unsigned alias,unsigned mode,uint64_t in,uint64_t expected,uint64_t flags,uint64_t fpcr){
 GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=fpcr|((uint64_t)mode<<22);c.fpsr=0x8000000;
 unsigned bytes=shape==2?8:4,active=shape?16/bytes:2;for(unsigned j=0;j<16/bytes;j++)memcpy((char*)c.vreg[1]+j*bytes,&in,bytes);
 f[op][shape][alias](&c);unsigned char out[16]={0};for(unsigned j=0;j<active;j++)memcpy(out+j*bytes,&expected,bytes);
 checks++;if(memcmp(c.vreg[alias],out,16)||c.fpsr!=(flags|0x8000000)){failures++;printf("FAIL op%u s%u a%u m%u in%llx got%llx expect%llx fpsr%llx flags%llx\n",op,shape,alias,mode,(unsigned long long)in,(unsigned long long)c.vreg[alias][0],(unsigned long long)expected,(unsigned long long)c.fpsr,(unsigned long long)flags);}
}
int main(){
 const double input[]={0.,-0.,0.25,-0.25,0.5,-0.5,1.5,-1.5,2.5,-2.5,3.75,-3.75,8.,-8.};
 const double expected[5][14]={
 {0.,-0.,0.,-0.,0.,-0.,2.,-2.,2.,-2.,4.,-4.,8.,-8.},
 {0.,-0.,1.,-0.,1.,-0.,2.,-1.,3.,-2.,4.,-3.,8.,-8.},
 {0.,-0.,0.,-1.,0.,-1.,1.,-2.,2.,-3.,3.,-4.,8.,-8.},
 {0.,-0.,0.,-0.,0.,-0.,1.,-1.,2.,-2.,3.,-3.,8.,-8.},
 {0.,-0.,0.,-0.,1.,-1.,2.,-2.,3.,-3.,4.,-4.,8.,-8.}};
 for(unsigned op=0;op<7;op++)for(unsigned s=0;s<3;s++)for(unsigned a=0;a<2;a++)for(unsigned m=0;m<4;m++){
 unsigned r=op<5?op:m;int d=s==2;
 for(unsigned j=0;j<14;j++)test(op,s,a,m,bits(input[j],d),bits(expected[r][j],d),op==5&&j>=2&&j<12?16:0,0);
 uint64_t exp=d?0x7ff0000000000000ULL:0x7f800000,quiet=d?0x8000000000000ULL:0x400000,sign=d?0x8000000000000000ULL:0x80000000;
 test(op,s,a,m,exp,exp,0,0);test(op,s,a,m,exp|sign,exp|sign,0,0);
 test(op,s,a,m,exp|quiet|35,exp|quiet|35,0,0);
 test(op,s,a,m,exp|35,exp|quiet|35,1,0);
 test(op,s,a,m,exp|sign|35,exp|quiet,1,1ULL<<25);
 test(op,s,a,m,1,0,128,1ULL<<24);
 test(op,s,a,m,sign|1,sign,128,1ULL<<24);
 test(op,s,a,m,1,r==1?bits(1,d):0,op==5?16:0,0);
 test(op,s,a,m,sign|1,r==2?bits(-1,d):sign,op==5?16:0,0);
 }
 printf("%u/%u FRINT checks passed\n",checks-failures,checks);return failures!=0;
}
)C");
}
