"""Compile the actual callback/cache method bodies against synthetic memory.

Usage: python code_guard_cache_test.py <repo> <output.cpp>
The emitted test uses no process, cartridge, or Dynarmic runtime; full builds
separately check these methods against their production types.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]) / "third_party/suyu/src/core/arm/dynarmic"
source = (root / "arm_dynarmic_64.cpp").read_text()
header = (root / "arm_dynarmic_64.h").read_text()
assert "std::atomic<bool> code_read_dirty{false};" in header
names = ["std::optional<u32> DynarmicCallbacks64::MemoryReadCode",
         "void DynarmicCallbacks64::InstructionCacheOperationRaised",
         "void ArmDynarmic64::ClearInstructionCache",
         "void ArmDynarmic64::InvalidateCacheRange"]
bodies = []
for name in names:
    start = source.index(name)
    bodies.append(source[start:source.index("\n}", start) + 2])

prefix = r'''
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <thread>
#define LOG_CRITICAL(cat,msg,...) std::fprintf(stderr,"%s\n",msg)
#define LOG_DEBUG(...) ((void)0)
using u64=uint64_t;using u32=uint32_t;
namespace Dynarmic { enum class HaltReason{CacheInvalidation};namespace A64 {
enum class InstructionCacheOperation {InvalidateByVAToPoU,InvalidateAllToPoU,InvalidateAllToPoUInnerSharable};}}
namespace Core {
namespace Memory { constexpr u64 YUZU_PAGEMASK=4095; }
static bool active=false,ready=false;
void* GetRecompLookup(){return active?&active:nullptr;} bool IsRecompCodeGuardReady(){return ready;}
namespace Hardware {constexpr unsigned NUM_CPU_CORES=4;}
struct FakeMemory {
    u32 words[1024]{};unsigned reads=0;
    bool IsValidVirtualAddressRange(u64 a,size_t n){return a<4096 && n<=4096-a;}
    void ReadBlock(u64 a,void* p,size_t n){std::memcpy(p,reinterpret_cast<char*>(words)+a,n);++reads;}
};
struct FakeJit {unsigned invalidations=0,clears=0,halts=0;u64 address=0;size_t size=0;
    void ClearCache(){++clears;}void InvalidateCacheRange(u64 a,size_t n){++invalidations;address=a;size=n;}
    void HaltExecution(Dynarmic::HaltReason){++halts;}
};
struct DynarmicCallbacks64;
struct ArmDynarmic64 {DynarmicCallbacks64* m_cb;FakeJit* m_jit;
    void ClearInstructionCache();void InvalidateCacheRange(u64,std::size_t);
};
struct FakeProcess {ArmDynarmic64* cpu;unsigned seen=0;
    ArmDynarmic64* GetArmInterface(size_t core){seen|=1u<<core;return core==0?cpu:nullptr;}
};
struct DynarmicCallbacks64 {
    struct {u32 inst[1024];} cached_code_page;
    std::atomic<bool> code_read_dirty{false};u64 last_code_addr=u64(-1);
    ArmDynarmic64& m_parent;FakeMemory& m_memory;
    FakeProcess* m_process;
    std::optional<u32> MemoryReadCode(u64);
    void InstructionCacheOperationRaised(Dynarmic::A64::InstructionCacheOperation,u64);
};
'''
suffix = r'''
}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char**argv){
#ifdef _WIN32
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    using namespace Core;
    FakeMemory memory;FakeJit jit;ArmDynarmic64 host{nullptr,&jit};FakeProcess process{&host};
    DynarmicCallbacks64 cb{{},{false},u64(-1),host,memory,&process};host.m_cb=&cb;
    memory.words[0]=11;CHECK(cb.MemoryReadCode(0)==11);CHECK(memory.reads==1);
    std::thread writer([&]{memory.words[0]=22;host.InvalidateCacheRange(0,64);});writer.join();
    CHECK(cb.last_code_addr==0);CHECK(cb.MemoryReadCode(0)==22);CHECK(memory.reads==2);
    memory.words[0]=33;CHECK(cb.MemoryReadCode(0)==22);CHECK(memory.reads==2);
    host.ClearInstructionCache();CHECK(cb.last_code_addr==0);CHECK(cb.MemoryReadCode(0)==33);CHECK(memory.reads==3);
    CHECK(!cb.MemoryReadCode(4096));
    active=argc>1;ready=argc>1 && std::strcmp(argv[1],"legacy");
    cb.InstructionCacheOperationRaised(Dynarmic::A64::InstructionCacheOperation::InvalidateByVAToPoU,127);
    CHECK(jit.address==64 && jit.size==64 && jit.halts==1);
    CHECK(process.seen==(active?15u:0u));
    cb.InstructionCacheOperationRaised(Dynarmic::A64::InstructionCacheOperation::InvalidateAllToPoU,0);
    CHECK(jit.clears==2 && jit.halts==2);
    std::puts("actual callback cache invalidation and IC policy passed");return 0;
}
'''
Path(sys.argv[2]).write_text(prefix + "\n".join(bodies) + suffix)
