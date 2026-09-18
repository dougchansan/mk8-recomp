// All integer modified-immediate encodings and destination read/modify/write.
// --asm emits GNU assembly mnemonic/.inst pairs for independent encoding checks.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"

int main(int argc, char**) {
    const bool assembly = argc > 1;
    if (assembly) std::puts(".text");
    else std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t vreg[32][2], sentinel[4]; } GuestContext;
typedef void (*Fn)(GuestContext*);
)C");
    unsigned count = 0;
    for (unsigned q=0;q<2;++q) for(unsigned op=0;op<2;++op)
    for(unsigned cmode=0;cmode<15;++cmode) for(unsigned imm=0;imm<256;++imm) {
        const unsigned rd=imm&1 ? 31 : 7;
        const unsigned word=0x0F000400|(q<<30)|(op<<29)|(cmode<<12)|
                            ((imm>>5)<<16)|((imm&31)<<5)|rd;
        if (assembly) {
            const bool combine=cmode<12 && (cmode&1);
            std::string m=combine ? (op ? "bic" : "orr") : op && cmode<14 ? "mvni" : "movi";
            const unsigned element=cmode<8 || cmode==12 || cmode==13 ? 4 : cmode<12 ? 2 : op ? 8 : 1;
            m += op && cmode==14 && !q ? " d"+std::to_string(rd) :
                 " v"+std::to_string(rd)+"."+std::to_string((q?16:8)/element)+(element==4?"s":element==2?"h":element==8?"d":"b");
            if (op && cmode==14) {
                uint64_t value=0;
                for(unsigned bit=0;bit<8;++bit) if(imm&(1<<bit)) value|=UINT64_C(255)<<(bit*8);
                char literal[32]; std::snprintf(literal,sizeof literal,", #0x%llx",(unsigned long long)value);
                m += literal;
            } else {
                m += ", #"+std::to_string(imm);
                if(cmode<12) m += ", lsl #"+std::to_string(cmode<8 ? (cmode/2)*8 : ((cmode-8)/2)*8);
                if(cmode==12 || cmode==13) m += ", msl #"+std::to_string((cmode-11)*8);
            }
            std::printf("%s\n.inst 0x%08x\n",m.c_str(),word);
        } else {
            std::string out; bool miss=false;
            suyu::recomp::Translate(word,0x1000,out,&miss);
            if(miss) { std::fprintf(stderr,"missing modified immediate %08x\n",word); return 1; }
            std::printf("static void f%u(GuestContext* c) {%s}\n",count,out.c_str());
        }
        ++count;
    }
    if(assembly) return 0;
    unsigned negatives=0;
    for(unsigned q=0;q<2;++q) for(unsigned op=0;op<2;++op)
    for(unsigned cmode=0;cmode<15;++cmode) {
        const unsigned word=0x0F000C00|(q<<30)|(op<<29)|(cmode<<12);
        std::string out; bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss) { std::fprintf(stderr,"reserved modified immediate decoded %08x\n",word); return 1; }
        ++negatives;
    }
    std::fprintf(stderr,"%u modified-immediate reserved checks passed\n",negatives);
    std::puts("static Fn functions[]={");
    for(unsigned n=0;n<count;++n) std::printf("f%u,\n",n);
    std::puts(R"C(};
int main(void) {
    unsigned n=0,checks=0;
    for(unsigned q=0;q<2;q++) for(unsigned op=0;op<2;op++)
    for(unsigned mode=0;mode<15;mode++) for(unsigned imm=0;imm<256;imm++) {
        const unsigned rd=imm&1 ? 31 : 7;
        for(unsigned sample=0;sample<4;sample++) {
            GuestContext actual,expected;
            for(unsigned b=0;b<sizeof(actual);b++) ((unsigned char*)&actual)[b]=sample==0?0:sample==1?255:(unsigned char)(b*37+sample*73);
            expected=actual;
            unsigned char* result=(unsigned char*)expected.vreg[rd];
            // Construct each immediate byte from lane position, independently of
            // the emitter's 64-bit shifts and replication.
            for(unsigned b=0;b<(q?16:8);b++) {
                unsigned value;
                if(mode<8) value=b%4==mode/2 ? imm : 0;
                else if(mode<12) value=b%2==(mode-8)/2 ? imm : 0;
                else if(mode<14) {
                    const unsigned low=mode-11;
                    value=b%4<low ? 255 : b%4==low ? imm : 0;
                } else value=op ? ((imm>>(b%8))&1 ? 255 : 0) : imm;
                if(mode<12 && (mode&1)) {
                    // Bit-by-bit Boolean oracle rather than host AND/OR of a mask.
                    unsigned merged=0;
                    for(unsigned bit=0;bit<8;bit++) {
                        const unsigned old=(result[b]>>bit)&1, mask=(value>>bit)&1;
                        if(op ? old && !mask : old || mask) merged+=1u<<bit;
                    }
                    result[b]=(unsigned char)merged;
                } else result[b]=(unsigned char)(op && mode<14 ? 255-value : value);
            }
            if(!q) memset(result+8,0,8);
            functions[n](&actual); checks++;
            if(memcmp(&actual,&expected,sizeof(actual))) { printf("FAIL case %u sample %u\n",n,sample); return 1; }
        }
        n++;
    }
    printf("%u generated-C modified-immediate checks passed\n",checks);
    return 0;
}
)C");
}
