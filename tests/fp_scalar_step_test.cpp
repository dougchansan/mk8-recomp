// Actual Translate execution against fp_scalar_step_vectors.py exact rationals.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Form { unsigned word,kind,width,rd,rn,rm; };
int main(int argc,char** argv) {
    for(unsigned word:{0xde22fc20u,0x5ea2e420u,0x5e423c20u,0x5e422420u}) {
        bool miss=false;std::string out;suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss){std::fprintf(stderr,"unsupported step/compare decoded %08x\n",word);return 1;}
    }
    const char* names[]={"frecps","frsqrts","fcmeq","fcmge","fcmgt"};
    const unsigned bases[]={0x5e20fc00,0x5ea0fc00,0x5e20e400,0x7e20e400,0x7ea0e400};
    std::vector<Form> forms;
    for(unsigned kind=0;kind<5;++kind)for(unsigned dbl=0;dbl<2;++dbl)for(unsigned alias=0;alias<5;++alias) {
        unsigned rd=alias==4?31:alias==3?1:alias,rn=alias==4?31:1,rm=alias==4?31:alias==3?1:2;
        forms.push_back({bases[kind]|(dbl<<22)|(rm<<16)|(rn<<5)|rd,kind,dbl?8u:4u,rd,rn,rm});
    }
    if(argc==2&&std::strcmp(argv[1],"--assembly")==0) {
        std::puts(".text");for(const auto& f:forms) {
            char type=f.width==8?'d':'s';std::printf(".inst 0x%08x\n%s %c%u,%c%u,%c%u\n",f.word,names[f.kind],type,f.rd,type,f.rn,type,f.rm);
        }return 0;
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t vreg[32][2],fpcr,fpsr,sentinel[4]; } GuestContext;
typedef void (*Fn)(GuestContext*);
#include "fp_scalar_step_vectors.h"
)C");
    for(unsigned j=0;j<forms.size();++j) {
        std::string out;bool miss=false;suyu::recomp::Translate(forms[j].word,0x1000,out,&miss);
        if(miss){std::fprintf(stderr,"step/compare missing %08x\n",forms[j].word);return 1;}
        std::printf("static void f%u(GuestContext* c) {\n%s}\n",j,out.c_str());
    }
    std::puts("static struct { Fn fn;unsigned kind,width,rd,rn,rm; } forms[]={");
    for(unsigned j=0;j<forms.size();++j){const auto& f=forms[j];std::printf("{f%u,%u,%u,%u,%u,%u},\n",j,f.kind,f.width,f.rd,f.rn,f.rm);}
    std::puts(R"C(};
int main(void) {
 unsigned checks=0;
 for(unsigned f=0;f<sizeof forms/sizeof forms[0];++f)for(unsigned v=0;v<sizeof vectors/sizeof vectors[0];++v) {
  if(forms[f].kind!=vectors[v].kind||forms[f].width!=vectors[v].width)continue;
  unsigned width=forms[f].width,rd=forms[f].rd,rn=forms[f].rn,rm=forms[f].rm,mode=vectors[v].mode;
  GuestContext c,expected;memset(&c,0xa5,sizeof c);
  c.fpcr=((uint64_t)(mode&3)<<22)|((mode&4)?1ULL<<24:0)|((mode&8)?1ULL<<25:0);c.fpsr=0x8000002;
  memcpy(c.vreg[rn],&vectors[v].a,width);if(rn!=rm)memcpy(c.vreg[rm],&vectors[v].b,width);
  expected=c;memset(expected.vreg[rd],0,16);
  uint64_t result=rn==rm?vectors[v].same:vectors[v].result;
  expected.fpsr|=rn==rm?vectors[v].sameflags:vectors[v].flags;
  memcpy(expected.vreg[rd],&result,width);forms[f].fn(&c);++checks;
  if(memcmp(&c,&expected,sizeof c)) {
   printf("FAIL f=%u v=%u kind=%u width=%u mode=%u a=%llx b=%llx got=%llx expected=%llx flags=%llx/%llx\n",f,v,forms[f].kind,width,mode,
    (unsigned long long)vectors[v].a,(unsigned long long)vectors[v].b,(unsigned long long)c.vreg[rd][0],
    (unsigned long long)expected.vreg[rd][0],(unsigned long long)c.fpsr,(unsigned long long)expected.fpsr);return 1;
  }
 }
 printf("%u generated-C scalar reciprocal-step/compare checks passed\n",checks);return 0;
}
)C");
}
