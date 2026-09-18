// Synthetic indexed widening arithmetic plus indexed MUL promotion regression.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Op { unsigned base,wide,uns,acc; const char* name; };
static const Op ops[]={
    {0x0f00a000,1,0,0,"smull"},{0x2f00a000,1,1,0,"umull"},
    {0x0f002000,1,0,1,"smlal"},{0x2f002000,1,1,1,"umlal"},
    {0x0f006000,1,0,2,"smlsl"},{0x2f006000,1,1,2,"umlsl"},
    {0x0f008000,0,1,0,"mul"},{0x2f000000,0,1,1,"mla"},{0x2f004000,0,1,2,"mls"},
};
struct Case {unsigned wide,uns,acc,q,size,lane,rd,rn,rm;};
int main(int argc,char**) {
    const bool assembly=argc>1;std::vector<Case> cases;unsigned negatives=0;
    if(assembly)std::puts(".text");
    else std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\ntypedef struct {uint64_t vreg[32][2];uint64_t sentinel[4];} GuestContext;\ntypedef void (*Fn)(GuestContext*);");
    for(const auto& op:ops){
        const unsigned word=op.base|0xe0420020u;
        std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss){std::fprintf(stderr,"high-bit reserved accepted %08x\n",word);return 1;}++negatives;
    }
    for(const auto& op:ops)for(unsigned q=0;q<2;++q) {
        for(unsigned size:{0u,3u}) {
            const unsigned word=op.base|(q<<30)|(size<<22)|(2<<16)|32;
            std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
            if(!miss){std::fprintf(stderr,"reserved accepted %08x\n",word);return 1;}++negatives;
        }
        for(unsigned size=1;size<=2;++size)for(unsigned lane=0;lane<(size==1?8u:4u);++lane)
        for(unsigned alias=0;alias<5;++alias) {
            const unsigned edge=size==1?15:31;
            const unsigned rd=alias==4?edge:alias==3?1:alias,rn=alias==4?edge:1,rm=alias==4?edge:alias==3?1:2;
            const unsigned lane_bits=size==1?((lane>>2)<<11)|(((lane>>1)&1)<<21)|((lane&1)<<20):
                ((lane>>1)<<11)|((lane&1)<<21)|((rm>>4)<<20);
            const unsigned word=op.base|(q<<30)|(size<<22)|((rm&15)<<16)|lane_bits|(rn<<5)|rd;
            const unsigned n=1u<<size,active=q?16:8;const char elt[]={'b','h','s','d'};
            if(assembly) {
                std::printf("%s%s v%u.%u%c,v%u.%u%c,v%u.%c[%u]\n",op.name,op.wide&&q?"2":"",rd,op.wide?8/n:active/n,elt[size+op.wide],rn,active/n,elt[size],rm,elt[size],lane);
                std::fprintf(stderr,"%08x\n",word);
            } else {
                std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
                if(miss){std::fprintf(stderr,"missing %s %08x\n",op.name,word);return 1;}
                std::printf("static void f%zu(GuestContext* c){%s}\n",cases.size(),out.c_str());
            }
            cases.push_back({op.wide,op.uns,op.acc,q,size,lane,rd,rn,rm});
        }
    }
    if(assembly)return 0;
    std::fprintf(stderr,"%u reserved encoding checks passed\n",negatives);
    std::puts("static Fn functions[]={");for(unsigned j=0;j<cases.size();++j)std::printf("f%u,\n",j);
    std::puts("};\nstatic const unsigned cases[][9]={");
    for(const auto& c:cases)std::printf("{%u,%u,%u,%u,%u,%u,%u,%u,%u},\n",c.wide,c.uns,c.acc,c.q,c.size,c.lane,c.rd,c.rn,c.rm);
    std::puts(R"C(};
static uint64_t rng=UINT64_C(0xf568719439421785);
static uint64_t random_word(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
static uint64_t lane(const void* p,unsigned off,unsigned n){
    const unsigned char* b=(const unsigned char*)p+off;uint64_t r=0;
    for(unsigned j=0;j<n;++j)r|=(uint64_t)b[j]<<(8*j);
    return r;
}
static void putlane(void* p,unsigned off,unsigned n,uint64_t v){
    unsigned char* b=(unsigned char*)p+off;for(unsigned j=0;j<n;++j)b[j]=(unsigned char)(v>>(8*j));
}
static uint64_t extend(uint64_t v,unsigned bits,unsigned uns){return !uns&&(v&(UINT64_C(1)<<(bits-1)))?v|(UINT64_MAX<<bits):v;}
// Independent bit-serial modular product; emitted code uses native multiply.
static uint64_t product(uint64_t a,uint64_t b){
    uint64_t r=0;for(unsigned j=0;j<64;++j){if(b&1)r+=a;a<<=1;b>>=1;}return r;
}
int main(void){
    unsigned checks=0;
    for(unsigned idx=0;idx<sizeof(cases)/sizeof(cases[0]);++idx){
        const unsigned* t=cases[idx];const unsigned w=t[0],u=t[1],acc=t[2],q=t[3],n=1u<<t[4],ix=t[5],rd=t[6],rn=t[7],rm=t[8];
        for(unsigned sample=0;sample<263;++sample){
            GuestContext got,want;
            for(unsigned r=0;r<32;++r)for(unsigned h=0;h<2;++h)
                got.vreg[r][h]=sample==0?UINT64_MAX:sample==1?0:sample==2?UINT64_C(0x8000800080008000):
                    sample==3?UINT64_C(0x7fffffff80000000):sample==4?UINT64_C(0x7fffffffffffffff):
                    sample==5?UINT64_C(0x8000000000000000):random_word();
            for(unsigned j=0;j<4;++j)got.sentinel[j]=random_word();
            want=got;memset(want.vreg[rd],0,16);
            const unsigned bytes=w?16:q?16:8,dn=w?2*n:n;
            const uint64_t m=extend(lane(got.vreg[rm],ix*n,n),n*8,u);
            for(unsigned off=0;off<bytes;off+=dn){
                const uint64_t a=extend(lane(got.vreg[rn],w?q*8+off/2:off,n),n*8,u);
                const uint64_t p=product(a,m),d=lane(got.vreg[rd],off,dn);
                putlane(want.vreg[rd],off,dn,acc==1?d+p:acc==2?d-p:p);
            }
            functions[idx](&got);++checks;
            if(memcmp(&got,&want,sizeof got)){printf("FAIL idx=%u sample=%u\n",idx,sample);return 1;}
        }
    }
    printf("%u generated-C indexed multiply checks passed\n",checks);return 0;
}
)C");
}
