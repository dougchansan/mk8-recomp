// Export only synthetic instruction words, never cartridge content.
#include <cstdio>
#include <string>
#include "core/recompiler/arm64_to_c.h"
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    for(uint32_t word:{0xd508751fu,0xd508711fu,0xd50b7500u}) {
        std::string out;bool miss=false;suyu::recomp::Translate(word,0x1000,out,&miss);
        if(!miss)return 1;
    }
    const uint32_t code[]={0xd2800020,0xd5033fdf,0xd2800041,0xd50b7522,0xd2800063,0xd4200000,
                           0x14000001,0xd2800084,0xd4200000};
    std::vector<uint64_t> roots={0x1018};
    auto stats=suyu::recomp::EmitProject("guard",reinterpret_cast<const uint8_t*>(code),sizeof code,
                                        0x1000,argv[1],false,nullptr,0,nullptr,0,0x1000,{},&roots);
    if(stats.unhandled || !suyu::recomp::IsTerminator(0xd5033fdf) ||
       !suyu::recomp::IsTerminator(0xd50b7522))return 1;
    std::printf("guard synthetic export: %zu blocks, %zu instructions\n",stats.blocks,stats.emitted);
}
