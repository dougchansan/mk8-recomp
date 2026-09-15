// Synthetic generated-C semantic checks; no game-derived inputs.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"

struct Op { unsigned base, kind, uns, flag; const char* name; };
static const Op ops[] = {
    {0x0e20bc00,0,1,0,"addp"},
    {0x0e20a400,1,0,0,"smaxp"},{0x0e20ac00,1,0,1,"sminp"},
    {0x2e20a400,1,1,0,"umaxp"},{0x2e20ac00,1,1,1,"uminp"},
    {0x0e30a800,2,0,0,"smaxv"},{0x0e31a800,2,0,1,"sminv"},
    {0x2e30a800,2,1,0,"umaxv"},{0x2e31a800,2,1,1,"uminv"},
    {0x0e200000,3,0,0,"saddl"},{0x0e202000,3,0,1,"ssubl"},
    {0x2e200000,3,1,0,"uaddl"},{0x2e202000,3,1,1,"usubl"},
    {0x0e201000,4,0,0,"saddw"},{0x0e203000,4,0,1,"ssubw"},
    {0x2e201000,4,1,0,"uaddw"},{0x2e203000,4,1,1,"usubw"},
    {0x0e202800,5,0,0,"saddlp"},{0x2e202800,5,1,0,"uaddlp"},
    {0x0e206800,5,0,1,"sadalp"},{0x2e206800,5,1,1,"uadalp"},
    {0x5ef1b800,6,1,0,"addp"},
    {0x0e207000,7,0,0,"sabdl"},{0x2e207000,7,1,0,"uabdl"},
    {0x0e205000,7,0,1,"sabal"},{0x2e205000,7,1,1,"uabal"},
};
struct Case { unsigned kind, uns, flag, q, size, rd, rn, rm; };
int main(int argc, char**) {
    const bool assembly = argc > 1;
    std::vector<Case> cases;
    unsigned negatives = 0;
    for (const auto& op : ops) {
        const unsigned word = op.base | 0xe0000020u |
            (op.kind <= 1 || op.kind == 3 || op.kind == 4 || op.kind == 7 ? 2u << 16 : 0);
        std::string out; bool miss = false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if (!miss) { std::fprintf(stderr,"high-bit reserved accepted %08x\n",word); return 1; }
        ++negatives;
    }
    for (unsigned word : {0x5e31b820u,0x5e71b820u,0x5eb1b820u,0x7ef1b820u}) {
        std::string out; bool miss = false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if (!miss) { std::fprintf(stderr,"scalar reserved accepted %08x\n",word); return 1; }
        ++negatives;
    }
    if (assembly) std::puts(".text");
    else std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\ntypedef struct { uint64_t vreg[32][2]; uint64_t sentinel[4]; } GuestContext;\ntypedef void (*Fn)(GuestContext*);");
    for (const auto& op : ops) for (unsigned q = 0; q < 2; ++q)
    for (unsigned size = 0; size < 4; ++size) {
        const bool valid = op.kind == 6 ? q == 1 && size == 3 :
            (size < 3 || (op.kind == 0 && q)) && !(op.kind == 2 && size == 2 && !q);
        if (!valid) {
            if (op.kind == 6) continue;
            const unsigned word = op.base | (q << 30) | (size << 22) | (1 << 5) |
                                  (op.kind <= 1 || op.kind == 3 || op.kind == 4 || op.kind == 7 ? 2 << 16 : 0);
            std::string out; bool miss = false;
            suyu::recomp::Translate(word,0x1000,out,&miss);
            if (!miss) { std::fprintf(stderr,"reserved accepted %08x\n",word); return 1; }
            ++negatives;
            continue;
        }
        for (unsigned alias = 0; alias < 5; ++alias) {
            const unsigned rd = alias == 4 ? 31 : alias == 3 ? 1 : alias;
            const unsigned rn = alias == 4 ? 31 : 1, rm = alias == 4 ? 31 : alias == 3 ? 1 : 2;
            const unsigned word = op.base | (op.kind == 6 ? 0 : (q << 30) | (size << 22)) |
                (rn << 5) | rd | (op.kind <= 1 || op.kind == 3 || op.kind == 4 || op.kind == 7 ? rm << 16 : 0);
            const unsigned bytes = 1u << size;
            const char elt[] = {'b','h','s','d'};
            if (assembly) {
                if (op.kind <= 1) std::printf("%s v%u.%u%c,v%u.%u%c,v%u.%u%c\n",op.name,rd,(q?16:8)/bytes,elt[size],rn,(q?16:8)/bytes,elt[size],rm,(q?16:8)/bytes,elt[size]);
                else if (op.kind == 2) std::printf("%s %c%u,v%u.%u%c\n",op.name,elt[size],rd,rn,(q?16:8)/bytes,elt[size]);
                else if (op.kind <= 4 || op.kind == 7) std::printf("%s%s v%u.%u%c,v%u.%u%c,v%u.%u%c\n",op.name,q?"2":"",rd,8/bytes,elt[size+1],rn,op.kind==4?8/bytes:(q?16:8)/bytes,elt[size+(op.kind==4)],rm,(q?16:8)/bytes,elt[size]);
                else if (op.kind == 5) std::printf("%s v%u.%u%c,v%u.%u%c\n",op.name,rd,(q?8:4)/bytes,elt[size+1],rn,(q?16:8)/bytes,elt[size]);
                else std::printf("addp d%u,v%u.2d\n",rd,rn);
                std::fprintf(stderr,"%08x\n",word);
            } else {
                std::string out; bool miss = false;
                suyu::recomp::Translate(word,0x1000,out,&miss);
                if (miss) { std::fprintf(stderr,"missing %s %08x\n",op.name,word); return 1; }
                std::printf("static void f%zu(GuestContext* c) { %s }\n",cases.size(),out.c_str());
            }
            cases.push_back({op.kind,op.uns,op.flag,q,size,rd,rn,rm});
        }
    }
    if (assembly) return 0;
    std::fprintf(stderr,"%u reserved encoding checks passed\n",negatives);
    std::puts("static Fn functions[]={");
    for (unsigned j=0;j<cases.size();++j) std::printf("f%u,\n",j);
    std::puts("};\nstatic const unsigned cases[][8]={");
    for (const auto& c:cases) std::printf("{%u,%u,%u,%u,%u,%u,%u,%u},\n",c.kind,c.uns,c.flag,c.q,c.size,c.rd,c.rn,c.rm);
    std::puts(R"C(};
static uint64_t rng=UINT64_C(0x917e536845672135);
static uint64_t random_word(void) { rng ^= rng<<13; rng ^= rng>>7; rng ^= rng<<17; return rng; }
static uint64_t lane(const void* p,unsigned offset,unsigned bytes) {
    const unsigned char* b=(const unsigned char*)p+offset;
    uint64_t r=0; for(unsigned j=0;j<bytes;++j) r |= (uint64_t)b[j]<<(8*j); return r;
}
static void putlane(void* p,unsigned offset,unsigned bytes,uint64_t v) {
    unsigned char* b=(unsigned char*)p+offset;
    for(unsigned j=0;j<bytes;++j) b[j]=(unsigned char)(v>>(8*j));
}
// Reference works on unsigned bit patterns, with explicit architectural sign
// extension; emitted C uses typed lane arrays and native signed conversion.
static uint64_t extend(uint64_t v,unsigned bytes,unsigned uns) {
    if(!uns && (v & (UINT64_C(1)<<(bytes*8-1)))) v |= UINT64_MAX << (bytes*8);
    return v;
}
static uint64_t select_extreme(uint64_t a,uint64_t b,unsigned bytes,unsigned uns,unsigned minimum) {
    const uint64_t sign=uns?0:UINT64_C(1)<<(bytes*8-1);
    return ((a^sign)<(b^sign)) == minimum ? a:b;
}
int main(void) {
    unsigned checks=0;
    for(unsigned idx=0;idx<sizeof(cases)/sizeof(cases[0]);++idx) {
        const unsigned* t=cases[idx];
        const unsigned k=t[0],u=t[1],flag=t[2],q=t[3],n=1u<<t[4],rd=t[5],rn=t[6],rm=t[7];
        const unsigned active=q?16:8;
        for(unsigned sample=0;sample<263;++sample) {
            GuestContext got,want;
            for(unsigned r=0;r<32;++r) for(unsigned h=0;h<2;++h)
                got.vreg[r][h]=sample==0?0:sample==1?UINT64_MAX:sample==2?UINT64_C(0x8000800080008000):
                    sample==3?UINT64_C(0x7fffffff80000000):sample==4?UINT64_C(0x7fffffffffffffff):
                    sample==5?UINT64_C(0x8000000000000000):sample==6?UINT64_C(0xff00ff00ff00ff00):random_word();
            for(unsigned j=0;j<4;++j) got.sentinel[j]=random_word();
            want=got; memset(want.vreg[rd],0,16);
            if(k<=1) for(unsigned off=0;off<active;off+=n) {
                const unsigned half=off/(active/2), pos=2*(off%(active/2));
                const uint64_t a=lane(got.vreg[half?rm:rn],pos,n),b=lane(got.vreg[half?rm:rn],pos+n,n);
                putlane(want.vreg[rd],off,n,k==0?a+b:select_extreme(a,b,n,u,flag));
            } else if(k==2) {
                uint64_t r=lane(got.vreg[rn],0,n);
                for(unsigned off=n;off<active;off+=n) r=select_extreme(r,lane(got.vreg[rn],off,n),n,u,flag);
                putlane(want.vreg[rd],0,n,r);
            } else if(k<=4) for(unsigned off=0;off<16;off+=2*n) {
                const uint64_t a=k==4?lane(got.vreg[rn],off,2*n):extend(lane(got.vreg[rn],q*8+off/2,n),n,u);
                const uint64_t b=extend(lane(got.vreg[rm],q*8+off/2,n),n,u);
                putlane(want.vreg[rd],off,2*n,flag?a-b:a+b);
            } else if(k==5) for(unsigned off=0;off<active;off+=2*n) {
                const uint64_t a=extend(lane(got.vreg[rn],off,n),n,u),b=extend(lane(got.vreg[rn],off+n,n),n,u);
                putlane(want.vreg[rd],off,2*n,a+b+(flag?lane(got.vreg[rd],off,2*n):0));
            } else if(k==7) for(unsigned off=0;off<16;off+=2*n) {
                const uint64_t a=extend(lane(got.vreg[rn],q*8+off/2,n),n,u);
                const uint64_t b=extend(lane(got.vreg[rm],q*8+off/2,n),n,u);
                const uint64_t sign=u?0:UINT64_C(1)<<63;
                const uint64_t diff=(a^sign)<(b^sign)?b-a:a-b;
                putlane(want.vreg[rd],off,2*n,diff+(flag?lane(got.vreg[rd],off,2*n):0));
            } else putlane(want.vreg[rd],0,8,got.vreg[rn][0]+got.vreg[rn][1]);
            functions[idx](&got); ++checks;
            if(memcmp(&got,&want,sizeof got)) { printf("FAIL idx=%u kind=%u sample=%u\n",idx,k,sample); return 1; }
        }
    }
    printf("%u generated-C integer pair/reduction/widen checks passed\n",checks); return 0;
}
)C");
}
