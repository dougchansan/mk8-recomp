#include <cstdio>
#include <string>
#include <vector>
#include "core/recompiler/arm64_to_c.h"
int main(int argc,char**argv) {
    std::vector<unsigned> words;
    // Register arithmetic, flag writes, comparisons, multiply/accumulate,
    // saturation, narrowing, and double-precision FP (including fused multiply).
    for(unsigned size=0;size<4;size++) {
        words.push_back(0x4e222c20u|(size<<22)); // SQSUB vector
        words.push_back(0x6e220c20u|(size<<22)); // UQADD vector
        words.push_back(0x5e207820u|(size<<22)); // SQABS scalar
        if(size<3) {
            words.push_back(0x4e229420u|(size<<22)); // MLA
            words.push_back(0x0e214820u|(size<<22)); // SQXTN
        }
    }
    for(unsigned word:{0xab020020u,0xeb020020u,0x9b020c20u,0x4e223420u,
                       0x1e622820u,0x1e623820u,0x1e620820u,0x1f420c20u,
                       0x1e221820u,0x1e621820u}) words.push_back(word);
    const std::pair<unsigned,const char*> extra_integer[]={
        {0x6e207820,"sqneg v0.16b,v1.16b"},{0x2e212820,"sqxtun v0.8b,v1.8h"},
        {0x6ea14820,"uqxtn2 v0.4s,v1.2d"},{0x4e62b420,"sqdmulh v0.8h,v1.8h,v2.8h"},
        {0x7ea2b420,"sqrdmulh s0,s1,s2"},{0x4f72c820,"sqdmulh v0.8h,v1.8h,v2.h[7]"},
        {0x4e224c20,"sqshl v0.16b,v1.16b,v2.16b"},{0x7ee24c20,"uqshl d0,d1,d2"}
    };
    for(auto [word,mnemonic]:extra_integer){(void)mnemonic;words.push_back(word);}
    const unsigned old_count=words.size();
    const std::pair<unsigned,const char*> added[]={
        {0x4e22f420,"fmax v0.4s,v1.4s,v2.4s"},{0x4ea2f420,"fmin v0.4s,v1.4s,v2.4s"},
        {0x4e62c420,"fmaxnm v0.2d,v1.2d,v2.2d"},{0x4ee2c420,"fminnm v0.2d,v1.2d,v2.2d"},
        {0x6e22f420,"fmaxp v0.4s,v1.4s,v2.4s"},{0x7e70f820,"fmaxp d0,v1.2d"},
        {0x6e30f820,"fmaxv s0,v1.4s"},{0x6eb0c820,"fminnmv s0,v1.4s"},
        {0x4e218820,"frintn v0.4s,v1.4s"},{0x4ea18820,"frintp v0.4s,v1.4s"},
        {0x4e219820,"frintm v0.4s,v1.4s"},{0x4ea19820,"frintz v0.4s,v1.4s"},
        {0x6e618820,"frinta v0.2d,v1.2d"},{0x6e619820,"frintx v0.2d,v1.2d"},
        {0x6ee19820,"frinti v0.2d,v1.2d"},
        {0x0e616820,"fcvtn v0.2s,v1.2d"},{0x4e616820,"fcvtn2 v0.4s,v1.2d"},
        {0x0e617820,"fcvtl v0.2d,v1.2s"},{0x4e617820,"fcvtl2 v0.2d,v1.4s"},
        {0x5e61a820,"fcvtns d0,d1"},{0x7e21c820,"fcvtau s0,s1"},
        {0x4e21a820,"fcvtns v0.4s,v1.4s"},{0x6e61c820,"fcvtau v0.2d,v1.2d"},
        {0x6ee1f820,"fsqrt v0.2d,v1.2d"},{0x6ee2d420,"fabd v0.2d,v1.2d,v2.2d"},
        {0x4e22d420,"fadd v0.4s,v1.4s,v2.4s"},{0x4ee2d420,"fsub v0.2d,v1.2d,v2.2d"},
        {0x7e70d820,"faddp d0,v1.2d"},
        {0x6e22dc20,"fmul v0.4s,v1.4s,v2.4s"},{0x4e22cc20,"fmla v0.4s,v1.4s,v2.4s"},
        {0x4ea2cc20,"fmls v0.4s,v1.4s,v2.4s"},{0x4f829020,"fmul v0.4s,v1.4s,v2.s[0]"},
        {0x4f821020,"fmla v0.4s,v1.4s,v2.s[0]"},{0x4f825020,"fmls v0.4s,v1.4s,v2.s[0]"},
        {0x1e221820,"fdiv s0,s1,s2"},{0x1e621820,"fdiv d0,d1,d2"},
        {0x6e22fc20,"fdiv v0.4s,v1.4s,v2.4s"},{0x6e62fc20,"fdiv v0.2d,v1.2d,v2.2d"},
        {0x6ea1d820,"frsqrte v0.4s,v1.4s"},{0x6ee1d820,"frsqrte v0.2d,v1.2d"},
        {0x7ea1d820,"frsqrte s0,s1"},{0x7ee1d820,"frsqrte d0,d1"},
        {0x4ea1d820,"frecpe v0.4s,v1.4s"},{0x4ee1d820,"frecpe v0.2d,v1.2d"},
        {0x5ea1d820,"frecpe s0,s1"},{0x5ee1d820,"frecpe d0,d1"}
    };
    if(argc==2&&std::string(argv[1])=="--assembly") {
        std::puts(".text");for(auto [word,mnemonic]:extra_integer)std::printf(".inst 0x%08x\n%s\n",word,mnemonic);
        for(auto [word,mnemonic]:added)std::printf(".inst 0x%08x\n%s\n",word,mnemonic);
        return 0;
    }
    for(auto [word,mnemonic]:added){(void)mnemonic;words.push_back(word);}
    std::puts("#include <math.h>\n#include <fenv.h>\n#include <float.h>\n#include <string.h>\n#include \"differential_state.h\"");
    for(unsigned j=0;j<words.size();j++) {
        std::string out;bool miss=false;
        suyu::recomp::Translate(words[j],0x1000,out,&miss);
        if(miss){std::fprintf(stderr,"untranslated synthetic word %08x\n",words[j]);return 1;}
        std::printf("static void f%u(GuestContext*c){%s}\n",j,out.c_str());
    }
    std::puts("static void(*const functions[])(GuestContext*)={");
    for(unsigned j=0;j<words.size();j++)std::printf("f%u,\n",j);
    std::puts("};\nstatic const uint32_t words[]={");
    for(auto w:words)std::printf("0x%08xu,\n",w);
    std::printf("};\nunsigned differential_is_fp(unsigned i){return i>=%u?2:(words[i]&0x5e000000u)==0x1e000000u;}\n",old_count);
    std::puts("unsigned differential_count(void){return sizeof(words)/sizeof(words[0]);}\n"
              "uint32_t differential_word(unsigned i){return words[i];}\n"
              "void differential_emit(unsigned i,GuestContext*c){functions[i](c);}");
}
