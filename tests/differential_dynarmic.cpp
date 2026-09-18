#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>
#include "dynarmic/interface/A64/a64.h"
#include "dynarmic/interface/exclusive_monitor.h"
#include "differential_state.h"

namespace {

// One shared region, seeded identically for both engines. Every memory case is
// aimed into it and any access that leaves it is a harness failure rather than
// a result: the two sides would then be agreeing about a fault instead of about
// a value, which is the way a memory differential passes without testing
// anything. The base is low so the page table the recompiler walks stays small.
constexpr uint64_t kRegionBase = 0x100000;
constexpr uint64_t kRegionSize = 0x10000;   // 16 pages of 4 KiB
constexpr uint64_t kPageBits = 12;
constexpr uint64_t kPageSize = uint64_t(1) << kPageBits;

alignas(16) uint8_t master_image[kRegionSize];
alignas(16) uint8_t oracle_image[kRegionSize];
alignas(16) uint8_t emitted_image[kRegionSize];

struct AccessLog {
    uint64_t count = 0;
    uint64_t page_crossing = 0;
    bool out_of_region = false;
    uint64_t bad_address = 0;
    uint32_t bad_size = 0;
};
AccessLog access_log;

bool InRegion(uint64_t va, uint32_t size) {
    return va >= kRegionBase && va - kRegionBase <= kRegionSize - size;
}

void NoteAccess(uint64_t va, uint32_t size) {
    access_log.count++;
    if ((va & (kPageSize - 1)) + size > kPageSize) access_log.page_crossing++;
    if (!InRegion(va, size)) {
        access_log.out_of_region = true;
        access_log.bad_address = va;
        access_log.bad_size = size;
    }
}

struct Callbacks final : Dynarmic::A64::UserCallbacks {
    uint32_t word[3] = {0, 0, 0};
    unsigned length = 1;
    bool memory_enabled = false;
    bool unsupported = false;

    std::optional<uint32_t> MemoryReadCode(uint64_t va) override {
        if (va < 0x1000 || va >= 0x1000 + 4ULL * length) return std::nullopt;
        return word[(va - 0x1000) / 4];
    }

    template <class T>
    T read(uint64_t va) {
        if (!memory_enabled) { unsupported = true; return {}; }
        NoteAccess(va, sizeof(T));
        T v{};
        if (InRegion(va, sizeof(T))) std::memcpy(&v, oracle_image + (va - kRegionBase), sizeof(T));
        return v;
    }
    template <class T>
    void write(uint64_t va, T value) {
        if (!memory_enabled) { unsupported = true; return; }
        NoteAccess(va, sizeof(T));
        if (InRegion(va, sizeof(T))) std::memcpy(oracle_image + (va - kRegionBase), &value, sizeof(T));
    }

