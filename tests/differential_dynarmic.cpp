#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include "dynarmic/interface/A64/a64.h"
#include "differential_state.h"
struct Callbacks final : Dynarmic::A64::UserCallbacks {
    uint32_t word=0;
    bool unsupported=false;
    std::optional<uint32_t> MemoryReadCode(uint64_t va) override {return va==0x1000?std::optional<uint32_t>(word):std::nullopt;}
    template<class T>T read(uint64_t){unsupported=true;return {};}
    uint8_t MemoryRead8(uint64_t a)override{return read<uint8_t>(a);}
    uint16_t MemoryRead16(uint64_t a)override{return read<uint16_t>(a);}
    uint32_t MemoryRead32(uint64_t a)override{return read<uint32_t>(a);}
    uint64_t MemoryRead64(uint64_t a)override{return read<uint64_t>(a);}
    Dynarmic::A64::Vector MemoryRead128(uint64_t a)override{return read<Dynarmic::A64::Vector>(a);}
    void MemoryWrite8(uint64_t,uint8_t)override{unsupported=true;}
    void MemoryWrite16(uint64_t,uint16_t)override{unsupported=true;}
    void MemoryWrite32(uint64_t,uint32_t)override{unsupported=true;}
    void MemoryWrite64(uint64_t,uint64_t)override{unsupported=true;}
    void MemoryWrite128(uint64_t,Dynarmic::A64::Vector)override{unsupported=true;}
    void CallSVC(uint32_t)override{unsupported=true;}
    void ExceptionRaised(uint64_t,Dynarmic::A64::Exception)override{unsupported=true;}
    void AddTicks(uint64_t)override{}
    uint64_t GetTicksRemaining()override{return 1;}
    uint64_t GetCNTPCT()override{return 0;}
};
static uint64_t random_state;
static uint64_t next(){random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return random_state;}
static uint32_t nzcv(const GuestContext&c){return (uint32_t(c.n)<<31)|(uint32_t(c.z)<<30)|(uint32_t(c.c)<<29)|(uint32_t(c.v)<<28);}
int main(int argc,char**argv){
    uint64_t seed=UINT64_C(0xc7615a4bc0928ed1);unsigned samples=256,group=0;bool inject=false;
    uint32_t only_word=0;
    for(int j=1;j<argc;j++){
        if(std::string_view(argv[j])=="--fp")group=1;
        else if(std::string_view(argv[j])=="--fp-new")group=2;
        else if(std::string_view(argv[j])=="--word"&&j+1<argc)only_word=(uint32_t)std::strtoul(argv[++j],nullptr,0);
        else if(std::string_view(argv[j])=="--inject-mismatch")inject=true;
        else if(std::string_view(argv[j])=="--seed"&&j+1<argc)seed=std::strtoull(argv[++j],nullptr,0);
        else if(std::string_view(argv[j])=="--samples"&&j+1<argc)samples=(unsigned)std::strtoul(argv[++j],nullptr,0);
        else{std::fprintf(stderr,"unknown/incomplete argument %s\n",argv[j]);return 2;}
    }
    if(!samples||!seed){std::fputs("seed and samples must be nonzero\n",stderr);return 2;}
    Callbacks cb;Dynarmic::A64::UserConfig config{};config.callbacks=&cb;
    Dynarmic::A64::Jit jit(config);unsigned checks=0;
    for(unsigned op=0;op<differential_count();op++){
        if(differential_is_fp(op)!=group||(only_word&&differential_word(op)!=only_word))continue;
        cb.word=differential_word(op);
        for(unsigned sample=0;sample<samples;sample++){
            // Each case has an independently reproducible seed, unaffected by skips.
            const uint64_t case_seed=seed^(uint64_t(cb.word)<<17)^sample;random_state=case_seed;
            GuestContext c{};
            for(auto&x:c.x)x=next();
            for(auto&r:c.vreg)for(auto&x:r)x=next();
            const uint64_t edges[]={0,UINT64_MAX,UINT64_C(0x8000800080008000),UINT64_C(0x7fff7fff7fff7fff)};
            if(sample<4)for(auto&r:c.vreg)for(auto&x:r)x=edges[sample];
            if(group){ // First four exact values; later full IEEE payloads.
                const double values[]={0.0,1.0,-1.0,2.0};
                if(sample<4)for(auto&r:c.vreg)for(auto&x:r)std::memcpy(&x,&values[sample],8);
            }
            if(group==2){
                const uint64_t fp_edges[]={0,UINT64_C(0x8000000000000000),UINT64_C(0x7ff0000000000000),
                    UINT64_C(0xfff0000000000000),UINT64_C(0x7ff8000000000012),UINT64_C(0x7ff0000000000012),
                    1,UINT64_C(0x7fefffffffffffff),UINT64_C(0x7fc000127fc00012),UINT64_C(0x7f8000127f800012),
                    UINT64_C(0x0000000100000001),UINT64_C(0x3ff8000000000000)};
                if(sample/16<12)for(auto&r:c.vreg)for(auto&x:r)x=fp_edges[sample/16];
            }
            c.n=next()&1;c.z=next()&1;c.c=next()&1;c.v=next()&1;
            c.fpcr=group==2?uint64_t(sample&15)<<22:0;c.fpsr=(sample&1)?0x08000000:0;
            jit.Reset();if(sample==0)jit.ClearCache();jit.SetPC(0x1000);jit.SetSP(c.x[31]);
            for(unsigned r=0;r<31;r++)jit.SetRegister(r,c.x[r]);
            for(unsigned r=0;r<32;r++)jit.SetVector(r,{c.vreg[r][0],c.vreg[r][1]});
            jit.SetFpcr((uint32_t)c.fpcr);jit.SetFpsr((uint32_t)c.fpsr);
            jit.SetPstate(nzcv(c));
            cb.unsupported=false;jit.Step();
            if(cb.unsupported){std::fprintf(stderr,"ORACLE_UNSUPPORTED word=%08x\n",cb.word);return 2;}
            differential_emit(op,&c);if(inject)c.x[0]^=1;
            auto mismatch=[&](const char*field,unsigned index,uint64_t actual,uint64_t expected){
                std::fprintf(stderr,"DIVERGENCE seed=0x%016llx case_seed=0x%016llx word=%08x sample=%u fpcr=%08x field=%s[%u] emitted=%016llx dynarmic=%016llx\n",
                    (unsigned long long)seed,(unsigned long long)case_seed,cb.word,sample,jit.GetFpcr(),field,index,(unsigned long long)actual,(unsigned long long)expected);
                return 1;};
            for(unsigned r=0;r<31;r++)if(c.x[r]!=jit.GetRegister(r))return mismatch("x",r,c.x[r],jit.GetRegister(r));
            if(c.x[31]!=jit.GetSP())return mismatch("sp",0,c.x[31],jit.GetSP());
            for(unsigned r=0;r<32;r++)for(unsigned h=0;h<2;h++)if(c.vreg[r][h]!=jit.GetVector(r)[h])return mismatch("v",r*2+h,c.vreg[r][h],jit.GetVector(r)[h]);
            if(nzcv(c)!=(jit.GetPstate()&0xf0000000))return mismatch("nzcv",0,nzcv(c),jit.GetPstate()&0xf0000000);
            if(c.fpcr!=jit.GetFpcr())return mismatch("fpcr",0,c.fpcr,jit.GetFpcr());
            if(c.fpsr!=jit.GetFpsr())return mismatch("fpsr",0,c.fpsr,jit.GetFpsr());
            if(jit.GetPC()!=0x1004)return mismatch("pc",0,0x1004,jit.GetPC());
            checks++;
        }
    }
    if(!checks){std::fputs("no matching cases\n",stderr);return 2;}
    std::printf("%u actual-Dynarmic differential cases passed (group %u)\n",checks,group);
}
