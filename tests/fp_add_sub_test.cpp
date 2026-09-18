// Compile generator, emit C beside fp_add_sub_vectors.py's generated header.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Form { unsigned word,sub,width,lanes,kind,rd,rn,rm; };
int main(int argc,char** argv) {
    for(unsigned word:{0x0e62d420u,0x0ee2d420u,0x2e62d420u,0x1ea22820u,
                       0x1ee22820u,0x0e421420u,0x5e30d820u}) {
        bool miss=false;std::string out;suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss){std::fprintf(stderr,"reserved/FP16 add decoded %08x\n",word);return 1;}
    }
    std::vector<Form> forms;
    for(unsigned kind=0;kind<4;++kind)for(unsigned arr=0;arr<(kind%2?3u:2u);++arr)
    for(unsigned sub=0;sub<(kind<2?2u:1u);++sub)for(unsigned alias=0;alias<5;++alias) {
        bool dbl=kind%2?arr==2:arr==1;unsigned lanes=kind%2?(arr==1?4:2):1;
        unsigned rd=alias==4?31:alias==3?1:alias,rn=alias==4?31:1,rm=alias==4?31:alias==3?1:2;
        unsigned word=kind==0?0x1e202800:kind==1?0x0e20d400:kind==2?0x7e30d800:0x2e20d400;
        word|=(dbl?1u<<22:0)|(rn<<5)|rd;
        if(kind!=2)word|=rm<<16;
        if(kind%2&&arr!=0)word|=1u<<30;
        if(sub)word|=kind==0?0x1000:0x800000;
        forms.push_back({word,sub,dbl?8u:4u,lanes,kind,rd,rn,rm});
    }
    if(argc==2&&std::strcmp(argv[1],"--assembly")==0) {
        std::puts(".text");for(const auto& f:forms){char t=f.width==8?'d':'s';
            std::printf(".inst 0x%08x\n%s ",f.word,f.kind>=2?"faddp":f.sub?"fsub":"fadd");
            if(f.kind==0)std::printf("%c%u,%c%u,%c%u\n",t,f.rd,t,f.rn,t,f.rm);
            else if(f.kind==2)std::printf("%c%u,v%u.2%c\n",t,f.rd,f.rn,t);
            else std::printf("v%u.%u%c,v%u.%u%c,v%u.%u%c\n",f.rd,f.lanes,t,f.rn,f.lanes,t,f.rm,f.lanes,t);
        }return 0;
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint64_t vreg[32][2],fpcr,fpsr,sentinel[4];} GuestContext;
typedef void (*Fn)(GuestContext*);
#include "fp_add_sub_vectors.h"
)C");
    for(unsigned j=0;j<forms.size();++j){bool miss=false;std::string out;suyu::recomp::Translate(forms[j].word,0x1000,out,&miss);
        if(miss)return 1;std::printf("static void f%u(GuestContext*c){\n%s}\n",j,out.c_str());}
    std::puts("static struct {Fn fn;unsigned sub,width,lanes,kind,rd,rn,rm;} forms[]={");
    for(unsigned j=0;j<forms.size();++j){auto f=forms[j];std::printf("{f%u,%u,%u,%u,%u,%u,%u,%u},\n",j,f.sub,f.width,f.lanes,f.kind,f.rd,f.rn,f.rm);}
    std::puts(R"C(};
static void set(GuestContext*c,unsigned r,unsigned e,unsigned w,uint64_t v){memcpy((char*)c->vreg[r]+e*w,&v,w);}
int main(void){unsigned checks=0;
for(unsigned f=0;f<sizeof forms/sizeof forms[0];++f)for(unsigned v=0;v<sizeof vectors/sizeof vectors[0];++v){
 if(forms[f].width!=vectors[v].width||forms[f].sub!=vectors[v].sub)continue;
 unsigned w=forms[f].width,rd=forms[f].rd,rn=forms[f].rn,rm=forms[f].rm,k=forms[f].kind,mode=vectors[v].mode;
 GuestContext c,expected;memset(&c,0xa5,sizeof c);c.fpcr=((uint64_t)(mode&3)<<22)|((mode&4)?1ULL<<24:0)|((mode&8)?1ULL<<25:0);c.fpsr=0x8000002;
 for(unsigned e=0;e<16/w;++e){set(&c,rn,e,w,k>=2&&e%2?vectors[v].b:vectors[v].a);if(rn!=rm&&k!=2)set(&c,rm,e,w,k>=2&&!(e%2)?vectors[v].a:vectors[v].b);}
 expected=c;memset(expected.vreg[rd],0,16);
 uint64_t r=k<2&&rn==rm?vectors[v].same:vectors[v].result;
 expected.fpsr|=k<2&&rn==rm?vectors[v].sameflags:vectors[v].flags;
 for(unsigned e=0;e<forms[f].lanes;++e)set(&expected,rd,e,w,r);
 forms[f].fn(&c);++checks;if(memcmp(&c,&expected,sizeof c)){printf("FAIL f=%u kind=%u v=%u width=%u mode=%u a=%llx b=%llx got=%llx want=%llx fpsr=%llx/%llx\n",f,k,v,w,mode,(unsigned long long)vectors[v].a,(unsigned long long)vectors[v].b,(unsigned long long)c.vreg[rd][0],(unsigned long long)expected.vreg[rd][0],(unsigned long long)c.fpsr,(unsigned long long)expected.fpsr);return 1;}
}printf("%u generated-C FADD/FSUB/FADDP checks passed\n",checks);return 0;}
)C");
}
