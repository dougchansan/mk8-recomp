// Synthetic generated-C SIMD integer semantics and assembler input generator.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Op { unsigned base,kind,uns,flag; const char* name; };
static const Op ops[]={
    {0x2e204800,0,1,0,"clz"},{0x0e204800,0,0,0,"cls"},
    {0x0e200400,1,0,0,"shadd"},{0x2e200400,1,1,0,"uhadd"},
    {0x0e202400,1,0,1,"shsub"},{0x2e202400,1,1,1,"uhsub"},
    {0x0e201400,1,0,2,"srhadd"},{0x2e201400,1,1,2,"urhadd"},
    {0x0e207400,2,0,0,"sabd"},{0x2e207400,2,1,0,"uabd"},
    {0x0e207c00,2,0,1,"saba"},{0x2e207c00,2,1,1,"uaba"},
    {0x0e204000,3,0,0,"addhn"},{0x2e204000,3,1,0,"raddhn"},
    {0x0e206000,3,0,1,"subhn"},{0x2e206000,3,1,1,"rsubhn"},
};
struct Case { unsigned kind,uns,flag,q,size,rd,rn,rm; };
int main(int argc,char**) {
    const bool assembly=argc>1;
    std::vector<Case> cases;
    if(assembly) std::puts(".text");
    else std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\ntypedef struct { uint64_t vreg[32][2]; uint64_t sentinel[4]; } GuestContext;\ntypedef void (*Fn)(GuestContext*);");
    unsigned negatives=0;
    for(const auto& op:ops) {
        const unsigned word=op.base|0xe0000020u|(op.kind?2<<16:0);
        std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss) {std::fprintf(stderr,"high-bit reserved accepted %08x\n",word);return 1;}
        ++negatives;
    }
    for(const auto& op:ops) for(unsigned q=0;q<2;++q) {
        const unsigned negative=op.base|(q<<30)|(3<<22)|32|(op.kind?2<<16:0);
        std::string out; bool miss=false;
        suyu::recomp::Translate(negative,0x1000,out,&miss);
        if(!miss) {std::fprintf(stderr,"reserved accepted %08x\n",negative);return 1;}
        ++negatives;
        for(unsigned size=0;size<3;++size) for(unsigned alias=0;alias<5;++alias) {
            const unsigned rd=alias==4?31:alias==3?1:alias,rn=alias==4?31:1,rm=alias==4?31:alias==3?1:2;
            const unsigned word=op.base|(q<<30)|(size<<22)|(rn<<5)|rd|(op.kind?rm<<16:0);
            const unsigned bytes=1u<<size,active=q?16:8; const char elt[]={'b','h','s','d'};
            if(assembly) {
                if(op.kind==0) std::printf("%s v%u.%u%c,v%u.%u%c\n",op.name,rd,active/bytes,elt[size],rn,active/bytes,elt[size]);
                else if(op.kind==3) std::printf("%s%s v%u.%u%c,v%u.%u%c,v%u.%u%c\n",op.name,q?"2":"",rd,active/bytes,elt[size],rn,8/bytes,elt[size+1],rm,8/bytes,elt[size+1]);
                else std::printf("%s v%u.%u%c,v%u.%u%c,v%u.%u%c\n",op.name,rd,active/bytes,elt[size],rn,active/bytes,elt[size],rm,active/bytes,elt[size]);
                std::fprintf(stderr,"%08x\n",word);
            } else {
                out.clear();miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
                if(miss) {std::fprintf(stderr,"missing %s %08x\n",op.name,word);return 1;}
                std::printf("static void f%zu(GuestContext* c) {%s}\n",cases.size(),out.c_str());
            }
            cases.push_back({op.kind,op.uns,op.flag,q,size,rd,rn,rm});
        }
    }
    if(assembly)return 0;
    std::fprintf(stderr,"%u reserved encoding checks passed\n",negatives);
    std::puts("static Fn functions[]={");
    for(unsigned j=0;j<cases.size();++j)std::printf("f%u,\n",j);
    std::puts("};\nstatic const unsigned cases[][8]={");
    for(const auto& c:cases)std::printf("{%u,%u,%u,%u,%u,%u,%u,%u},\n",c.kind,c.uns,c.flag,c.q,c.size,c.rd,c.rn,c.rm);
    std::puts(R"C(};
static uint64_t rng=UINT64_C(0x32ce182691734516);
static uint64_t random_word(void) {rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return rng;}
static uint64_t lane(const void* p,unsigned off,unsigned n) {
    const unsigned char* b=(const unsigned char*)p+off;uint64_t r=0;
    for(unsigned j=0;j<n;++j)r|=(uint64_t)b[j]<<(8*j);
    return r;
}
static void putlane(void* p,unsigned off,unsigned n,uint64_t v) {
    unsigned char* b=(unsigned char*)p+off;for(unsigned j=0;j<n;++j)b[j]=(unsigned char)(v>>(8*j));
}
static uint64_t extend(uint64_t v,unsigned bits,unsigned uns) {
    return !uns && (v&(UINT64_C(1)<<(bits-1)))?v| (UINT64_MAX<<bits):v;
}
// Bit-pattern oracles deliberately avoid the signed lane arithmetic emitted.
static uint64_t oracle(unsigned k,unsigned u,unsigned flag,unsigned bits,uint64_t a,uint64_t b,uint64_t d) {
    const uint64_t sign=UINT64_C(1)<<(bits-1),mask=(sign-1)|sign;
    if(k==0) {
        unsigned count=0;
        if(!u && (a&sign))a^=mask;
        if(a==0)return bits-(u?0:1);
        while(!(a&sign)) {a<<=1;++count;}
        return count-(u?0:1);
    }
    if(k==1) {
        a=extend(a,bits,u);b=extend(b,bits,u);
        return ((flag==1?a-b:a+b+(flag==2))>>1)&mask;
    }
    if(k==2) {
        const uint64_t order=u?0:sign;
        const uint64_t diff=((a^order)<(b^order)?b-a:a-b)&mask;
        return (diff+(flag?d:0))&mask;
    }
    // Narrow reference uses a bytewise ripple carry/borrow through the full
    // source width, including the optional rounding bias at the cut point.
    unsigned char x[8]; unsigned carry=0;
    for(unsigned j=0;j<bits/4;++j) {
        const unsigned av=(unsigned)((a>>(8*j))&255),bv=(unsigned)((b>>(8*j))&255);
        const unsigned sum=av+(flag?(bv^255):bv)+carry+(j==0&&flag);
        x[j]=(unsigned char)sum;carry=sum>>8;
    }
    if(u) {
        unsigned j=bits/8-1, sum=(unsigned)x[j]+128;x[j]=(unsigned char)sum;carry=sum>>8;
        while(carry && ++j<bits/4) {sum=(unsigned)x[j]+carry;x[j]=(unsigned char)sum;carry=sum>>8;}
    }
    return lane(x,bits/8,bits/8);
}
int main(void) {
    unsigned checks=0;
    for(unsigned idx=0;idx<sizeof(cases)/sizeof(cases[0]);++idx) {
        const unsigned* t=cases[idx];const unsigned k=t[0],u=t[1],f=t[2],q=t[3],n=1u<<t[4],rd=t[5],rn=t[6],rm=t[7];
        const unsigned samples=n==1&&k!=3?65536:521;
        for(unsigned sample=0;sample<samples;++sample) {
            GuestContext got,want;
            for(unsigned r=0;r<32;++r)for(unsigned h=0;h<2;++h)
                got.vreg[r][h]=sample==0?0:sample==1?UINT64_MAX:sample==2?UINT64_C(0x8000800080008000):
                    sample==3?UINT64_C(0x7fffffff80000000):sample==4?UINT64_C(0x7fffffffffffffff):
                    sample==5?UINT64_C(0x8000000000000000):random_word();
            if(n==1 && k!=3) {
                memset(got.vreg[rn],sample&255,16);
                if(rm!=rn)memset(got.vreg[rm],sample>>8,16);
            }
            for(unsigned j=0;j<4;++j)got.sentinel[j]=random_word();
            want=got;
            if(k!=3 || !q)memset(want.vreg[rd],0,16);
            const unsigned active=k==3?8:q?16:8;
            for(unsigned off=0;off<active;off+=n) {
                const unsigned source=k==3?off*2:off,sz=k==3?n*2:n;
                const uint64_t a=lane(got.vreg[rn],source,sz),b=lane(got.vreg[rm],source,sz),d=lane(got.vreg[rd],off,n);
                putlane(want.vreg[rd],off+(k==3?q*8:0),n,oracle(k,u,f,n*8,a,b,d));
            }
            functions[idx](&got);++checks;
            if(memcmp(&got,&want,sizeof got)) {printf("FAIL idx=%u kind=%u sample=%u\n",idx,k,sample);return 1;}
        }
    }
    printf("%u generated-C SIMD misc checks passed\n",checks);return 0;
}
)C");
}
