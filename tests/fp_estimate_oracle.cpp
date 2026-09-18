// Export synthetic S/D estimate vectors using the active Dynarmic software oracle.
// Link against the existing libdynarmic.a; redirect stdout to a local .txt file.
#include <cstdio>
#include <cstdint>
#include <vector>
#include "dynarmic/common/fp/fpcr.h"
#include "dynarmic/common/fp/fpsr.h"
#include "dynarmic/common/fp/op/FPRecipEstimate.h"
#include "dynarmic/common/fp/op/FPRSqrtEstimate.h"
int main() {
    uint64_t seed=0xc7615a4bc0928ed1ULL;
    for(unsigned width:{32u,64u}) for(unsigned op:{0u,1u}) {
        const unsigned f=width==32?23:52,e=width==32?8:11;
        const uint64_t sign=1ULL<<(width-1),hidden=1ULL<<f,inf=((1ULL<<e)-1)<<f;
        const uint64_t one=uint64_t((1u<<(e-1))-1)<<f;
        std::vector<uint64_t> values={0,sign,inf,inf|sign,inf|(hidden>>1)|17,
            inf|17,inf|sign|17,1,2,hidden/4-1,hidden/4,hidden/4+1,
            hidden/2,hidden-1,hidden,inf-1,inf-hidden,inf-2*hidden,
            inf-3*hidden,one,one|sign,one+hidden,one+2*hidden};
        for(unsigned j=0;j<256;j++){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;
            values.push_back(width==32?seed&0xffffffff:seed);}
        for(unsigned control=0;control<16;control++) for(uint64_t bits:values) {
            uint32_t fpcr=(control&3)<<22|((control>>2)&1)<<24|((control>>3)&1)<<25;
            Dynarmic::FP::FPCR cr(fpcr);Dynarmic::FP::FPSR sr(0x08000000);
            uint64_t result=width==32
                ? (op?Dynarmic::FP::FPRSqrtEstimate<uint32_t>((uint32_t)bits,cr,sr)
                     :Dynarmic::FP::FPRecipEstimate<uint32_t>((uint32_t)bits,cr,sr))
                : (op?Dynarmic::FP::FPRSqrtEstimate<uint64_t>(bits,cr,sr)
                     :Dynarmic::FP::FPRecipEstimate<uint64_t>(bits,cr,sr));
            std::printf("%u %u %x %llx %llx %x\n",width,op,fpcr,
                        (unsigned long long)bits,(unsigned long long)result,sr.Value());
        }
    }
}
