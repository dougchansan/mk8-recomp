#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "core/recompiler/arm64_to_c.h"

namespace {

// A case is one to three real instructions. More than one word exists for the
// exclusives: STXR only reports success when it follows the LDXR that took the
// reservation, and CLREX only becomes observable through a later STXR, so
// testing these one instruction at a time can never leave the failure path.
constexpr unsigned kMaxWords = 3;
struct Case {
    unsigned word[kMaxWords] = {0, 0, 0};
    const char* mnemonic[kMaxWords] = {nullptr, nullptr, nullptr};  // checked by disassembler
    unsigned count = 1;
    unsigned group = 0;
    // A handful of cases are supposed to touch no memory at all - a STXR with
    // no reservation must fail without storing. The runner asserts both
    // directions: these must access nothing, everything else must access
    // something. A case that quietly stopped addressing the shared region
    // would otherwise keep reporting passes forever.
    bool expect_no_access = false;
};

std::vector<Case> cases;

void Add(unsigned group, unsigned word) {
    Case c;
    c.word[0] = word;
    c.group = group;
    cases.push_back(c);
}
void AddChecked(unsigned group, unsigned word, const char* mnemonic) {
    Case c;
    c.word[0] = word;
    c.mnemonic[0] = mnemonic;
    c.group = group;
    cases.push_back(c);
}

// Encodings below were produced by the field helpers in the ARM manual's
// load/store sections and every one is checked against a disassembler at build
// time by differential_verify_assembly.py. A silently mis-encoded word is the
// one failure this harness cannot detect by itself: it would decode to some
// other legal instruction, both engines would agree about it, and the case
// would report a pass while testing nothing of what it names.
void AddIntegerAndFpCases() {
    // Group 0 is integer, group 1 scalar FP; the original harness told them
    // apart by encoding and that classification is preserved exactly.
    auto classify = [](unsigned word) {
        return (word & 0x5e000000u) == 0x1e000000u ? 1u : 0u;
    };
    for (unsigned size = 0; size < 4; size++) {
        Add(classify(0x4e222c20u), 0x4e222c20u | (size << 22));  // SQSUB vector
        Add(classify(0x6e220c20u), 0x6e220c20u | (size << 22));  // UQADD vector
        Add(classify(0x5e207820u), 0x5e207820u | (size << 22));  // SQABS scalar
        if (size < 3) {
            Add(classify(0x4e229420u), 0x4e229420u | (size << 22));  // MLA
            Add(classify(0x0e214820u), 0x0e214820u | (size << 22));  // SQXTN
        }
    }
    for (unsigned word : {0xab020020u, 0xeb020020u, 0x9b020c20u, 0x4e223420u,
                          0x1e622820u, 0x1e623820u, 0x1e620820u, 0x1f420c20u,
                          0x1e221820u, 0x1e621820u})
        Add(classify(word), word);
    const std::pair<unsigned, const char*> extra_integer[] = {
        {0x6e207820, "sqneg v0.16b, v1.16b"}, {0x2e212820, "sqxtun v0.8b, v1.8h"},
        {0x6ea14820, "uqxtn2 v0.4s, v1.2d"},  {0x4e62b420, "sqdmulh v0.8h, v1.8h, v2.8h"},
        {0x7ea2b420, "sqrdmulh s0, s1, s2"},  {0x4f72c820, "sqdmulh v0.8h, v1.8h, v2.h[7]"},
        {0x4e224c20, "sqshl v0.16b, v1.16b, v2.16b"}, {0x7ee24c20, "uqshl d0, d1, d2"}};
    for (auto [word, mnemonic] : extra_integer) AddChecked(classify(word), word, mnemonic);

    const std::pair<unsigned, const char*> added[] = {
        {0x4e22f420, "fmax v0.4s, v1.4s, v2.4s"}, {0x4ea2f420, "fmin v0.4s, v1.4s, v2.4s"},
        {0x4e62c420, "fmaxnm v0.2d, v1.2d, v2.2d"}, {0x4ee2c420, "fminnm v0.2d, v1.2d, v2.2d"},
        {0x6e22f420, "fmaxp v0.4s, v1.4s, v2.4s"}, {0x7e70f820, "fmaxp d0, v1.2d"},
        {0x6e30f820, "fmaxv s0, v1.4s"}, {0x6eb0c820, "fminnmv s0, v1.4s"},
        {0x4e218820, "frintn v0.4s, v1.4s"}, {0x4ea18820, "frintp v0.4s, v1.4s"},
        {0x4e219820, "frintm v0.4s, v1.4s"}, {0x4ea19820, "frintz v0.4s, v1.4s"},
        {0x6e618820, "frinta v0.2d, v1.2d"}, {0x6e619820, "frintx v0.2d, v1.2d"},
        {0x6ee19820, "frinti v0.2d, v1.2d"},
        {0x0e616820, "fcvtn v0.2s, v1.2d"}, {0x4e616820, "fcvtn2 v0.4s, v1.2d"},
        {0x0e617820, "fcvtl v0.2d, v1.2s"}, {0x4e617820, "fcvtl2 v0.2d, v1.4s"},
        {0x5e61a820, "fcvtns d0, d1"}, {0x7e21c820, "fcvtau s0, s1"},
        {0x4e21a820, "fcvtns v0.4s, v1.4s"}, {0x6e61c820, "fcvtau v0.2d, v1.2d"},
        {0x6ee1f820, "fsqrt v0.2d, v1.2d"}, {0x6ee2d420, "fabd v0.2d, v1.2d, v2.2d"},
        {0x4e22d420, "fadd v0.4s, v1.4s, v2.4s"}, {0x4ee2d420, "fsub v0.2d, v1.2d, v2.2d"},
        {0x7e70d820, "faddp d0, v1.2d"},
        {0x6e22dc20, "fmul v0.4s, v1.4s, v2.4s"}, {0x4e22cc20, "fmla v0.4s, v1.4s, v2.4s"},
        {0x4ea2cc20, "fmls v0.4s, v1.4s, v2.4s"}, {0x4f829020, "fmul v0.4s, v1.4s, v2.s[0]"},
        {0x4f821020, "fmla v0.4s, v1.4s, v2.s[0]"}, {0x4f825020, "fmls v0.4s, v1.4s, v2.s[0]"},
        {0x1e221820, "fdiv s0, s1, s2"}, {0x1e621820, "fdiv d0, d1, d2"},
        {0x6e22fc20, "fdiv v0.4s, v1.4s, v2.4s"}, {0x6e62fc20, "fdiv v0.2d, v1.2d, v2.2d"},
        {0x6ea1d820, "frsqrte v0.4s, v1.4s"}, {0x6ee1d820, "frsqrte v0.2d, v1.2d"},
        {0x7ea1d820, "frsqrte s0, s1"}, {0x7ee1d820, "frsqrte d0, d1"},
        {0x4ea1d820, "frecpe v0.4s, v1.4s"}, {0x4ee1d820, "frecpe v0.2d, v1.2d"},
        {0x5ea1d820, "frecpe s0, s1"}, {0x5ee1d820, "frecpe d0, d1"}};
    for (auto [word, mnemonic] : added) AddChecked(2, word, mnemonic);
}

// Group 3: one memory instruction per case.
//
// Every case addresses through x1 (or SP), carries pair data in x3 and a
// store-exclusive status in x4, so the runner knows which registers have to be
// aimed at the shared region and which are free to stay random. Destination is
// x0/v0 throughout.
void AddMemoryCases() {
    const std::pair<unsigned, const char*> mem[] = {
        // Unsigned scaled immediate offset, every integer width and both
        // sign-extending and zero-extending loads.
        {0x39002020u, "strb w0, [x1, #8]"},        {0x39402020u, "ldrb w0, [x1, #8]"},
        {0x39802020u, "ldrsb x0, [x1, #8]"},       {0x39c02020u, "ldrsb w0, [x1, #8]"},
        {0x79002020u, "strh w0, [x1, #0x10]"},     {0x79402020u, "ldrh w0, [x1, #0x10]"},
        {0x79802020u, "ldrsh x0, [x1, #0x10]"},    {0x79c02020u, "ldrsh w0, [x1, #0x10]"},
        {0xb9002020u, "str w0, [x1, #0x20]"},      {0xb9402020u, "ldr w0, [x1, #0x20]"},
        {0xb9802020u, "ldrsw x0, [x1, #0x20]"},
        {0xf9002020u, "str x0, [x1, #0x40]"},      {0xf9402020u, "ldr x0, [x1, #0x40]"},
        // Same, SIMD, 8/16/32/64/128.
        {0x3d002020u, "str b0, [x1, #8]"},         {0x3d402020u, "ldr b0, [x1, #8]"},
        {0x7d002020u, "str h0, [x1, #0x10]"},      {0x7d402020u, "ldr h0, [x1, #0x10]"},
        {0xbd002020u, "str s0, [x1, #0x20]"},      {0xbd402020u, "ldr s0, [x1, #0x20]"},
        {0xfd002020u, "str d0, [x1, #0x40]"},      {0xfd402020u, "ldr d0, [x1, #0x40]"},
        {0x3d802020u, "str q0, [x1, #0x80]"},      {0x3dc02020u, "ldr q0, [x1, #0x80]"},
        // Unscaled signed offset (LDUR/STUR), including deliberately
        // misaligned displacements.
        {0xf85f8020u, "ldur x0, [x1, #-8]"},       {0xf81f8020u, "stur x0, [x1, #-8]"},
        {0x3cdf0020u, "ldur q0, [x1, #-0x10]"},    {0x3c9f0020u, "stur q0, [x1, #-0x10]"},
        {0xbc403020u, "ldur s0, [x1, #3]"},        {0xb89fc020u, "ldursw x0, [x1, #-4]"},
        {0x78401020u, "ldurh w0, [x1, #1]"},
        // Pre- and post-index, with the writeback the runner then compares.
        {0xf8408420u, "ldr x0, [x1], #8"},         {0xf8408c20u, "ldr x0, [x1, #8]!"},
        {0xf81f0420u, "str x0, [x1], #-0x10"},     {0xf81f0c20u, "str x0, [x1, #-0x10]!"},
        {0x38c01420u, "ldrsb w0, [x1], #1"},
        {0x3cc10c20u, "ldr q0, [x1, #0x10]!"},     {0x3c810420u, "str q0, [x1], #0x10"},
        // Register offset: every extend and both scaled and unscaled.
        {0xf8626820u, "ldr x0, [x1, x2]"},         {0xf8627820u, "ldr x0, [x1, x2, lsl #3]"},
        {0xf8624820u, "ldr x0, [x1, w2, uxtw]"},   {0xf862c820u, "ldr x0, [x1, w2, sxtw]"},
        {0xf862f820u, "ldr x0, [x1, x2, sxtx #3]"},{0xf862d820u, "ldr x0, [x1, w2, sxtw #3]"},
        {0xb862d820u, "ldr w0, [x1, w2, sxtw #2]"},{0xb8625820u, "ldr w0, [x1, w2, uxtw #2]"},
        {0xb8a27820u, "ldrsw x0, [x1, x2, lsl #2]"},
        {0x78a27820u, "ldrsh x0, [x1, x2, lsl #1]"},
        {0x38a26820u, "ldrsb x0, [x1, x2]"},       {0x38626820u, "ldrb w0, [x1, x2]"},
        {0x3822c820u, "strb w0, [x1, w2, sxtw]"},  {0x78225820u, "strh w0, [x1, w2, uxtw #1]"},
        {0x3ce27820u, "ldr q0, [x1, x2, lsl #4]"}, {0x3ca2d820u, "str q0, [x1, w2, sxtw #4]"},
        // Pairs, GPR and SIMD, signed offset and pre/post index, plus LDPSW.
        {0x29010c20u, "stp w0, w3, [x1, #8]"},     {0x29410c20u, "ldp w0, w3, [x1, #8]"},
        {0xa9010c20u, "stp x0, x3, [x1, #0x10]"},  {0xa9410c20u, "ldp x0, x3, [x1, #0x10]"},
        {0x69410c20u, "ldpsw x0, x3, [x1, #8]"},   {0x68c10c20u, "ldpsw x0, x3, [x1], #8"},
        {0x69ff0c20u, "ldpsw x0, x3, [x1, #-8]!"},
        {0xa8c10c20u, "ldp x0, x3, [x1], #0x10"},  {0xa9bf0c20u, "stp x0, x3, [x1, #-0x10]!"},
        {0x28c10c20u, "ldp w0, w3, [x1], #8"},     {0x29bf0c20u, "stp w0, w3, [x1, #-8]!"},
        {0x2d010c20u, "stp s0, s3, [x1, #8]"},     {0x2d410c20u, "ldp s0, s3, [x1, #8]"},
        {0x6d010c20u, "stp d0, d3, [x1, #0x10]"},  {0x6d410c20u, "ldp d0, d3, [x1, #0x10]"},
        {0xad010c20u, "stp q0, q3, [x1, #0x20]"},  {0xad410c20u, "ldp q0, q3, [x1, #0x20]"},
        {0xacc10c20u, "ldp q0, q3, [x1], #0x20"},  {0xad810c20u, "stp q0, q3, [x1, #0x20]!"},
        {0x2cc10c20u, "ldp s0, s3, [x1], #8"},     {0x6dc10c20u, "ldp d0, d3, [x1, #0x10]!"},
        {0x6c810c20u, "stp d0, d3, [x1], #0x10"},
        // LDNP/STNP (non-temporal pair, mode 000) are deliberately absent:
        // Translate() has no encoding for them and generation aborts if they
        // are listed. They are a real decode gap, not a harness limitation.
        // SP as the base register. Load/store treats Rn=31 as SP rather than
        // XZR, and SP is compared like any other register.
        {0xf94023e0u, "ldr x0, [sp, #0x40]"},      {0xf90023e0u, "str x0, [sp, #0x40]"},
        {0x394023e0u, "ldrb w0, [sp, #8]"},        {0x3dc023e0u, "ldr q0, [sp, #0x80]"},
        {0xa9410fe0u, "ldp x0, x3, [sp, #0x10]"},  {0xa9bf0fe0u, "stp x0, x3, [sp, #-0x10]!"},
        {0xf85f83e0u, "ldur x0, [sp, #-8]"},
        // Acquire/release ordered accesses.
        {0x08dffc20u, "ldarb w0, [x1]"},           {0x089ffc20u, "stlrb w0, [x1]"},
        {0x48dffc20u, "ldarh w0, [x1]"},           {0x489ffc20u, "stlrh w0, [x1]"},
        {0x88dffc20u, "ldar w0, [x1]"},            {0x889ffc20u, "stlr w0, [x1]"},
        {0xc8dffc20u, "ldar x0, [x1]"},            {0xc89ffc20u, "stlr x0, [x1]"},
    };
    for (auto [word, mnemonic] : mem) AddChecked(3, word, mnemonic);

    // A lone store-exclusive. No reservation is held, so both engines have to
    // report failure in the status register and leave memory untouched - which
    // means performing no access at all.
    const std::pair<unsigned, const char*> lone_exclusive[] = {
        {0x08047c20u, "stxrb w4, w0, [x1]"}, {0x88047c20u, "stxr w4, w0, [x1]"},
        {0xc8047c20u, "stxr w4, x0, [x1]"},  {0xc8240c20u, "stxp w4, x0, x3, [x1]"},
    };
    for (auto [word, mnemonic] : lone_exclusive) {
        AddChecked(3, word, mnemonic);
        cases.back().expect_no_access = true;
    }
}

// Group 4: load-exclusive followed by store-exclusive to the same address, so
// the store succeeds. This is the only way to reach the success path, and the
// value STXR writes to its status register is compared like any other result.
void AddMemorySequenceCases() {
    auto add = [](std::initializer_list<std::pair<unsigned, const char*>> insns) {
        Case c;
        c.group = 4;
        c.count = (unsigned)insns.size();
        unsigned k = 0;
        for (auto [w, m] : insns) { c.word[k] = w; c.mnemonic[k] = m; k++; }
        cases.push_back(c);
    };
    // Load-exclusive then store-exclusive to the same address: the store
    // succeeds, memory changes, and the status register must read 0.
    add({{0x085f7c20u, "ldxrb w0, [x1]"},  {0x08047c20u, "stxrb w4, w0, [x1]"}});
    add({{0x485f7c20u, "ldxrh w0, [x1]"},  {0x48047c20u, "stxrh w4, w0, [x1]"}});
    add({{0x885f7c20u, "ldxr w0, [x1]"},   {0x88047c20u, "stxr w4, w0, [x1]"}});
    add({{0xc85f7c20u, "ldxr x0, [x1]"},   {0xc8047c20u, "stxr w4, x0, [x1]"}});
    add({{0x085ffc20u, "ldaxrb w0, [x1]"}, {0x0804fc20u, "stlxrb w4, w0, [x1]"}});
    add({{0x485ffc20u, "ldaxrh w0, [x1]"}, {0x4804fc20u, "stlxrh w4, w0, [x1]"}});
    add({{0x885ffc20u, "ldaxr w0, [x1]"},  {0x8804fc20u, "stlxr w4, w0, [x1]"}});
    add({{0xc85ffc20u, "ldaxr x0, [x1]"},  {0xc804fc20u, "stlxr w4, x0, [x1]"}});
    add({{0x887f0c20u, "ldxp w0, w3, [x1]"},  {0x88240c20u, "stxp w4, w0, w3, [x1]"}});
    add({{0xc87f0c20u, "ldxp x0, x3, [x1]"},  {0xc8240c20u, "stxp w4, x0, x3, [x1]"}});
    add({{0xc87f8c20u, "ldaxp x0, x3, [x1]"}, {0xc8248c20u, "stlxp w4, x0, x3, [x1]"}});
    // The store-exclusive must consume the reservation, so a second one
    // immediately after has to fail and must leave memory alone. A monitor that
    // only records "a reservation exists" passes the case above and fails here.
    add({{0xc85f7c20u, "ldxr x0, [x1]"}, {0xc8047c20u, "stxr w4, x0, [x1]"},
         {0xc8047c20u, "stxr w4, x0, [x1]"}});
    add({{0x885f7c20u, "ldxr w0, [x1]"}, {0x88047c20u, "stxr w4, w0, [x1]"},
         {0x88047c20u, "stxr w4, w0, [x1]"}});
    // CLREX drops the reservation, so the store after it must fail. CLREX has
    // no architectural state of its own, so only a following STXR can observe
    // whether it did anything.
    add({{0xc85f7c20u, "ldxr x0, [x1]"}, {0xd5033f5fu, "clrex"},
         {0xc8047c20u, "stxr w4, x0, [x1]"}});
    // A plain store to the reserved granule between the pair. Real hardware
    // clears the reservation here; neither engine models that - Dynarmic's
    // monitor is only consulted by exclusive operations, and the standalone
    // runtime keeps a bare address latch - so both report success. The case is
    // kept because it pins that shared, deliberate divergence from the
    // architecture in place: if either side ever starts modelling it, this
    // stops matching and the difference gets looked at rather than discovered
    // in a game.
    add({{0xc85f7c20u, "ldxr x0, [x1]"}, {0xf9000020u, "str x0, [x1]"},
         {0xc8047c20u, "stxr w4, x0, [x1]"}});
}

}  // namespace

