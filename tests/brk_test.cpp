// BRK is a synchronous stop, never decoder fallback or fallthrough.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
int main(){
 using namespace suyu::recomp;
 if(std::string(RuntimeH()).find("#define RECOMP_HALT_BREAKPOINT 3")==std::string::npos){
  std::fprintf(stderr,"breakpoint runtime constant mismatch\n");return 1;
 }
 for(unsigned word:{0xd4200001u,0xd4200002u,0xd4200010u,0xd4000000u}){
  std::string out;bool miss=false;Translate(word,0x1000,out,&miss);
  if(!miss||IsTerminator(word)){std::fprintf(stderr,"reserved BRK mask %08x\n",word);return 1;}
 }
 for(unsigned imm=0;imm<65536;imm++){
  unsigned word=0xd4200000|(imm<<5);std::string out;bool miss=false;
  if(Translate(word,0x1000,out,&miss)||miss||!IsTerminator(word)||out.find("RECOMP_HALT_BREAKPOINT")==std::string::npos){std::fprintf(stderr,"BRK decode %u\n",imm);return 1;}
 }
 std::vector<u32> words={0xd503201f,0xd4224680,0xf9000020,0xd65f03c0};
 auto blocks=DiscoverBlocks(reinterpret_cast<const u8*>(words.data()),words.size()*4,0x1000,0);
 if(blocks.size()!=2||blocks[0].vaddr!=0x1000||blocks[0].count!=2||blocks[1].vaddr!=0x1008||blocks[1].count!=2){std::fprintf(stderr,"BRK discovery split failed\n");return 1;}
 std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n#define RECOMP_HALT_BREAKPOINT 3\nstatic uint64_t g_module_base;\ntypedef struct { uint64_t x[32],pc,pending_svc;int halted;uint64_t guard;} GuestContext;\nstatic unsigned stores;\nstatic void recomp_store64(GuestContext*c,uint64_t a,uint64_t v){(void)c;(void)a;(void)v;stores++;}");
 unsigned id=0;for(unsigned imm:{0u,1u,0x1234u,0xffffu}){
  std::string out;bool miss=false;Translate(0xd4200000|(imm<<5),0x1004,out,&miss);
  // Deliberately append translated memory write despite BRK's terminator result.
  Translate(0xf9000020,0x1008,out,&miss);
  std::printf("static void f%u(GuestContext*c){%s}\n",id++,out.c_str());
 }
 std::puts(R"C(
static void(*fn[])(GuestContext*)={f0,f1,f2,f3};
int main(void){unsigned checks=0;for(unsigned n=0;n<4;n++)for(unsigned base=0;base<3;base++)for(unsigned stale=0;stale<3;stale++){
 GuestContext a,e;memset(&a,0,sizeof a);for(unsigned r=0;r<32;r++)a.x[r]=0xfedcba9876543210ULL+r;
 a.pc=0x9999;a.pending_svc=stale==0?~0ULL:0x55;a.halted=(int)stale;a.guard=0xbaadfeedULL;e=a;
 g_module_base=base==0?0:base==1?0x7100000000ULL:0x8000000000000000ULL;
 e.pc=g_module_base+0x1004;e.halted=RECOMP_HALT_BREAKPOINT;stores=0;fn[n](&a);checks++;
 if(stores||memcmp(&a,&e,sizeof a)){printf("BRK execution failed %u %u %u\n",n,base,stale);return 1;}
 }printf("65536 BRK encodings, 4 negatives, discovery split, %u generated-C stop checks passed\n",checks);return 0;}
)C");
}
