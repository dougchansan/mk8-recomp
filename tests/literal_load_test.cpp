// PC-relative literal loads, including discarded GPR destinations and address wrap.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
struct Test { unsigned vector,opc,rt; uint64_t address; };
int main(int argc,char**) {
    bool assembly=argc>1;
    if(assembly) std::puts(".text");
    else std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint64_t x[32],vreg[32][2],access[2],count; unsigned char mem[256];} GuestContext;
#define LOAD(B) static uint64_t recomp_load##B(GuestContext* c,uint64_t a) { c->access[c->count++]=a; uint64_t v=0; for(unsigned j=0;j<B/8;j++) v|=(uint64_t)c->mem[(a+j)&255]<<(8*j); return v; }
LOAD(32) LOAD(64)
typedef void (*Fn)(GuestContext*);
)C");
    std::vector<Test> tests;
    for(unsigned vector=0;vector<2;vector++) for(unsigned opc=0;opc<4;opc++) {
        if(vector && opc==3) continue;
        for(int offset : {-1048576,-4,0,4,1048572})
        for(uint64_t pc : {UINT64_C(0x7100000000),UINT64_C(0),UINT64_MAX-3})
        for(unsigned rt : {5u,31u}) {
            unsigned word=0x18000000|(vector<<26)|(opc<<30)|((unsigned(offset/4)&0x7ffff)<<5)|rt;
            if(assembly) {
                std::string m;
                if(!vector && opc==3) m="prfm #"+std::to_string(rt);
                else {
                    m=(!vector && opc==2) ? "ldrsw " : "ldr ";
                    m+=vector ? "sdq"[opc] : opc==0 ? 'w' : 'x';
                    m+=!vector && rt==31 ? "zr" : std::to_string(rt);
                }
                m+=", ."+std::string(offset>=0?"+":"")+std::to_string(offset);
                std::printf("%s\n.inst 0x%08x\n",m.c_str(),word);
            } else {
                std::string out; bool miss=false;
                suyu::recomp::Translate(word,pc,out,&miss);
                if(miss) {std::fprintf(stderr,"missing literal %08x\n",word);return 1;}
                std::printf("static void f%zu(GuestContext* c) {(void)c;%s}\n",tests.size(),out.c_str());
            }
            tests.push_back({vector,opc,rt,pc+uint64_t(int64_t(offset))});
        }
    }
    if(assembly) return 0;
    for(unsigned word : {0xDC000000u,0xDCFFFFE0u}) {
        std::string out; bool miss=false;
        suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss) {std::fprintf(stderr,"reserved load decoded %08x\n",word);return 1;}
    }
    std::fprintf(stderr,"2 literal reserved checks passed\n");
    std::puts("static struct {Fn fn; unsigned vector,opc,rt; uint64_t addr;} tests[]={");
    for(unsigned n=0;n<tests.size();n++) {
        const auto& t=tests[n];
        std::printf("{f%u,%u,%u,%u,UINT64_C(0x%llx)},\n",n,t.vector,t.opc,t.rt,(unsigned long long)t.address);
    }
    std::puts(R"C(};
int main(void) {
    unsigned checks=0;
    for(unsigned n=0;n<sizeof(tests)/sizeof(tests[0]);n++) for(unsigned sample=0;sample<4;sample++) {
        GuestContext actual,expected;
        for(unsigned j=0;j<sizeof(actual);j++) ((unsigned char*)&actual)[j]=(unsigned char)(j*37+sample*73);
        actual.count=0;expected=actual;
        if(tests[n].vector || tests[n].opc!=3) {
            const unsigned bytes=tests[n].vector ? 4u<<tests[n].opc : tests[n].opc==1?8:4;
            unsigned char value[16]={0};
            for(unsigned j=0;j<bytes;j++) value[j]=actual.mem[(tests[n].addr+j)&255];
            if(tests[n].vector) memcpy(expected.vreg[tests[n].rt],value,16);
            else if(tests[n].rt!=31) {
                if(tests[n].opc==2 && value[3]>=128) memset(value+4,255,4);
                memcpy(&expected.x[tests[n].rt],value,8);
            }
            expected.access[0]=tests[n].addr;expected.count=1;
            if(bytes==16) { expected.access[1]=tests[n].addr+8;expected.count=2; }
        }
        tests[n].fn(&actual);checks++;
        if(memcmp(&actual,&expected,sizeof actual)) {printf("FAIL literal %u sample %u\n",n,sample);return 1;}
    }
    printf("%u generated-C literal-load checks passed\n",checks);
    return 0;
}
)C");
}
