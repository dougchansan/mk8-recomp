#include "recomp_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
extern BlockFn recomp_image_lookup(uint64_t);
extern void recomp_image_set_base(uint64_t);
extern unsigned recomp_image_guard_v2(unsigned);
static unsigned char memory[128];
static int missing_zero;
static unsigned checked_reads;
static GuestContext* observed_context;
static const uint32_t words[]={0xd2800020,0xd5033fdf,0xd2800041,0xd50b7522,0xd2800063,0xd4200000,
                               0x14000001,0xd2800084,0xd4200000};
static uint64_t load(void* user,uint64_t va,uint32_t size){
    uint64_t result=0;(void)user;
    if(size==0){
        ++checked_reads;
        if(va<0x1000 || va-0x1000>sizeof memory-4 || (missing_zero && va==0x1044))return 0;
        memcpy(&result,memory+va-0x1000,4);
        return result|UINT64_C(0x100000000);
    }
    if(va>=0x1000 && va-0x1000+size<=sizeof memory)memcpy(&result,memory+va-0x1000,size);
    return result;
}
static void observe_guard_abort(int signal_number){
    (void)signal_number;
    if(observed_context && observed_context->x[6]==0 && observed_context->pc==0x1044){
        fputs("guard abort before any block effect\n",stderr);fflush(stderr);_Exit(86);
    }
    _Exit(87);
}
static void zero_block(GuestContext* c){
    const uint32_t expected[]={0xd28000c6,0};
    recomp_code_guard(c,0x1040,expected,2,g_recomp_guard_host_v2);
    c->x[6]=6;
    recomp_unhandled(c,0,0x1044);
}
static unsigned old_guard_v1(unsigned version){(void)version;return 1;}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char** argv){
#ifdef _WIN32
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    GuestContext c;RecompHostMem bridge;const char* mode=argc>1?argv[1]:"hosted";
    memset(&c,0,sizeof c);memset(&bridge,0,sizeof bridge);memcpy(memory,words,sizeof words);
    c.mem=memory;c.mem_size=sizeof memory;c.mem_base_vaddr=0x1000;c.pc=0x1000;
    c.pending_svc=~UINT64_C(0);c.chain_budget=32;c.x[2]=0x10ff;
    bridge.load=load;if(strcmp(mode,"standalone"))c.host_mem=&bridge;
    recomp_image_set_base(0);CHECK(recomp_image_guard_v2(2)==2);
    if(!strcmp(mode,"hosted-unmapped-zero") || !strcmp(mode,"mapped-zero") || !strcmp(mode,"special-mapped-zero")){
        const uint32_t zero_code[]={0xd28000c6,0};memcpy(memory+64,zero_code,sizeof zero_code);
        missing_zero=!strcmp(mode,"hosted-unmapped-zero");
        if(missing_zero){observed_context=&c;signal(SIGABRT,observe_guard_abort);}
        zero_block(&c);
        CHECK(!missing_zero && c.x[6]==6 && c.pc==0x1044 && c.halted==RECOMP_HALT_UNHANDLED && checked_reads==2);
        puts("mapped zero reaches intended trap passed");return 0;
    }
    recomp_image_lookup(c.pc)(&c);
    CHECK(c.pc==0x1008 && c.x[0]==1 && c.x[1]==0 && c.halted==0);
    if(!strcmp(mode,"mutated")){memory[8]^=0x20;}
    if(!strcmp(mode,"missing")){recomp_image_guard_v2(0);}
    if(!strcmp(mode,"legacy") || !strcmp(mode,"v1")){
        // A mixed set must negotiate version zero for every capable module.
        unsigned (*modules[])(unsigned)={recomp_image_guard_v2,!strcmp(mode,"v1")?old_guard_v1:NULL};
        int ready=1;
        for(unsigned j=0;j<2;++j)if(!modules[j] || modules[j](0)!=2)ready=0;
        for(unsigned j=0;j<2;++j)if(modules[j])modules[j](ready?2:0);
    }
    if(!strcmp(mode,"unmapped")){c.host_mem=NULL;c.mem=NULL;}
    if(!strcmp(mode,"chain")){
        memory[28]^=0x20;c.pc=0x1018;recomp_image_lookup(c.pc)(&c);return 9;
    }
    recomp_image_lookup(c.pc)(&c);
    CHECK(c.x[1]==2 && c.x[3]==0);
    if(c.host_mem){
        CHECK(c.halted==RECOMP_HALT_IC_IVAU && c.pc==0x100c && c.pending_svc==0x10c0);
        c.halted=0;c.pending_svc=~UINT64_C(0);c.pc+=4;
    }else CHECK(c.halted==0 && c.pc==0x1010 && c.pending_svc==~UINT64_C(0));
    recomp_image_lookup(c.pc)(&c);
    CHECK(c.x[3]==3 && c.pc==0x1014 && c.halted==RECOMP_HALT_BREAKPOINT);
    puts("guard unchanged IC/ISB execution passed");return 0;
}
