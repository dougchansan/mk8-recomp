// Synthetic structure transfers: generated-C execution and GNU assembler oracle.
// --asm emits mnemonic/.inst pairs; their object words must match pairwise.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"

struct Case {
    unsigned word, kind, regs, bytes, active, lane, load, post, rn, rt, rm;
    std::string assembly;
};

int main(int argc, char**) {
    const bool assembly = argc > 1;
    std::vector<Case> cases;
    for (unsigned kind = 0; kind < 4; ++kind)
    for (unsigned regs = 1; regs <= 4; ++regs)
    for (unsigned size = 0; size < 4; ++size)
    for (unsigned q = 0; q < 2; ++q)
    for (unsigned load = 0; load < 2; ++load)
    for (unsigned post = 0; post < 4; ++post)
    for (unsigned alias = 0; alias < 2; ++alias) {
        if (kind == 1 && !load) continue;
        if (kind == 2 && size == 3 && q == 0 && regs != 1) continue;
        const unsigned bytes = 1u << size, active = q ? 16 : 8;
        const unsigned rn = alias ? 31 : 3, rt = alias ? 31 : 7;
        const unsigned rm = post == 1 ? 31 : post == 3 ? rn : 4;
        // Rm=31 means immediate post-index, including with an SP base.
        for (unsigned low = 0; low < (kind == 0 ? 8 / bytes : 1); ++low) {
            const unsigned lane = q * (8 / bytes) + low;
            unsigned word;
            if (kind == 0) {
                const unsigned opcode = (size < 3 ? size * 2 : 4) + (regs - 1) / 2;
                const unsigned s = size < 3 ? (low >> (2 - size)) & 1 : 0;
                const unsigned sz = size == 0 ? low % 4 : size == 1 ? (low % 2)*2 : size == 2 ? 0 : 1;
                word = 0x0D000000 | (opcode << 13) | (((regs - 1) & 1) << 21) | (s << 12) | (sz << 10);
            } else if (kind == 1) {
                word = 0x0D00C000 | (((regs - 1) / 2) << 13) | (((regs - 1) & 1) << 21) | (size << 10);
            } else {
                const unsigned interleaved[] = {0, 7, 8, 4, 0};
                const unsigned consecutive[] = {0, 7, 10, 6, 2};
                word = 0x0C000000 | ((kind == 3 ? consecutive[regs] : interleaved[regs]) << 12) | (size << 10);
            }
            word |= (q << 30) | (load << 22) | (rn << 5) | rt;
            if (post) word |= 0x00800000 | (rm << 16);
            std::string mnemonic = load ? "ld" : "st";
            mnemonic += std::to_string(kind == 3 ? 1 : regs);
            if (kind == 1) mnemonic += "r";
            mnemonic += " {";
            for (unsigned r = 0; r < regs; ++r) {
                if (r) mnemonic += ", ";
                mnemonic += "v" + std::to_string((rt+r)&31) + ".";
                if (kind != 0) mnemonic += std::to_string(active/bytes);
                mnemonic += "bhsd"[size];
            }
            mnemonic += "}";
            if (kind == 0) mnemonic += "[" + std::to_string(lane) + "]";
            mnemonic += ", [" + (rn == 31 ? std::string("sp") : "x"+std::to_string(rn)) + "]";
            if (post) mnemonic += rm == 31 ? ", #"+std::to_string(regs*(kind >= 2 ? active : bytes)) : ", x"+std::to_string(rm);
            cases.push_back({word,kind,regs,bytes,active,lane,load,post,rn,rt,rm,mnemonic});
        }
    }
    if (assembly) {
        std::puts(".text");
        for (const auto& t : cases) std::printf("%s\n.inst 0x%08x\n", t.assembly.c_str(), t.word);
        return 0;
    }
    unsigned negatives = 0;
    auto reject = [&](unsigned word) {
        std::string out; bool miss = false;
        suyu::recomp::Translate(word, 0x1000, out, &miss);
        ++negatives;
        if (!miss) { std::fprintf(stderr,"reserved structure decoded: %08x\n",word); std::exit(1); }
    };
    for (unsigned regs = 0; regs < 4; ++regs) {
        const unsigned r = ((regs&1)<<21) | ((regs/2)<<13);
        for (unsigned q = 0; q < 2; ++q) {
            const unsigned base = 0x0D400000 | (q<<30) | r;
            reject(base | 0x4400); // Halfword odd size.
            reject(base | 0x8800); // Word reserved size.
            reject(base | 0x9400); // Doubleword S=1.
            reject(base | 0xD000); // Replicate S=1.
            reject((base & ~0x00400000) | 0xC000); // Replicate store.
            reject(base | 0x00010000); // No-offset Rm field is reserved.
            reject(base | 0xC000 | 0x00010000);
        }
    }
    for (unsigned word : {0x0C200000u,0x0CA00000u,0x4CFF0000u,0x0C400C00u,
                          0x0C404C00u,0x0C408C00u,0x8D400000u}) reject(word);
    std::fprintf(stderr,"%u structure reserved checks passed\n",negatives);
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t x[32], vreg[32][2]; unsigned char mem[512]; uint64_t access[64], count; } GuestContext;
#define MEMORY(B) \
static uint64_t recomp_load##B(GuestContext* c,uint64_t a) { c->access[c->count++]=a; uint64_t v=0; for(unsigned j=0;j<B/8;j++) v|=(uint64_t)c->mem[a+j]<<(8*j); return v; } \
static void recomp_store##B(GuestContext* c,uint64_t a,uint64_t v) { c->access[c->count++]=a; for(unsigned j=0;j<B/8;j++) c->mem[a+j]=(unsigned char)(v>>(8*j)); }
MEMORY(8) MEMORY(16) MEMORY(32) MEMORY(64)
typedef void (*Fn)(GuestContext*);
)C");
    for (unsigned n = 0; n < cases.size(); ++n) {
        std::string out; bool miss = false;
        suyu::recomp::Translate(cases[n].word,0x1000,out,&miss);
        if(miss) { std::fprintf(stderr,"missing %08x %s\n",cases[n].word,cases[n].assembly.c_str()); return 1; }
        std::printf("static void f%u(GuestContext* c) {%s}\n",n,out.c_str());
    }
    std::puts("static struct {Fn fn; unsigned kind,regs,bytes,active,lane,load,post,rn,rt,rm;} cases[]={");
    for(unsigned n=0;n<cases.size();++n) {
        const auto& t=cases[n];
        std::printf("{f%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u},\n",n,t.kind,t.regs,t.bytes,t.active,t.lane,t.load,t.post,t.rn,t.rt,t.rm);
    }
    std::puts(R"C(};
int main(void) {
    unsigned checks=0;
    for(unsigned n=0;n<sizeof(cases)/sizeof(cases[0]);n++) for(unsigned pattern=0;pattern<4;pattern++) {
        GuestContext actual,expected;
        for(unsigned j=0;j<sizeof(actual);j++) ((unsigned char*)&actual)[j]=(unsigned char)(j*37+pattern*61);
        actual.x[cases[n].rn]=96;
        actual.count=0;
        if(cases[n].rm!=cases[n].rn && cases[n].rm!=31) actual.x[cases[n].rm]=13;
        expected=actual;
        const unsigned count=cases[n].kind>=2 ? cases[n].active/cases[n].bytes : 1;
        unsigned address=96;
        // Reference consumes a contiguous memory stream, scattering bytes into registers.
        for(unsigned lane=0;lane<count;lane++) for(unsigned r=0;r<cases[n].regs;r++) {
            unsigned char* v=(unsigned char*)expected.vreg[(cases[n].rt+r)&31];
            unsigned off=cases[n].kind==0 ? cases[n].lane*cases[n].bytes : lane*cases[n].bytes;
            const unsigned source=cases[n].kind==3 ? 96+r*cases[n].active+lane*cases[n].bytes : address;
            for(unsigned j=0;j<cases[n].bytes;j++) {
                if(cases[n].load) v[off+j]=actual.mem[source+j];
                else expected.mem[source+j]=v[off+j];
            }
            if(cases[n].kind==1) for(unsigned j=cases[n].bytes;j<cases[n].active;j++) v[j]=v[j%cases[n].bytes];
            address+=cases[n].bytes;
        }
        if(cases[n].load && cases[n].kind!=0 && cases[n].active==8)
            for(unsigned r=0;r<cases[n].regs;r++) expected.vreg[(cases[n].rt+r)&31][1]=0;
        if(cases[n].post) expected.x[cases[n].rn]=96+(cases[n].rm==31 ? address-96 : actual.x[cases[n].rm]);
        expected.count=count*cases[n].regs;
        for(unsigned j=0;j<expected.count;j++) expected.access[j]=96+j*cases[n].bytes;
        cases[n].fn(&actual); checks++;
        if(memcmp(&actual,&expected,sizeof(actual))) { printf("FAIL case %u pattern %u\n",n,pattern); return 1; }
    }
    printf("%u generated-C structure transfer checks passed\n",checks);
    return 0;
}
)C");
}
