// Shifted-register operands: the 32-bit forms must never emit a shift count
// that is out of range for a 32-bit operand.
//
// The 32-bit encodings of "logical (shifted register)" and "add/subtract
// (shifted register)" are only allocated for imm6 < 32 - the ARM manual spells
// it "if sf == '0' && imm6<5> == '1' then UNDEFINED". The emitter used to
// translate them anyway: the operand was narrowed to uint32_t and then shifted
// by an amount up to 63, and the ROR complement was computed as 32 - imm6,
// which underflows to ~4.29e9. Both shift counts are undefined behaviour in C
// and MSVC rejects them outright (C4293), which is how this surfaced: Mario
// Kart 8 Deluxe emitted 4,669 of them and the export would not compile.
//
// They reach the decoder at all because the ARM64 pass speculatively walks
// ARM32 regions - 0x0AFFFFF0, checked below, is an ARM32 "beq" that happens to
// land in the logical-shifted-register space as BIC w16, wzr, wzr, ror #63.
//
// This test has two halves. First, the unallocated encodings must be reported
// as unhandled rather than translated. Second, the legal forms must still
// compute the right answer - a fix that merely silenced the warning by masking
// the shift count would pass the first half and fail the second.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"

namespace {

struct Legal {
    unsigned word;
    const char* comment;
};

// Translated and executed. Every one has imm6 < 32 in the 32-bit forms.
const Legal kLegal[] = {
    {0x2AC123E0u, "orr w0, wzr, w1, ror #8   -> ror w0, w1, #8"},
    {0xAAC123E0u, "orr x0, xzr, x1, ror #8   -> ror x0, x1, #8"},
    {0xAAC1FFE0u, "orr x0, xzr, x1, ror #63  -> the 64-bit edge"},
    {0x2A417FE0u, "orr w0, wzr, w1, lsr #31  -> the 32-bit edge"},
    {0x2A817FE0u, "orr w0, wzr, w1, asr #31  -> sign fill"},
    {0x0AE12040u, "bic w0, w2, w1, ror #8    -> the MK8 shape, legal amount"},
    {0x0B020C20u, "add w0, w1, w2, lsl #3"},
};

// Must NOT be translated: sf == 0 with imm6 >= 32.
const Legal kUnallocated[] = {
    {0x0AFFFFF0u, "bic w16, wzr, wzr, ror #63 - the exact word from MK8"},
    {0x2A41ABE0u, "orr w0, wzr, w1, lsr #42"},
    {0x2A81ABE0u, "orr w0, wzr, w1, asr #42"},
    {0x2A01ABE0u, "orr w0, wzr, w1, lsl #42"},
    {0x0B028020u, "add w0, w1, w2, lsl #32"},
    {0x4B02FC20u, "sub w0, w1, w2, lsl #63"},
};

}  // namespace

int main() {
    // An unallocated encoding that still produces a translation is the bug,
    // whatever text it produced: report it here rather than letting the
    // generated C decide.
    for (const Legal& bad : kUnallocated) {
        std::string output;
        bool unhandled = false;
        suyu::recomp::Translate(bad.word, 0x1000, output, &unhandled);
        if (!unhandled) {
            std::fprintf(stderr,
                         "FAIL: 0x%08X (%s) was translated; the 32-bit shifted-register\n"
                         "      forms are unallocated for imm6 >= 32.\n"
                         "      emitted: %s\n",
                         bad.word, bad.comment, output.c_str());
            return 1;
        }
    }

    std::puts("#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>");
    std::puts("typedef struct { uint64_t x[32]; uint8_t n,z,c_,v; } GuestContext;");
    // The emitter names the context pointer `c` and writes flags through it;
    // the struct above only needs the fields these instructions touch.
    std::puts("#define c_ c_");
    for (unsigned index = 0; index < sizeof(kLegal) / sizeof(kLegal[0]); ++index) {
        std::string output;
        bool unhandled = false;
        suyu::recomp::Translate(kLegal[index].word, 0x1000, output, &unhandled);
        if (unhandled) {
            std::fprintf(stderr, "FAIL: 0x%08X (%s) is legal but was not translated\n",
                         kLegal[index].word, kLegal[index].comment);
            return 1;
        }
        std::printf("/* %s */\nstatic void f%u(GuestContext*c){%s}\n", kLegal[index].comment, index,
                    output.c_str());
    }

    std::puts(R"C(
static uint32_t ror32(uint32_t v,unsigned n){return n?(uint32_t)((v>>n)|(v<<(32-n))):v;}
static uint64_t ror64(uint64_t v,unsigned n){return n?((v>>n)|(v<<(64-n))):v;}
static unsigned checks=0,failures=0;
static void expect(const char*what,uint64_t got,uint64_t want){
 ++checks;
 if(got!=want){++failures;
  printf("FAIL %s: got 0x%016llx want 0x%016llx\n",what,
         (unsigned long long)got,(unsigned long long)want);}}
int main(void){
 const uint64_t values[]={0,1,UINT64_C(0x7fffffff),UINT64_C(0x80000000),
  UINT64_C(0xffffffff),UINT64_C(0x0123456789abcdef),UINT64_C(0xdeadbeef00000001),
  UINT64_MAX};
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i){
  const uint64_t v=values[i];
  const uint32_t w=(uint32_t)v;
  GuestContext c;
  #define RUN(fn) do{ for(unsigned r=0;r<32;++r)c.x[r]=0; c.n=c.z=c.c_=c.v=0; \
                      c.x[1]=v; c.x[2]=v^UINT64_C(0xa5a5a5a5a5a5a5a5); fn(&c); }while(0)
  RUN(f0); expect("ror w0,w1,#8",c.x[0],(uint64_t)ror32(w,8));
  RUN(f1); expect("ror x0,x1,#8",c.x[0],ror64(v,8));
  RUN(f2); expect("ror x0,x1,#63",c.x[0],ror64(v,63));
  RUN(f3); expect("lsr w1,#31",c.x[0],(uint64_t)(w>>31));
  RUN(f4); expect("asr w1,#31",c.x[0],(uint64_t)(uint32_t)((int32_t)w>>31));
  RUN(f5); expect("bic w0,w2,w1,ror #8",c.x[0],
                  (uint64_t)((uint32_t)c.x[2] & ~ror32(w,8)));
  RUN(f6); expect("add w0,w1,w2,lsl #3",c.x[0],
                  (uint64_t)(uint32_t)(w+((uint32_t)c.x[2]<<3)));
 }
 printf("%u/%u shifted-register checks passed\n",checks-failures,checks);
 return failures?1:0;
})C");
    return 0;
}
