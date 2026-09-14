// Generator: compile with -I third_party/suyu/src, run > estimate-test.c.
// Compile the emitted C and pass vectors exported by fp_estimate_oracle.cpp.
// The same vectors work with GCC and MSVC; no host-FP oracle is used in C.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(int argc, char** argv) {
    const bool assembly=argc>1 && std::string(argv[1])=="--assembly";
    if(assembly) puts(".text");
    else puts("#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n"
         "typedef struct {uint64_t x[32],vreg[32][2],fpcr,fpsr;} GuestContext;");
    if(!assembly) puts(suyu::recomp::EstimateRuntimeC());
    for(unsigned op=0;op<2;op++)for(unsigned form=0;form<5;form++)
    for(unsigned alias=0;alias<3;alias++) {
        bool dbl=form==1||form==4,scalar=form<2;
        unsigned rn=alias==2?31:1,rd=alias==2?31:alias;
        unsigned word=(scalar?0x5ea1d800:0x4ea1d800)|(op<<29)|(dbl?0x400000:0)|(rn<<5)|rd;
        if(form==2)word&=~0x40000000u;
        if(assembly) {
            printf(".inst 0x%08x\n",word);
            if(scalar) printf("%s %c%u,%c%u\n",op?"frsqrte":"frecpe",dbl?'d':'s',rd,dbl?'d':'s',rn);
            else printf("%s v%u.%s,v%u.%s\n",op?"frsqrte":"frecpe",rd,
                        dbl?"2d":form==2?"2s":"4s",rn,dbl?"2d":form==2?"2s":"4s");
            continue;
        }
        std::string body;bool miss=false;suyu::recomp::Translate(word,0x1000,body,&miss);
        if(miss||body.find("recomp_fp_estimate(")==std::string::npos)return 2;
        printf("static void f%u%u%u(GuestContext*c){%s}\n",op,form,alias,body.c_str());
    }
    if(assembly)return 0;
    unsigned negatives=0;
    for(unsigned word:{0x0ee1d820u,0x2ee1d820u,0xcea1d820u,0xeea1d820u,
                       0x5ef9d820u,0x7ef9d820u,0x4ef9d820u,0x6ef9d820u}) {
        std::string body;bool miss=false;suyu::recomp::Translate(word,0x1000,body,&miss);
        if(!miss){negatives++;fprintf(stderr,"negative decoded: %08x\n",word);}
    }
    printf("static const unsigned negatives=%u;\n",negatives);
    puts("typedef void(*Fn)(GuestContext*);static Fn fn[2][5][3]={");
    for(unsigned op=0;op<2;op++){puts("{");for(unsigned form=0;form<5;form++)
        printf("{f%u%u0,f%u%u1,f%u%u2},",op,form,op,form,op,form);puts("},");}
    puts(R"C(};
int main(int argc,char**argv){
 if(argc!=2){puts("pass exported oracle vectors");return 2;}
 FILE*f=fopen(argv[1],"r");if(!f)return 2;
 unsigned width,op,fpcr,flags,checks=0,failures=negatives;
 unsigned long long bits,result;
 while(fscanf(f,"%u %u %x %llx %llx %x",&width,&op,&fpcr,&bits,&result,&flags)==6){
  for(unsigned form=0;form<5;form++)for(unsigned alias=0;alias<3;alias++){
   unsigned dbl=form==1||form==4;if((dbl?64u:32u)!=width)continue;
   unsigned rd=alias==2?31:alias,rn=alias==2?31:1,bytes=width/8;
   unsigned lanes=form<2?1:form==2?2:16/bytes;
   GuestContext actual,expected;memset(&actual,0xa5,sizeof actual);
   actual.fpcr=fpcr;actual.fpsr=0x08000000;
   for(unsigned j=0;j<lanes;j++){uint64_t in=j&1?(dbl?0x3ff0000000000000ULL:0x3f800000ULL):bits;
    memcpy((char*)actual.vreg[rn]+j*bytes,&in,bytes);}
   expected=actual;memset(expected.vreg[rd],0,16);expected.fpsr=flags;
   for(unsigned j=0;j<lanes;j++){uint64_t out=j&1?(dbl?0x3feff00000000000ULL:0x3f7f8000ULL):result;
    memcpy((char*)expected.vreg[rd]+j*bytes,&out,bytes);}
   fn[op][form][alias](&actual);checks++;
   if(memcmp(&actual,&expected,sizeof actual)&&failures++<8)
    printf("FAIL width=%u op=%u form=%u alias=%u input=%llx fpcr=%x\n",width,op,form,alias,bits,fpcr);
  }
 }
 fclose(f);printf("checks=%u failures=%u negatives=8\n",checks,failures);
 return !checks||failures;
})C");
}
