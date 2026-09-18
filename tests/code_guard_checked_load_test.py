"""Extract the production checked HostLoad body for synthetic mapping tests."""
from pathlib import Path
import sys

source = (Path(sys.argv[1]) / "third_party/suyu/src/core/arm/recomp/arm_recomp.cpp").read_text()
start = source.index("    static u64 HostLoad(")
body = source[start:source.index("\n    }", start) + 6]
prefix = r'''
#include <cstdint>
#include <cstdio>
#include <cstring>
using u64=uint64_t;using u32=uint32_t;
struct Memory {
    unsigned char bytes[16]{},mapping[16]{};
    unsigned checks=0,reads=0;
    bool IsValidVirtualAddress(u64 va){++checks;return va>=0x1000 && va-0x1000<16 && mapping[va-0x1000]!=0;}
    u64 Read(u64 va,unsigned n){u64 result=0;std::memcpy(&result,bytes+va-0x1000,n);return result;}
    u64 Read8(u64 va){return Read(va,1);}u64 Read16(u64 va){return Read(va,2);}
    u32 Read32(u64 va){++reads;return static_cast<u32>(Read(va,4));}u64 Read64(u64 va){return Read(va,8);}
};
struct System {Memory memory;Memory& ApplicationMemory(){return memory;}};
struct Impl {System system;
'''
suffix = r'''
};
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d\n",__LINE__);return 1;}}while(0)
int main(){
    Impl host;auto& memory=host.system.memory;std::memset(memory.mapping,1,sizeof memory.mapping);
    CHECK(Impl::HostLoad(&host,0x1000,0)==(u64{1}<<32));CHECK(memory.checks==4 && memory.reads==1);
    const u32 word=0xdeadbeef;std::memcpy(memory.bytes,&word,4);
    CHECK(Impl::HostLoad(&host,0x1000,0)==((u64{1}<<32)|word));
    for(unsigned missing=0;missing<4;++missing){
        const auto reads=memory.reads;memory.mapping[missing]=0;
        CHECK(Impl::HostLoad(&host,0x1000,0)==0 && memory.reads==reads);memory.mapping[missing]=1;
    }
    const auto checks=memory.checks;
    CHECK(Impl::HostLoad(&host,~u64{0}-2,0)==0 && memory.checks==checks);
    // A mapped special page need not provide any direct-pointer fast path.
    std::memset(memory.mapping,2,4);std::memset(memory.bytes,0,4);
    CHECK(Impl::HostLoad(&host,0x1000,0)==(u64{1}<<32));
    CHECK(Impl::HostLoad(&host,0x1000,4)==0); // Ordinary loads keep their old ABI.
    std::puts("checked HostLoad zero/partial/special/overflow mapping checks passed");return 0;
}
'''
Path(sys.argv[2]).write_text(prefix + body + suffix)