    uint8_t MemoryRead8(uint64_t a) override { return read<uint8_t>(a); }
    uint16_t MemoryRead16(uint64_t a) override { return read<uint16_t>(a); }
    uint32_t MemoryRead32(uint64_t a) override { return read<uint32_t>(a); }
    uint64_t MemoryRead64(uint64_t a) override { return read<uint64_t>(a); }
    Dynarmic::A64::Vector MemoryRead128(uint64_t a) override {
        return read<Dynarmic::A64::Vector>(a);
    }
    void MemoryWrite8(uint64_t a, uint8_t v) override { write<uint8_t>(a, v); }
    void MemoryWrite16(uint64_t a, uint16_t v) override { write<uint16_t>(a, v); }
    void MemoryWrite32(uint64_t a, uint32_t v) override { write<uint32_t>(a, v); }
    void MemoryWrite64(uint64_t a, uint64_t v) override { write<uint64_t>(a, v); }
    void MemoryWrite128(uint64_t a, Dynarmic::A64::Vector v) override {
        write<Dynarmic::A64::Vector>(a, v);
    }
    // Store-exclusive. Whether the reservation is still valid is decided by
    // Dynarmic's own ExclusiveMonitor, which hands us the value it captured at
    // the load-exclusive; all that happens here is the compare-and-swap against
    // the shared image, the same thing Core::Memory::WriteExclusive does for
    // the emulator. Leaving these at their defaults makes every store-exclusive
    // fail, which looks exactly like a recompiler bug and is not one.
    template <class T>
    bool write_exclusive(uint64_t va, T value, T expected) {
        if (!memory_enabled) { unsupported = true; return false; }
        NoteAccess(va, sizeof(T));
        if (!InRegion(va, sizeof(T))) return false;
        T current{};
        std::memcpy(&current, oracle_image + (va - kRegionBase), sizeof(T));
        if (std::memcmp(&current, &expected, sizeof(T)) != 0) return false;
        std::memcpy(oracle_image + (va - kRegionBase), &value, sizeof(T));
        return true;
    }
    bool MemoryWriteExclusive8(uint64_t a, uint8_t v, uint8_t e) override {
        return write_exclusive<uint8_t>(a, v, e);
    }
    bool MemoryWriteExclusive16(uint64_t a, uint16_t v, uint16_t e) override {
        return write_exclusive<uint16_t>(a, v, e);
    }
    bool MemoryWriteExclusive32(uint64_t a, uint32_t v, uint32_t e) override {
        return write_exclusive<uint32_t>(a, v, e);
    }
    bool MemoryWriteExclusive64(uint64_t a, uint64_t v, uint64_t e) override {
        return write_exclusive<uint64_t>(a, v, e);
    }
    bool MemoryWriteExclusive128(uint64_t a, Dynarmic::A64::Vector v,
                                 Dynarmic::A64::Vector e) override {
        return write_exclusive<Dynarmic::A64::Vector>(a, v, e);
    }

    void CallSVC(uint32_t) override { unsupported = true; }
    void ExceptionRaised(uint64_t, Dynarmic::A64::Exception) override { unsupported = true; }
    void AddTicks(uint64_t) override {}
    uint64_t GetTicksRemaining() override { return 1; }
    uint64_t GetCNTPCT() override { return 0; }
};

uint64_t random_state;
uint64_t next() {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}
uint32_t nzcv(const GuestContext& c) {
    return (uint32_t(c.n) << 31) | (uint32_t(c.z) << 30) | (uint32_t(c.c) << 29) |
           (uint32_t(c.v) << 28);
}

// Register-offset addressing, so the harness can aim the index register. The
// emitter is the thing under test, so the extend is read straight out of the
// encoding rather than taken from anything the emitter produced.
bool IsRegisterOffset(uint32_t w) {
    return ((w >> 27) & 7) == 7 && ((w >> 24) & 3) == 0 && ((w >> 21) & 1) == 1 &&
           ((w >> 10) & 3) == 2;
}
unsigned ExtendOption(uint32_t w) { return (w >> 13) & 7; }

// The host bridge for the page-table backend. Entries are absolute, matching
// Memory::GetPointerImpl: recomp_host_ptr adds the full virtual address to the
// stored pointer, so one constant entry serves every page of the region.
std::vector<uintptr_t> page_entries;
RecompHostMem host_bridge;

uint64_t BridgeLoad(void*, uint64_t va, uint32_t size) {
    NoteAccess(va, size);
    uint64_t v = 0;
    if (InRegion(va, size)) std::memcpy(&v, emitted_image + (va - kRegionBase), size);
    return v;
}
void BridgeStore(void*, uint64_t va, uint32_t size, uint64_t value) {
    NoteAccess(va, size);
    if (InRegion(va, size)) std::memcpy(emitted_image + (va - kRegionBase), &value, size);
}

void BuildHostBridge() {
    page_entries.assign((size_t)((kRegionBase + kRegionSize) >> kPageBits) + 1, 0);
    const uintptr_t base = (uintptr_t)emitted_image - (uintptr_t)kRegionBase;
    for (uint64_t va = kRegionBase; va < kRegionBase + kRegionSize; va += kPageSize)
        page_entries[(size_t)(va >> kPageBits)] = base;
    host_bridge = RecompHostMem{};
    host_bridge.load = BridgeLoad;
    host_bridge.store = BridgeStore;
    host_bridge.page_entries = page_entries.data();
    host_bridge.page_entry_stride = sizeof(uintptr_t);
    host_bridge.page_bits = kPageBits;
    host_bridge.pointer_mask = ~uint64_t(0);
    host_bridge.address_space_max = kRegionBase + kRegionSize;
}

}  // namespace

