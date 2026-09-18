// Load the synthetic generated DLL and execute from the middle of one emitted
// block. This proves lookup and the side-entry prologue agree at runtime.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct GuestContext {
    std::uint64_t x[32];
    std::uint64_t pc;
    std::uint8_t n, z, c, v;
    std::uint8_t* mem;
    std::uint64_t mem_size;
    std::uint64_t mem_base_vaddr;
    int halted;
    std::uint64_t pending_svc;
    std::uint64_t vreg[32][2];
    std::uint64_t tpidr_el0;
    const void* host_mem;
    std::uint64_t tpidrro_el0;
    std::uint64_t fpcr;
    std::uint64_t fpsr;
    int chain_budget;
};

using BlockFn = void (*)(GuestContext*);
using LookupFn = BlockFn (*)(std::uint64_t);
using SetBaseFn = void (*)(std::uint64_t);
using RunSliceFn = void (*)(GuestContext*);
using GuardFn = unsigned (*)(unsigned);
using AbiFn = unsigned (*)();

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) return 2;
    SetErrorMode(SEM_NOGPFAULTERRORBOX);
    const auto dll_path = std::filesystem::path(argv[1]);
    HMODULE dll = LoadLibraryW(dll_path.c_str());
    if (!dll) {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    const auto lookup = reinterpret_cast<LookupFn>(GetProcAddress(dll, "recomp_image_lookup"));
    const auto set_base = reinterpret_cast<SetBaseFn>(GetProcAddress(dll, "recomp_image_set_base"));
    const auto run_slice =
        reinterpret_cast<RunSliceFn>(GetProcAddress(dll, "recomp_image_run_slice"));
    const auto guard = reinterpret_cast<GuardFn>(GetProcAddress(dll, "recomp_image_guard_v2"));
    const auto image_abi = reinterpret_cast<AbiFn>(GetProcAddress(dll, "recomp_image_abi"));
    if (!lookup || !set_base || !run_slice || !guard || !image_abi || image_abi() != 4) {
        std::fputs("generated image does not export side-entry ABI 4\n", stderr);
        return 1;
    }
    // Before set_base builds the flat index, the interval-table fallback must
    // already resolve interior entries.
    BlockFn first = lookup(0x1000);
    if (!first || lookup(0x1004) != first || lookup(0x1008) != first ||
        lookup(0x100c) != first || lookup(0x1002) != nullptr || lookup(0x1010) != nullptr) {
        std::fputs("lookup did not expose exactly the aligned side entries\n", stderr);
        return 1;
    }
    set_base(0);
    guard(0);
    if (lookup(0x1020) == nullptr || lookup(0x1024) != lookup(0x1020) ||
        lookup(0x1028) != nullptr) {
        std::fputs("flat index lost the final block's interior/high bound\n", stderr);
        return 1;
    }

    std::uint32_t code[]{0xD2800020, 0xFFFFFFFF, 0xD503201F, 0xD65F03C0, 0x00000000,
                         0x91000400, 0x14000001, 0x17FFFFFE, 0xD503201F, 0xD65F03C0};
    GuestContext context{};
    context.x[0] = 0xA5A5A5A5A5A5A5A5ULL;
    context.x[30] = 0x2000;
    context.pc = 0x1008;
    context.mem = reinterpret_cast<std::uint8_t*>(code);
    context.mem_size = sizeof(code);
    context.mem_base_vaddr = 0x1000;
    context.pending_svc = ~std::uint64_t{0};
    context.chain_budget = 32;
    if (argc == 3 && std::strcmp(argv[2], "--mutate") == 0) {
        code[2] ^= 1;
        lookup(context.pc)(&context);
        std::fputs("mutated side entry bypassed the code guard\n", stderr);
        return 86;
    }
    run_slice(&context);
    const bool ok = context.x[0] == 0xA5A5A5A5A5A5A5A5ULL && context.pc == 0x2000 &&
                    context.halted == 0;
    if (!ok) {
        std::fprintf(stderr, "side entry executed the wrong prefix: x0=%llx pc=%llx halted=%d\n",
                     static_cast<unsigned long long>(context.x[0]),
                     static_cast<unsigned long long>(context.pc), context.halted);
        return 1;
    }

    // Enter the ADD/B block at its branch. The bounded module-local slice must
    // follow both branches, return to the ADD root, and stop at its budget.
    std::memset(&context, 0, sizeof(context));
    context.pc = 0x1018;
    context.mem = reinterpret_cast<std::uint8_t*>(code);
    context.mem_size = sizeof(code);
    context.mem_base_vaddr = 0x1000;
    context.pending_svc = ~std::uint64_t{0};
    context.chain_budget = 3;
    run_slice(&context);
    if (context.x[0] != 1 || context.pc != 0x101c || context.halted != 0) {
        std::fprintf(stderr, "module slice reused a stale side entry: x0=%llu pc=%llx halted=%d\n",
                     static_cast<unsigned long long>(context.x[0]),
                     static_cast<unsigned long long>(context.pc), context.halted);
        return 1;
    }
    FreeLibrary(dll);
    std::puts("interior lookup, interval fallback, high bound, and bounded slice passed");
}
