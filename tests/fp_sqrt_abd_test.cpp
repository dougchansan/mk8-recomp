// Emit actual translated C; compile it beside the header emitted by
// fp_sqrt_abd_vectors.py. --assembly supplies independent mnemonic roundtrips.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Form { unsigned word,kind,width,lanes,rd,rn,rm; };
int main(int argc,char** argv) {
    for(unsigned word : {0x2ee1f820u,0x2ee2d420u,0xaee1f820u,0xfee2d420u,
                         0x2ef9f820u,0x7ec21420u,0x2ec21420u}) {
        std::string out;bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss) { std::fprintf(stderr,"reserved sqrt/abd decoded %08x\n",word);return 1; }
    }
    std::vector<Form> forms;
    for(unsigned kind=0;kind<2;++kind)
    for(unsigned arrangement=0;arrangement<(kind?5u:3u);++arrangement)
    for(unsigned alias=0;alias<5;++alias) {
        bool scalar=arrangement>=3,dbl=arrangement==2||arrangement==4,q=arrangement!=0;
        unsigned rd=alias==4?31:alias==3?1:alias;
        unsigned rn=alias==4?31:1,rm=alias==4?31:alias==3?1:2;
        unsigned word=(kind?(scalar?0x7ea0d400u:0x2ea0d400u):0x2ea1f800u)|
                      (dbl?1u<<22:0)|(q?1u<<30:0)|(rn<<5)|rd;
        if(kind)word|=rm<<16;
        forms.push_back({word,kind,dbl?8u:4u,scalar?1u:(q?16u:8u)/(dbl?8u:4u),rd,rn,rm});
    }
    if(argc==2&&std::strcmp(argv[1],"--assembly")==0) {
        std::puts(".text");
        for(const auto& f:forms) {
            std::printf(".inst 0x%08x\n%s ",f.word,f.kind?"fabd":"fsqrt");
            if(f.lanes==1) std::printf("%c%u,%c%u,%c%u\n",f.width==8?'d':'s',f.rd,f.width==8?'d':'s',f.rn,f.width==8?'d':'s',f.rm);
            else {
                std::printf("v%u.%u%c,v%u.%u%c",f.rd,f.lanes,f.width==8?'d':'s',f.rn,f.lanes,f.width==8?'d':'s');
                if(f.kind)std::printf(",v%u.%u%c",f.rm,f.lanes,f.width==8?'d':'s');
                std::puts("");
            }
        }
        return 0;
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t vreg[32][2],fpcr,fpsr,sentinel[4]; } GuestContext;
typedef void (*Fn)(GuestContext*);
#include "fp_sqrt_abd_vectors.h"
)C");
    for(unsigned j=0;j<forms.size();++j) {
        std::string out;bool miss=false;
        suyu::recomp::Translate(forms[j].word,0x1000,out,&miss);
        if(miss) { std::fprintf(stderr,"sqrt/abd missing %08x\n",forms[j].word);return 1; }
        std::printf("static void f%u(GuestContext* c) {\n%s}\n",j,out.c_str());
    }
    // The old FADDP mask accidentally included FABD. Pin retained FADDP.
    unsigned add_index=0;
    for(unsigned word : {0x2e22d420u,0x6e22d420u,0x6e62d420u}) {
        std::string out;bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(miss)return 1;
        std::printf("static void add%u(GuestContext* c) {\n%s}\n",add_index++,out.c_str());
    }
    std::puts("static struct { Fn fn;unsigned kind,width,lanes,rd,rn,rm; } forms[]={");
    for(unsigned j=0;j<forms.size();++j) {
        const auto& f=forms[j];
        std::printf("{f%u,%u,%u,%u,%u,%u,%u},\n",j,f.kind,f.width,f.lanes,f.rd,f.rn,f.rm);
    }
    std::puts(R"C(};
static void set(GuestContext* c,unsigned reg,unsigned lane,unsigned width,uint64_t v) {
 memcpy((unsigned char*)c->vreg[reg]+lane*width,&v,width);
}
int main(void) {
 unsigned checks=0;
 {
  GuestContext c={0};float a[]={1,2,4,8},b[]={16,32,64,128},r[]={3,12,48,192};
  memcpy(c.vreg[1],a,16);memcpy(c.vreg[2],b,16);add1(&c);
  if(memcmp(c.vreg[0],r,16))return 1;
  r[1]=48;r[2]=0;r[3]=0;add0(&c);if(memcmp(c.vreg[0],r,16))return 1;
  double da[]={1,2},db[]={16,32},dr[]={3,48};
  memcpy(c.vreg[1],da,16);memcpy(c.vreg[2],db,16);add2(&c);
  if(memcmp(c.vreg[0],dr,16))return 1;
  checks+=3;
 }
 for(unsigned f=0;f<sizeof forms/sizeof forms[0];++f)
 for(unsigned v=0;v<sizeof vectors/sizeof vectors[0];++v) {
  if(forms[f].kind!=vectors[v].kind||forms[f].width!=vectors[v].width)continue;
  unsigned width=forms[f].width,rd=forms[f].rd,rn=forms[f].rn,rm=forms[f].rm,mode=vectors[v].mode;
  GuestContext c,expected;memset(&c,0xa5,sizeof c);
  c.fpcr=((uint64_t)(mode&3)<<22)|((mode&4)?1ULL<<24:0)|((mode&8)?1ULL<<25:0);
  c.fpsr=0x8000002;
  for(unsigned e=0;e<16/width;++e) {
   set(&c,rn,e,width,vectors[v].a);
   if(rn!=rm&&forms[f].kind)set(&c,rm,e,width,vectors[v].b);
  }
  expected=c;memset(expected.vreg[rd],0,16);
  uint64_t result=rn==rm?vectors[v].same:vectors[v].result;
  expected.fpsr|=rn==rm?vectors[v].sameflags:vectors[v].flags;
  for(unsigned e=0;e<forms[f].lanes;++e)set(&expected,rd,e,width,result);
  forms[f].fn(&c);++checks;
  if(memcmp(&c,&expected,sizeof c)) {
   printf("FAIL f=%u v=%u kind=%u width=%u mode=%u a=%llx b=%llx got=%llx expected=%llx flags=%llx/%llx\n",
    f,v,forms[f].kind,width,mode,(unsigned long long)vectors[v].a,(unsigned long long)vectors[v].b,
    (unsigned long long)c.vreg[rd][0],(unsigned long long)expected.vreg[rd][0],
    (unsigned long long)c.fpsr,(unsigned long long)expected.fpsr);return 1;
  }
 }
 printf("%u generated-C FSQRT/FABD checks passed\n",checks);return 0;
}
)C");
}