int main(int argc, char** argv) {
    uint64_t seed = UINT64_C(0xc7615a4bc0928ed1);
    unsigned samples = 256, group = 0;
    bool inject = false, inject_memory = false;
    uint32_t only_word = 0;
    for (int j = 1; j < argc; j++) {
        std::string_view a(argv[j]);
        if (a == "--fp") group = 1;
        else if (a == "--fp-new") group = 2;
        else if (a == "--memory") group = 3;
        else if (a == "--memory-seq") group = 4;
        else if (a == "--word" && j + 1 < argc) only_word = (uint32_t)std::strtoul(argv[++j], nullptr, 0);
        else if (a == "--inject-mismatch") inject = true;
        else if (a == "--inject-memory-mismatch") inject_memory = true;
        else if (a == "--seed" && j + 1 < argc) seed = std::strtoull(argv[++j], nullptr, 0);
        else if (a == "--samples" && j + 1 < argc) samples = (unsigned)std::strtoul(argv[++j], nullptr, 0);
        else { std::fprintf(stderr, "unknown/incomplete argument %s\n", argv[j]); return 2; }
    }
    if (!samples || !seed) { std::fputs("seed and samples must be nonzero\n", stderr); return 2; }
    const bool memory_group = group >= 3;

    Callbacks cb;
    Dynarmic::A64::UserConfig config{};
    config.callbacks = &cb;
    // A real monitor, not a hand-written one. The point of the exclusive cases
    // is to compare the recompiler's reservation logic against an independent
    // implementation; supplying our own MemoryWriteExclusive callbacks would
    // have compared it against this file's idea of the architecture.
    Dynarmic::ExclusiveMonitor monitor(1);
    config.global_monitor = &monitor;
    config.processor_id = 0;
    Dynarmic::A64::Jit jit(config);
    BuildHostBridge();

    unsigned checks = 0, cases_run = 0;
    uint64_t total_accesses = 0, total_page_crossing = 0;
    // Every sequence case ends in a store-exclusive, and its status register is
    // the result most worth comparing. Counting both outcomes is what stops a
    // green run that only ever reached the failure path: if the reservation
    // never held, every case would agree on 1 and report a pass having tested
    // nothing about success.
    uint64_t stxr_success = 0, stxr_failure = 0;

    for (unsigned op = 0; op < differential_count(); op++) {
        if (differential_is_fp(op) != group ||
            (only_word && differential_word(op) != only_word))
            continue;
        const unsigned length = memory_group ? differential_length(op) : 1;
        for (unsigned k = 0; k < length; k++) cb.word[k] = differential_word_at(op, k);
        cb.length = length;
        cb.memory_enabled = memory_group;
        cases_run++;
        const uint64_t accesses_before = access_log.count;

        for (unsigned sample = 0; sample < samples; sample++) {
            // Each case has an independently reproducible seed, unaffected by skips.
            const uint64_t case_seed = seed ^ (uint64_t(cb.word[0]) << 17) ^ sample;
            random_state = case_seed;
            GuestContext c{};
            for (auto& x : c.x) x = next();
            for (auto& r : c.vreg) for (auto& x : r) x = next();
            const uint64_t edges[] = {0, UINT64_MAX, UINT64_C(0x8000800080008000),
                                      UINT64_C(0x7fff7fff7fff7fff)};
            if (sample < 4) for (auto& r : c.vreg) for (auto& x : r) x = edges[sample];
            if (group == 1 || group == 2) {  // First four exact values; later full IEEE payloads.
                const double values[] = {0.0, 1.0, -1.0, 2.0};
                if (sample < 4)
                    for (auto& r : c.vreg) for (auto& x : r) std::memcpy(&x, &values[sample], 8);
            }
            if (group == 2) {
                const uint64_t fp_edges[] = {0, UINT64_C(0x8000000000000000),
                    UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
                    UINT64_C(0x7ff8000000000012), UINT64_C(0x7ff0000000000012), 1,
                    UINT64_C(0x7fefffffffffffff), UINT64_C(0x7fc000127fc00012),
                    UINT64_C(0x7f8000127f800012), UINT64_C(0x0000000100000001),
                    UINT64_C(0x3ff8000000000000)};
                if (sample / 16 < 12)
                    for (auto& r : c.vreg) for (auto& x : r) x = fp_edges[sample / 16];
            }
            c.n = next() & 1; c.z = next() & 1; c.c = next() & 1; c.v = next() & 1;
            c.fpcr = group == 2 ? uint64_t(sample & 15) << 22 : 0;
            c.fpsr = (sample & 1) ? 0x08000000 : 0;

            uint64_t index_register = 0;
            if (memory_group) {
                c.fpcr = 0;
                c.fpsr = 0;
                // The first 512 samples sweep the base across a page boundary
                // one byte at a time, so for every encoding - whatever its own
                // displacement - some sample straddles the boundary and some
                // sample is misaligned. Later samples are random within a
                // margin wide enough for the largest displacement in the table.
                uint64_t base_off;
                if (sample < 0x200) base_off = 2 * kPageSize - 0x100 + sample;
                else base_off = 0x800 + (next() % (kRegionSize - 0x1000));
                c.x[1] = kRegionBase + base_off;
                c.x[31] = kRegionBase + (base_off & ~uint64_t(15));  // SP
                // Index register for register-offset forms. Small, so the
                // address stays inside the region, and signed on alternate
                // samples for SXTW so a sign-extension mistake shows up as a
                // wrong address rather than never being exercised.
                const uint64_t k = next() % 16;
                if (IsRegisterOffset(cb.word[0]) && ExtendOption(cb.word[0]) == 6 && (sample & 1))
                    index_register = (uint64_t)(int64_t)(int32_t)-(int32_t)k;
                else
                    index_register = k;
                if (IsRegisterOffset(cb.word[0])) c.x[2] = index_register;

                for (auto& b : master_image) b = (uint8_t)next();
                std::memcpy(oracle_image, master_image, kRegionSize);
            }

            const GuestContext initial = c;

            jit.Reset();
            if (sample == 0) jit.ClearCache();
            jit.ClearExclusiveState();
            monitor.Clear();
            jit.SetPC(0x1000);
            jit.SetSP(c.x[31]);
            for (unsigned r = 0; r < 31; r++) jit.SetRegister(r, c.x[r]);
            for (unsigned r = 0; r < 32; r++) jit.SetVector(r, {c.vreg[r][0], c.vreg[r][1]});
            jit.SetFpcr((uint32_t)c.fpcr);
            jit.SetFpsr((uint32_t)c.fpsr);
            jit.SetPstate(nzcv(c));
            cb.unsupported = false;
            for (unsigned k = 0; k < length; k++) jit.Step();
            if (cb.unsupported) {
                std::fprintf(stderr, "ORACLE_UNSUPPORTED word=%08x\n", cb.word[0]);
                return 2;
            }
            if (access_log.out_of_region) {
                std::fprintf(stderr,
                    "OUT_OF_REGION word=%08x sample=%u address=%016llx size=%u base=%016llx\n"
                    "the case addressed outside the shared region, so nothing was compared\n",
                    cb.word[0], sample, (unsigned long long)access_log.bad_address,
                    access_log.bad_size, (unsigned long long)initial.x[1]);
                return 2;
            }

            if (group == 4) {
                if (jit.GetRegister(4) == 0) stxr_success++;
                else stxr_failure++;
            }

            // The emitted code runs once per memory backend. The flat buffer is
            // the standalone runtime's path; the page table is the path suyu
            // actually drives, through recomp_host_ptr and the pair same-page
            // split, and those have never been compared against anything.
            const unsigned backends = memory_group ? 2 : 1;
            for (unsigned backend = 0; backend < backends; backend++) {
                GuestContext e = initial;
                if (memory_group) {
                    std::memcpy(emitted_image, master_image, kRegionSize);
                    e.mem = emitted_image;
                    e.mem_size = kRegionSize;
                    e.mem_base_vaddr = kRegionBase;
                    e.host_mem = backend == 1 ? &host_bridge : nullptr;
                    recomp_clrex(&e);
                }
                differential_emit(op, &e);
                if (inject) e.x[0] ^= 1;
                // Registers matching while the region is never really compared
                // is the way a memory differential passes without testing
                // memory. This flips one byte the emitted side wrote and the
                // expected result is a reported divergence, never a clean run.
                if (inject_memory && memory_group) emitted_image[0] ^= 1;

                auto mismatch = [&](const char* field, unsigned index, uint64_t actual,
                                    uint64_t expected) {
                    std::fprintf(stderr,
                        "DIVERGENCE seed=0x%016llx case_seed=0x%016llx word=%08x",
                        (unsigned long long)seed, (unsigned long long)case_seed, cb.word[0]);
                    for (unsigned k = 1; k < length; k++) std::fprintf(stderr, ",%08x", cb.word[k]);
                    std::fprintf(stderr,
                        " sample=%u backend=%s base=%016llx index=%016llx fpcr=%08x"
                        " field=%s[%u] emitted=%016llx dynarmic=%016llx\n",
                        sample, backend ? "page-table" : "flat",
                        (unsigned long long)initial.x[1], (unsigned long long)index_register,
                        jit.GetFpcr(), field, index, (unsigned long long)actual,
                        (unsigned long long)expected);
                    return 1;
                };

                for (unsigned r = 0; r < 31; r++)
                    if (e.x[r] != jit.GetRegister(r))
                        return mismatch("x", r, e.x[r], jit.GetRegister(r));
                if (e.x[31] != jit.GetSP()) return mismatch("sp", 0, e.x[31], jit.GetSP());
                for (unsigned r = 0; r < 32; r++)
                    for (unsigned h = 0; h < 2; h++)
                        if (e.vreg[r][h] != jit.GetVector(r)[h])
                            return mismatch("v", r * 2 + h, e.vreg[r][h], jit.GetVector(r)[h]);
                if (nzcv(e) != (jit.GetPstate() & 0xf0000000))
                    return mismatch("nzcv", 0, nzcv(e), jit.GetPstate() & 0xf0000000);
                if (e.fpcr != jit.GetFpcr()) return mismatch("fpcr", 0, e.fpcr, jit.GetFpcr());
                if (e.fpsr != jit.GetFpsr()) return mismatch("fpsr", 0, e.fpsr, jit.GetFpsr());
                if (jit.GetPC() != 0x1000 + 4 * length)
                    return mismatch("pc", 0, 0x1000 + 4 * length, jit.GetPC());
                if (memory_group) {
                    // Registers agreeing is not enough: a store that went to the
                    // wrong address, or did not happen at all, is only visible
                    // in the region itself.
                    for (uint64_t off = 0; off < kRegionSize; off++)
                        if (emitted_image[off] != oracle_image[off])
                            return mismatch("mem", (unsigned)off, emitted_image[off],
                                            oracle_image[off]);
                }
                checks++;
            }
        }
        if (memory_group) {
            const bool touched = access_log.count != accesses_before;
            const bool expected_none = differential_expect_no_access(op) != 0;
            if (touched == expected_none) {
                std::fprintf(stderr,
                    "ACCESS_EXPECTATION word=%08x expected %s over %u samples but observed %s\n"
                    "a memory case that does not address the shared region tests nothing\n",
                    cb.word[0], expected_none ? "no access" : "an access", samples,
                    touched ? "an access" : "none");
                return 2;
            }
        }
        total_accesses = access_log.count;
        total_page_crossing = access_log.page_crossing;
    }

    if (!checks) { std::fputs("no matching cases\n", stderr); return 2; }
    if (group == 4 && (!stxr_success || !stxr_failure)) {
        std::fprintf(stderr,
            "EXCLUSIVE_COVERAGE store-exclusive succeeded %llu times and failed %llu times\n"
            "both outcomes have to occur or the comparison never reached one of the paths\n",
            (unsigned long long)stxr_success, (unsigned long long)stxr_failure);
        return 2;
    }
    std::printf("%u actual-Dynarmic differential comparisons passed across %u cases (group %u)\n",
                checks, cases_run, group);
    if (memory_group)
        std::printf("oracle memory accesses: %llu, of which %llu crossed a page boundary\n",
                    (unsigned long long)total_accesses, (unsigned long long)total_page_crossing);
    if (group == 4)
        std::printf("store-exclusive outcomes: %llu succeeded, %llu failed\n",
                    (unsigned long long)stxr_success, (unsigned long long)stxr_failure);
}
