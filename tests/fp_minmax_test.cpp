// Synthetic encodings and independent bit-result cases for generated SIMD min/max C.
// Build C++20 with -I third_party/suyu/src; emit stdout, compile as C11, run.
#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"

struct Form { unsigned word, kind, width, lanes, op, rd, rn, rm; };
int main(int argc, char** argv) {
    // GNU objdump: reserved 1D vector/reduction encodings and unsupported FP16.
    for (unsigned word : {0x0e62f420u, 0x2e62f420u, 0x4e70f820u, 0x2e30f820u,
                          0x5e30f820u, 0x0e62c420u, 0x2e62c420u, 0x6e70f820u,
                          0x6e70c820u, 0x7e31f820u, 0xee22f420u}) {
        std::string out;
        bool miss = false;
        suyu::recomp::Translate(word, 0x1000, out, &miss);
        if (!miss) { std::fprintf(stderr, "reserved/unsupported minmax decoded %08x\n", word); return 1; }
    }
    std::vector<Form> forms;
    for (unsigned kind = 0; kind < 4; ++kind)
    for (unsigned arrangement = 0; arrangement < 3; ++arrangement)
    for (unsigned op = 0; op < 4; ++op)
    for (unsigned alias = 0; alias < 5; ++alias) {
        if ((kind == 2 && arrangement == 0) || (kind == 3 && arrangement != 1)) continue;
        const bool dbl = arrangement == 2;
        const unsigned rd = alias == 4 ? 31 : alias == 3 ? 1 : alias;
        const unsigned rn = alias == 4 ? 31 : 1;
        const unsigned rm = alias == 4 ? 31 : alias == 3 ? 1 : 2;
        unsigned word = kind < 2 ? 0x0e20f400 : kind == 2 ? 0x7e30f800 : 0x6e30f800;
        if (kind < 2) word |= (arrangement != 0 ? 1u << 30 : 0) | (kind << 29) | (rm << 16);
        word |= (dbl ? 1u << 22 : 0) | ((op & 1) << 23) | (rn << 5) | rd;
        if (op >= 2) word ^= 0x3000;
        forms.push_back({word, kind, dbl ? 8u : 4u, kind != 2 && arrangement == 1 ? 4u : 2u, op, rd, rn, rm});
    }
    if (argc == 2 && std::strcmp(argv[1], "--assembly") == 0) {
        std::puts(".text");
        for (const auto& f : forms) {
            const std::string mnemonic = std::string((f.op & 1) ? "fmin" : "fmax") +
                (f.op >= 2 ? "nm" : "") + (f.kind == 3 ? "v" : f.kind ? "p" : "");
            const std::string arrangement = std::to_string(f.lanes) + (f.width == 8 ? "d" : "s");
            std::printf(".inst 0x%08x\n%s ", f.word, mnemonic.c_str());
            if (f.kind >= 2) std::printf("%c%u, v%u.%s\n", f.width == 8 ? 'd' : 's', f.rd, f.rn, arrangement.c_str());
            else std::printf("v%u.%s, v%u.%s, v%u.%s\n", f.rd, arrangement.c_str(), f.rn,
                             arrangement.c_str(), f.rm, arrangement.c_str());
        }
        return 0;
    }
    std::puts(R"C(#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { uint64_t vreg[32][2]; uint64_t fpcr,fpsr,sentinel[4]; } GuestContext;
typedef void (*Fn)(GuestContext*);
)C");
    {
        std::string out;bool miss=false;
        suyu::recomp::Translate(0x4e22f420,0x1000,out,&miss);
        if(miss)return 1;
        std::printf("static void diagnostic_fmax(GuestContext* c) {\n%s}\n",out.c_str());
    }
    for (unsigned j = 0; j < forms.size(); ++j) {
        std::string out;
        bool miss = false;
        suyu::recomp::Translate(forms[j].word, 0x1000, out, &miss);
        if (miss) { std::fprintf(stderr, "minmax missing %08x\n", forms[j].word); return 1; }
        std::printf("static void f%u(GuestContext* c) {\n%s}\n", j, out.c_str());
    }
    std::puts("static struct { Fn fn; unsigned kind,width,lanes,op,rd,rn,rm; } forms[]={");
    for (unsigned j = 0; j < forms.size(); ++j) {
        const auto& f = forms[j];
        std::printf("{f%u,%u,%u,%u,%u,%u,%u,%u},\n", j, f.kind, f.width, f.lanes, f.op, f.rd, f.rn, f.rm);
    }
    std::puts(R"C(};
// Values are ordered by real value for indices 0..9. The two zeros are
// deliberately distinct. NaN priority is specified by the case table below.
static const uint64_t values[2][14]={
 {0xff800000,0xbf800000,0x807fffff,0x80000001,0x80000000,0,
  1,0x007fffff,0x3f800000,0x7f800000,0x7fc00123,0xffc00456,0x7f800123,0xff800456},
 {UINT64_C(0xfff0000000000000),UINT64_C(0xbff0000000000000),
  UINT64_C(0x800fffffffffffff),UINT64_C(0x8000000000000001),UINT64_C(0x8000000000000000),0,
  1,UINT64_C(0x000fffffffffffff),UINT64_C(0x3ff0000000000000),UINT64_C(0x7ff0000000000000),
  UINT64_C(0x7ff8000000000123),UINT64_C(0xfff8000000000456),
  UINT64_C(0x7ff0000000000123),UINT64_C(0xfff0000000000456)}
};
// Golden NaN cases: input IDs, max,min,maxnum,minnum output IDs, IOC.
// Outputs 10/11 are quiet versions of 12/13, respectively.
static const unsigned nan_cases[][7]={
 {10,8,10,10,8,8,0},{8,10,10,10,8,8,0},
 {11,8,11,11,8,8,0},{8,11,11,11,8,8,0},
 {12,8,10,10,10,10,1},{8,12,10,10,10,10,1},
 {13,8,11,11,11,11,1},{8,13,11,11,11,11,1},
 {10,11,10,10,10,10,0},{11,10,11,11,11,11,0},
 {10,12,10,10,10,10,1},{12,10,10,10,10,10,1},
 {10,13,11,11,11,11,1},{13,10,11,11,11,11,1},
 {11,12,10,10,10,10,1},{12,11,10,10,10,10,1},
 {12,13,10,10,10,10,1},{13,12,11,11,11,11,1},
 {10,10,10,10,10,10,0},{11,11,11,11,11,11,0},
 {12,12,10,10,10,10,1},{13,13,11,11,11,11,1},
 {11,13,11,11,11,11,1},{13,11,11,11,11,11,1}
};
static unsigned checks;
static uint64_t get(const GuestContext* c,unsigned reg,unsigned lane,unsigned width) {
 uint64_t r=0; memcpy(&r,(const unsigned char*)c->vreg[reg]+lane*width,width);return r;
}
static void set(GuestContext* c,unsigned reg,unsigned lane,unsigned width,uint64_t v) {
 memcpy((unsigned char*)c->vreg[reg]+lane*width,&v,width);
}
static int subnormal(unsigned id) { return id==2||id==3||id==6||id==7; }
static unsigned pair_ref(unsigned a,unsigned b,unsigned op,unsigned mode,uint64_t* flags) {
 if(mode&1) {
  if(subnormal(a)) {a=a<4?4:5;*flags|=128;}
  if(subnormal(b)) {b=b<4?4:5;*flags|=128;}
 }
 if(a<10&&b<10) return op&1 ? (a<b?a:b) : (a>b?a:b);
 // Replace every non-NaN with the table's finite placeholder, then restore
 // it if the numeric form selected it. This leaves NaN cases as fixed data.
 unsigned aa=a<10?8:a,bb=b<10?8:b;
 for(unsigned j=0;j<sizeof nan_cases/sizeof nan_cases[0];++j) {
  const unsigned* n=nan_cases[j];
  if(n[0]!=aa||n[1]!=bb) continue;
  *flags|=n[6];
  unsigned result=n[2+op];
  if(result==8) result=a<10?a:b;
  return result;
 }
 puts("missing oracle table case");return 99;
}
static uint64_t bits(unsigned id,unsigned width,unsigned mode) {
 if((mode&2)&&id>=10) return width==8?UINT64_C(0x7ff8000000000000):0x7fc00000;
 return values[width==8][id];
}
static int run(unsigned f,unsigned mode,const unsigned* n,const unsigned* m) {
 const unsigned width=forms[f].width,lanes=forms[f].lanes,op=forms[f].op;
 const unsigned rd=forms[f].rd,rn=forms[f].rn,rm=forms[f].rm,kind=forms[f].kind;
 GuestContext c,expected;
 memset(&c,0xa5,sizeof c);
 c.fpcr=((mode&1)?1ULL<<24:0)|((mode&2)?1ULL<<25:0)|((uint64_t)(mode>>2)<<22);
 c.fpsr=0x8000010;
 for(unsigned e=0;e<16/width;++e) set(&c,rn,e,width,values[width==8][n[e]]);
 if(rm!=rn&&kind<2) for(unsigned e=0;e<16/width;++e) set(&c,rm,e,width,values[width==8][m[e]]);
 expected=c;memset(expected.vreg[rd],0,16);
 unsigned result[4]={0};
 if(kind==3) {
  unsigned lo=pair_ref(n[0],n[1],op,mode,&expected.fpsr);
  unsigned hi=pair_ref(n[2],n[3],op,mode,&expected.fpsr);
  // DN transforms intermediate NaNs into a default quiet NaN. Track their
  // identity separately by choosing an explicit expected final bit value.
  result[0]=pair_ref(lo,hi,op,mode,&expected.fpsr);
 } else if(kind==2) result[0]=pair_ref(n[0],n[1],op,mode,&expected.fpsr);
 else for(unsigned e=0;e<lanes;++e) {
  const unsigned* src=e<lanes/2?n:(rn==rm?n:m);
  unsigned a=kind?src[2*(e%(lanes/2))]:n[e];
  unsigned b=kind?src[2*(e%(lanes/2))+1]:(rn==rm?n[e]:m[e]);
  result[e]=pair_ref(a,b,op,mode,&expected.fpsr);
 }
 for(unsigned e=0;e<(kind<2?lanes:1);++e) set(&expected,rd,e,width,bits(result[e],width,mode));
 forms[f].fn(&c);++checks;
 if(memcmp(&c,&expected,sizeof c)) {
  printf("FAIL f=%u kind=%u width=%u op=%u mode=%u n=%u,%u,%u,%u m=%u,%u,%u,%u got=%llx expected=%llx flags=%llx/%llx\n",
   f,kind,width,op,mode,n[0],n[1],n[2],n[3],m[0],m[1],m[2],m[3],
   (unsigned long long)get(&c,rd,0,width),(unsigned long long)get(&expected,rd,0,width),
   (unsigned long long)c.fpsr,(unsigned long long)expected.fpsr);return 1;
 }
 return 0;
}
int main(void) {
 {
  // Actual Dynarmic --fp-new sample32 differs only in IOC for this vector.
  // S lanes are [0,qNaN,0,qNaN], not double infinities. Arm FPProcessNaN
  // raises IOC for signaling NaNs only; keep the architectural expectation.
  GuestContext c={0},expected;
  c.vreg[1][0]=c.vreg[1][1]=c.vreg[2][0]=c.vreg[2][1]=UINT64_C(0x7ff0000000000000);
  expected=c;expected.vreg[0][0]=expected.vreg[0][1]=UINT64_C(0x7ff0000000000000);
  diagnostic_fmax(&c);++checks;
  if(memcmp(&c,&expected,sizeof c)){puts("FAIL exact qNaN FMAX architectural state");return 1;}
 }
 for(unsigned f=0;f<sizeof forms/sizeof forms[0];++f)
 for(unsigned mode=0;mode<16;++mode) {
  for(unsigned a=0;a<14;++a) for(unsigned b=0;b<14;++b) {
   unsigned n[]={a,b,a,b},m[]={b,a,b,a};
   if(run(f,mode,n,m))return 1;
  }
  // Distinct payloads in four lanes pin balanced reduction and pair ordering.
  for(unsigned a=10;a<14;++a) for(unsigned b=10;b<14;++b)
  for(unsigned c=10;c<14;++c) for(unsigned d=10;d<14;++d) {
   unsigned n[]={a,b,c,d},m[]={d,c,b,a};
   if(run(f,mode,n,m))return 1;
  }
 }
 printf("%u generated-C SIMD min/max checks passed\n",checks);
 return 0;
}
)C");
}
