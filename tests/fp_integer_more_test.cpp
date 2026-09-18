#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(){puts(R"C(#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr;} GuestContext;
)C");
for(unsigned o=0;o<4;o++)for(unsigned u=0;u<2;u++)for(unsigned s=0;s<3;s++)for(unsigned a=0;a<2;a++)for(unsigned k=0;k<3;k++){
unsigned w=s==2?64:32,fb=k==0?1:k==1?w/2:w;
unsigned b=o==0?0x0e21a800:o==1?0x0e21c800:o==2?0x0f00fc00:0x0f00e400;
b|=(u<<29)|(s?1U<<30:0)|(o>=2?((2*w-fb)<<16):(s==2?1U<<22:0))|32|a;
std::string out;bool bad=false;suyu::recomp::Translate(b,0x1000,out,&bad);if(bad)return 1;printf("void f%u%u%u%u%u(GuestContext*c){%s}\n",o,u,s,a,k,out.c_str());}
puts("typedef void(*Fn)(GuestContext*);Fn f[4][2][3][2][3]={");for(unsigned o=0;o<4;o++){puts("{");for(unsigned u=0;u<2;u++){puts("{");for(unsigned s=0;s<3;s++){puts("{");for(unsigned a=0;a<2;a++)printf("{f%u%u%u%u0,f%u%u%u%u1,f%u%u%u%u2},",o,u,s,a,o,u,s,a,o,u,s,a);puts("},");}puts("},");}puts("},");}puts("};");
puts(R"C(
unsigned checks,failures;
void check(unsigned o,unsigned u,unsigned s,unsigned a,unsigned k,uint64_t input,unsigned mode,uint64_t expected,unsigned flags){
#ifdef FP_INTEGER_VECTOR_EXPORT
printf("%u %u %u %u %u %016" PRIx64 " %u %016" PRIx64 " %u\n",o,u,s,a,k,input,mode,expected,flags);
#endif
unsigned bytes=s==2?8:4;
GuestContext c;memset(&c,0xa5,sizeof c);c.fpcr=(uint64_t)mode<<22;c.fpsr=0;
for(unsigned j=0;j<16/bytes;j++)memcpy((char*)c.vreg[1]+j*bytes,&input,bytes);
unsigned char out[16]={0};for(unsigned j=0;j<(s?16/bytes:2);j++)memcpy(out+j*bytes,&expected,bytes);
f[o][u][s][a][k](&c);checks++;if(memcmp(out,c.vreg[a],16)||c.fpsr!=flags){failures++;if(failures<12)fprintf(stderr,"FAIL o%u u%u s%u k%u mode%u in%llx got%llx want%llx flags%llu want%u\n",o,u,s,k,mode,(unsigned long long)input,(unsigned long long)c.vreg[a][0],(unsigned long long)expected,(unsigned long long)c.fpsr,flags);}
}
#ifdef FP_INTEGER_REPLAY
int main(int argc,char**argv){
if(argc!=2){fprintf(stderr,"usage: integer-replay vectors.txt\n");return 2;}
FILE*fp=fopen(argv[1],"r");if(!fp){perror(argv[1]);return 2;}
unsigned o,u,s,a,k,mode,flags;uint64_t input,expected;int fields;
while((fields=fscanf(fp,"%u %u %u %u %u %" SCNx64 " %u %" SCNx64 " %u",&o,&u,&s,&a,&k,&input,&mode,&expected,&flags))==9){
if(o>=4||u>=2||s>=3||a>=2||k>=3||mode>=4||flags>255){fprintf(stderr,"invalid vector index\n");fclose(fp);return 2;}
check(o,u,s,a,k,input,mode,expected,flags);
}
int malformed=fields!=EOF||ferror(fp)||checks!=378594;fclose(fp);
fprintf(stderr,"%u/%u integer conversion replay checks passed\n",checks-failures,checks);
if(malformed)fprintf(stderr,"malformed or incomplete vector file (expected 378594 cases)\n");
return failures||malformed;
}
#else
void test(unsigned o,unsigned u,unsigned s,unsigned a,unsigned k,uint64_t input,unsigned mode){
unsigned width=s==2?64:32,fb=k==0?1:k==1?width/2:width;
uint64_t expected=0;unsigned flags=0;
if(o==3){long double x;if(u)x=width==64?(long double)input:(long double)(uint32_t)input;else if(width==64){int64_t v;memcpy(&v,&input,8);x=v;}else{int32_t v;memcpy(&v,&input,4);x=v;}
x=ldexpl(x,-(int)fb);long double back;if(width==64){volatile double d=(double)x;memcpy(&expected,(const void*)&d,8);back=d;}else{volatile float v=(float)x;memcpy(&expected,(const void*)&v,4);back=v;}if(back!=x)flags=16;
}else{long double x;if(width==64){double d;memcpy(&d,&input,8);x=d;}else{float v;memcpy(&v,&input,4);x=v;}
if(o==2){x=ldexpl(x,fb);}long double r;if(isnan(x)){r=0;flags=1;}else{r=o==2?truncl(x):o==1?roundl(x):nearbyintl(x);long double low=u?0:-ldexpl(1,width-1),high=u?ldexpl(1,width)-1:ldexpl(1,width-1)-1;
if(r<low){r=low;flags=1;}else if(r>high){r=high;flags=1;}else if(r!=x)flags=16;}
if(u)expected=(uint64_t)r;else expected=(uint64_t)(int64_t)r;}
check(o,u,s,a,k,input,mode,expected,flags);
}
int main(){
if(LDBL_MANT_DIG<64||LDBL_MAX_EXP<2048){fprintf(stderr,"Oracle requires extended-precision long double; compile with FP_INTEGER_REPLAY and use exported Linux vectors.\n");return 2;}
int modes[]={FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO};uint64_t seed=0x14eefeab;
for(unsigned o=0;o<4;o++)for(unsigned u=0;u<2;u++)for(unsigned s=0;s<3;s++)for(unsigned k=0;k<3;k++)for(unsigned mode=0;mode<(o==3?4:1);mode++){
fesetround(modes[mode]);for(unsigned j=0;j<3000;j++){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;test(o,u,s,j&1,k,seed,mode);}
if(o<3){double values[]={0,-0.,0.5,-0.5,1.5,-1.5,2.5,-2.5,INFINITY,-INFINITY,NAN};for(unsigned j=0;j<11;j++){uint64_t v=0;if(s==2)memcpy(&v,&values[j],8);else{float x=(float)values[j];memcpy(&v,&x,4);}test(o,u,s,0,k,v,mode);}}
}fesetround(FE_TONEAREST);fprintf(stderr,"%u/%u integer conversion checks passed\n",checks-failures,checks);return failures!=0;}
#endif
)C");}
