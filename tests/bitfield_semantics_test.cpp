// Emit runnable C for SBFM boundary cases; compile generated C with shift warnings fatal.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"

int main() {
    const unsigned words[] = {0x93607ED6u, 0x9340FC20u};
    std::puts("#include <stdint.h>\n#include <stdio.h>\ntypedef struct { uint64_t x[32]; } GuestContext;");
    for (unsigned index = 0; index < 2; ++index) {
        std::string output;
        bool miss = false;
        suyu::recomp::Translate(words[index], 0x1000, output, &miss);
        if (miss) return 1;
        std::printf("static void f%u(GuestContext*c){%s}\n", index, output.c_str());
    }
    std::puts(R"C(int main(void) {
 const uint64_t values[]={0,1,UINT64_C(0x7fffffff),UINT64_C(0x80000000),
  UINT64_C(0xffffffff),UINT64_C(0x0123456789abcdef),UINT64_MAX};
 unsigned checks=0;
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i){
  GuestContext c={{0}};c.x[22]=values[i];f0(&c);++checks;
  if(c.x[22] != ((values[i]&UINT64_C(0xffffffff))<<32)) return 1;
  c.x[1]=values[i];f1(&c);++checks;if(c.x[0]!=values[i])return 1;
 }
 printf("%u/14 SBFM semantic checks passed\n",checks);return 0;
})C");
}
