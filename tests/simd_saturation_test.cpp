// Synthetic generated-C execution checks; oracle uses signed magnitudes.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main() {
    for (unsigned word : {0x0ee22c20u, 0x0ee07820u, 0x4ee14820u,
                          0x4e22b420u, 0x4ee2b420u, 0x4f02c820u,
                          0x4fc2c820u, 0x6f42c820u, 0x0ee24c20u, 0x0ee25c20u}) {
        std::string out; bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss){std::fprintf(stderr,"reserved saturation accepted %08x\n",word);return 1;}
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t vreg[32][2]; uint64_t fpsr; } GuestContext;
typedef void (*Fn)(GuestContext*);
)C");
    unsigned count=0;
    // 0/1 signed add/sub, 2/3 unsigned add/sub, 4/5 abs/neg,
    // 6/7/8 signed/unsigned/signed-to-unsigned narrow, 9/10 mulhigh/round.
    for(unsigned op=0;op<17;op++) for(unsigned size=0;size<4;size++)
    for(unsigned shape=0;shape<3;shape++) for(unsigned alias=0;alias<3;alias++)
    for(unsigned elem=0;elem<(op>=11&&op<13?(size==1?8:4):1);elem++) {
        if((op>=6&&op<=8&&size==3)||(op>=9&&op<13&&(size==0||size==3))||
           ((op<6||op>=13)&&shape==0&&size==3)) continue;
        unsigned word=op<4 ? 0x0e200c00u|((op&1)<<13)|((op>>1)<<29)|(2<<16) :
            op<6 ? 0x0e207800u|((op-4)<<29) :
            op<9 ? (op==8?0x2e212800u:0x0e214800u|((op-6)<<29)) :
            op<11 ? 0x0e20b400u|((op-9)<<29)|(2<<16) :
            op>=13 ? 0x0e204c00u|((op>=15?1:0)<<29)|((op==14||op==16?1:0)<<12)|(2<<16) :
            0x0f00c000u|((op-11)<<12)|((size==2?18:2)<<16)|
                ((elem>>(size==1?2:1))<<11)|((elem& (size==1?2:1))<<(size==1?20:21))|
                (size==1?(elem&1)<<20:0);
        word|=(size<<22)|(shape==0?0:shape==1?0x40000000u:0x50000000u)|(1<<5)|alias;
        std::string out; bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(miss) { std::fprintf(stderr,"missing saturation %08x\n",word); return 1; }
        std::printf("static void f%u(GuestContext*c){%s}\n",count++,out.c_str());
    }
    std::puts("static Fn fn[]={"); for(unsigned j=0;j<count;j++)std::printf("f%u,\n",j);
    std::puts(R"C(};
static uint64_t seed=123456789;
static uint64_t rnd(void){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;}
static uint64_t mask(unsigned n){return UINT64_MAX>>(64-n);}
static uint64_t readlane(const void*p,unsigned n){uint64_t x=0;memcpy(&x,p,n/8);return x;}
static uint64_t oracle(unsigned op,uint64_t a,uint64_t b,unsigned n,int*qc){
 uint64_t m=mask(n),sign=UINT64_C(1)<<(n-1),lim=m; int neg=0;
 if(op>=13){int sh=(int)(b&255);if(sh>=128)sh-=256;
  neg=op<15&&(a&sign)!=0;uint64_t v=neg?((~a+1)&m):a;
  lim=op>=15?m:neg?sign:sign-1;
  if(sh>=0){while(sh--){if(v>lim/2){*qc=1;v=lim;break;}v*=2;}}
  else{int last=0;while(sh++){last=(int)(v&1);v=(v>>1)+(neg?last:0);}if((op==14||op==16)&&last){if(neg)v--;else v++;}}
  return neg?((~v+1)&m):v;
 }
 if(op==2){if(b>m-a){*qc=1;return m;}return a+b;}
 if(op==3){if(a<b){*qc=1;return 0;}return a-b;}
 if(op==7){lim=mask(n/2);if(a>lim){*qc=1;return lim;}return a;}
 int na=(a&sign)!=0,nb=(b&sign)!=0;
 uint64_t ma=na?((~a+1)&m):a,mb=nb?((~b+1)&m):b,v;
 if(op==0||op==1){nb^=op==1;
   if(na==nb){neg=na;lim=neg?sign:sign-1;if(ma>lim||mb>lim-ma){*qc=1;return neg?sign:sign-1;}v=ma+mb;}
   else if(ma>=mb){v=ma-mb;neg=na;}else{v=mb-ma;neg=nb;}
 }else if(op==4||op==5){v=ma;neg=op==5?!na:0;}
 else if(op==6||op==8){v=ma;neg=na;n/=2;sign=UINT64_C(1)<<(n-1);m=mask(n);}
 else {int64_t x=na?-(int64_t)ma:(int64_t)ma,y=nb?-(int64_t)mb:(int64_t)mb;
   int64_t p=x*y,d=INT64_C(1)<<(n-1);if(op==10)p+=d/2;
   int64_t q=p/d;if(p<0&&p%d)q--;neg=q<0;v=neg?(uint64_t)-q:(uint64_t)q;}
 if(op==8){if(neg&&v){*qc=1;return 0;}lim=m;}else lim=neg?sign:sign-1;
 if(v>lim){*qc=1;v=lim;}return neg?((~v+1)&m):v;
}
int main(void){unsigned k=0,checks=0;
for(unsigned op=0;op<17;op++)for(unsigned size=0;size<4;size++)
for(unsigned shape=0;shape<3;shape++)for(unsigned alias=0;alias<3;alias++)
for(unsigned elem=0;elem<(op>=11&&op<13?(size==1?8:4):1);elem++){
 if((op>=6&&op<=8&&size==3)||(op>=9&&op<13&&(size==0||size==3))||((op<6||op>=13)&&shape==0&&size==3))continue;
 unsigned narrow=op>=6&&op<=8,n=8u<<size,sn=narrow?n*2:n;
 unsigned bytes=shape==2?n/8:shape==1?16:8,lanes=narrow?(shape==2?1:8/(n/8)):bytes/(n/8);
 for(unsigned sample=0;sample<(op>=13?2048u:400u);sample++){
 GuestContext c,e;for(unsigned r=0;r<32;r++)for(unsigned h=0;h<2;h++)c.vreg[r][h]=rnd();
 c.fpsr=sample&1?0x08000081:0x81;
 for(unsigned l=0;l<lanes;l++){uint64_t edges[]={0,1,mask(sn),UINT64_C(1)<<(sn-1),(UINT64_C(1)<<(sn-1))-1,(UINT64_C(1)<<(sn-1))+1};
 uint64_t a=sample<36?edges[sample/6]:rnd(),b=sample<36?edges[sample%6]:rnd();
 if(op>=13){b=(b&~UINT64_C(255))|(sample&255);if(sample<1536)a=edges[sample/256];}
 memcpy((unsigned char*)c.vreg[1]+l*sn/8,&a,sn/8);memcpy((unsigned char*)c.vreg[op>=11&&op<13&&size==2?18:2]+l*n/8,&b,n/8);}
 e=c;if(!(narrow&&shape==1))memset(e.vreg[alias],0,16);
 int qc=0;for(unsigned l=0;l<lanes;l++){uint64_t a=readlane((unsigned char*)c.vreg[1]+l*sn/8,sn),b=readlane((unsigned char*)c.vreg[op>=11&&op<13&&size==2?18:2]+(op>=11&&op<13?elem:l)*n/8,n);
 uint64_t v=oracle(op>=11&&op<13?op-2:op,a,b,sn,&qc);memcpy((unsigned char*)e.vreg[alias]+(narrow&&shape==1?8:0)+l*n/8,&v,n/8);}
 if(qc){e.fpsr|=UINT64_C(1)<<27;}fn[k](&c);checks++;
 if(memcmp(&c,&e,sizeof(c))){printf("FAIL op=%u size=%u shape=%u alias=%u sample=%u\n",op,size,shape,alias,sample);return 1;}
 }k++;}printf("%u saturation checks passed\n",checks);return 0;}
)C");
}