int main(int argc, char** argv) {
    AddIntegerAndFpCases();
    AddMemoryCases();
    AddMemorySequenceCases();

    const std::string mode = argc == 2 ? argv[1] : std::string();

    // The runtime is emitted rather than reimplemented. Memory cases call the
    // production recomp_load*/recomp_store*/recomp_ldp*/recomp_ldxr helpers, so
    // the page-table fast path, the pair same-page split and the exclusive
    // reservation logic are all under test and not just the statements
    // Translate() puts around them.
    if (mode == "--runtime-h") { std::fputs(suyu::recomp::RuntimeH(), stdout); return 0; }
    if (mode == "--runtime-c") { std::fputs(suyu::recomp::RuntimeC(), stdout); return 0; }

    // Expected disassembly for every case that declares one, for the
    // disassembler check in differential_verify_assembly.py.
    if (mode == "--encodings") {
        for (const auto& c : cases)
            for (unsigned k = 0; k < c.count; k++)
                if (c.mnemonic[k]) std::printf("%08x\t%s\n", c.word[k], c.mnemonic[k]);
        return 0;
    }

    // The GNU assembler listing the original harness checked against. Kept for
    // the Linux path; the disassembler check covers the same ground and also
    // runs where no aarch64 assembler is installed.
    if (mode == "--assembly") {
        std::puts(".text");
        for (const auto& c : cases)
            for (unsigned k = 0; k < c.count; k++)
                if (c.mnemonic[k]) std::printf(".inst 0x%08x\n%s\n", c.word[k], c.mnemonic[k]);
        return 0;
    }

    std::puts("#include <math.h>\n#include <fenv.h>\n#include <float.h>\n"
              "#include <string.h>\n#include \"differential_state.h\"");
    // The runtime's dispatcher is linked in but never entered: single cases are
    // called directly. Supplying the one symbol the module would otherwise
    // provide keeps the real runtime translation unit intact.
    std::puts("BlockFn recomp_lookup(uint64_t pc){(void)pc;return 0;}");

    for (unsigned j = 0; j < cases.size(); j++) {
        std::printf("static void f%u(GuestContext*c){", j);
        for (unsigned k = 0; k < cases[j].count; k++) {
            std::string body;
            bool miss = false;
            // Each instruction is translated at the address it actually
            // occupies, so anything PC-relative in a sequence is right.
            suyu::recomp::Translate(cases[j].word[k], 0x1000 + 4 * k, body, &miss);
            if (miss) {
                std::fprintf(stderr, "untranslated synthetic word %08x\n", cases[j].word[k]);
                return 1;
            }
            std::printf("{%s}\n", body.c_str());
        }
        std::puts("}");
    }

    std::puts("static void(*const functions[])(GuestContext*)={");
    for (unsigned j = 0; j < cases.size(); j++) std::printf("f%u,\n", j);
    std::puts("};\nstatic const uint32_t words[][3]={");
    for (const auto& c : cases)
        std::printf("{0x%08xu,0x%08xu,0x%08xu},\n", c.word[0], c.word[1], c.word[2]);
    std::puts("};\nstatic const unsigned char counts[]={");
    for (const auto& c : cases) std::printf("%u,\n", c.count);
    std::puts("};\nstatic const unsigned char no_access[]={");
    for (const auto& c : cases) std::printf("%u,\n", c.expect_no_access ? 1u : 0u);
    std::puts("};\nstatic const unsigned char groups[]={");
    for (const auto& c : cases) std::printf("%u,\n", c.group);
    std::puts("};\n"
              "unsigned differential_expect_no_access(unsigned i){return no_access[i];}\n"
              "unsigned differential_is_fp(unsigned i){return groups[i];}\n"
              "unsigned differential_count(void){return sizeof(counts)/sizeof(counts[0]);}\n"
              "unsigned differential_length(unsigned i){return counts[i];}\n"
              "uint32_t differential_word(unsigned i){return words[i][0];}\n"
              "uint32_t differential_word_at(unsigned i,unsigned k){return words[i][k];}\n"
              "void differential_emit(unsigned i,GuestContext*c){functions[i](c);}");
}
