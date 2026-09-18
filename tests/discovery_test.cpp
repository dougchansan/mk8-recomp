// Synthetic discovery regressions; no guest dumps or runtime prerequisites.
#include "core/recompiler/arm64_to_c.h"
#include <chrono>
#include <initializer_list>
#include <iostream>

using namespace suyu::recomp;
static int failures = 0;

static void Check(const char* name, const std::vector<u32>& words,
                  std::initializer_list<std::pair<u32, u32>> expected,
                  u64 entry = 0, const std::vector<u64>& roots = {}) {
    auto blocks = DiscoverBlocks(reinterpret_cast<const u8*>(words.data()), words.size() * 4,
                                 0x1000, entry, &roots);
    bool ok = blocks.size() == expected.size();
    size_t i = 0;
    for (const auto& [offset, count] : expected) {
        if (i >= blocks.size()) { ok = false; break; }
        const auto& b = blocks[i++];
        ok &= b.vaddr == 0x1000 + offset * 4 && b.count == count && b.size == count * 4;
        ok &= b.is_entry == (offset == 0);
    }
    std::cout << (ok ? "ok " : "FAIL ") << name << '\n';
    failures += !ok;
}

int main() {
    constexpr u32 nop = 0xD503201F, ret = 0xD65F03C0;
    Check("empty", {}, {});
    Check("all zero", {0, 0, 0, 0}, {{0, 1}});
    Check("fallthrough trap and restart", {nop, 0, 0, nop, ret},
          {{0, 1}, {1, 1}, {3, 2}});
    Check("post return padding", {ret, 0, 0, nop, ret},
          {{0, 1}, {3, 2}});
    Check("post branch register padding", {0xD61F0000, 0, 0, nop, ret},
          {{0, 1}, {3, 2}});
    Check("post unconditional branch padding", {0x14000003, 0, 0, nop, ret},
          {{0, 1}, {3, 2}});
    Check("post return entry overrides padding", {ret, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}}, 0x1004);
    Check("post return extra root overrides padding", {ret, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}}, 0, {0x1004});
    Check("post return interior root survives padding", {ret, 0, 0, ret},
          {{0, 1}, {2, 1}, {3, 1}}, 0, {0x1008});
    Check("branch targets immediate padding", {0x14000001, 0, 0},
          {{0, 1}, {1, 1}});
    // ADRP x0, current page; ADD x0, x0, #12; RET; rooted zero.
    Check("computed root overrides post return padding", {0x90000000, 0x91003000, ret, 0, 0},
          {{0, 3}, {3, 1}});
    Check("call preserves return trap", {0x94000003, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("indirect call preserves return trap", {0xD63F0000, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("conditional preserves fallthrough trap", {0x54000060, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("cbz preserves fallthrough trap", {0x34000060, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("tbz preserves fallthrough trap", {0x36000060, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("later branch targets first zero", {ret, 0, 0, 0x17FFFFFE},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("later branch targets interior zero", {ret, 0, 0, 0x17FFFFFF},
          {{0, 1}, {2, 1}, {3, 1}});
    Check("svc preserves return trap", {0xD4000001, 0, 0, ret},
          {{0, 1}, {1, 1}, {3, 1}});
    Check("nonzero post return remains", {ret, 1, 2, ret},
          {{0, 1}, {1, 3}});
    Check("entry inside zeros", {0, 0, 0, 0}, {{0, 1}, {2, 1}}, 0x1008);
    Check("extra roots inside zeros", {0, 0, 0, 0, 0},
          {{0, 1}, {2, 1}, {3, 1}}, 0, {0x1008, 0x100C, 0x1008, 0x2000});
    Check("direct branch root inside zeros", {0x14000003, 0, 0, 0, 0},
          {{0, 1}, {3, 1}});
    // ADRP x0, current page; ADD x0, x0, #16 materializes the fourth zero.
    Check("computed root inside zeros", {0x90000000, 0x91004000, 0, 0, 0, 0},
          {{0, 2}, {2, 1}, {4, 1}});
    Check("nonzero undefined words retained", {0xFFFFFFFF, 0xE7C00000, ret}, {{0, 3}});
    Check("ordinary roots unchanged", {nop, nop, ret, nop, ret},
          {{0, 1}, {1, 2}, {3, 2}}, 0, {0x1004});

    // Deterministic mixtures check preservation independently of block layout.
    bool property_ok = true;
    u32 seed = 0xA64;
    const u32 alphabet[] = {0, 0, 0, nop, ret, 0xD61F0000, 0x94000000,
                            0x14000000, 0x54000000, 1, 0xFFFFFFFF};
    for (u32 trial = 0; trial < 256; ++trial) {
        std::vector<u32> mixed(64);
        std::vector<u64> roots;
        for (u32 k = 0; k < mixed.size(); ++k) {
            seed = seed * 1664525u + 1013904223u;
            mixed[k] = alphabet[seed % std::size(alphabet)];
            if ((seed >> 24) < 32) roots.push_back(0x1000 + k * 4);
        }
        const auto blocks = DiscoverBlocks(reinterpret_cast<const u8*>(mixed.data()),
                                           mixed.size() * 4, 0x1000, 0, &roots);
        std::vector<u32> covered(mixed.size());
        std::unordered_set<u64> starts;
        for (const auto& block : blocks) {
            starts.insert(block.vaddr);
            for (u32 k = 0; k < block.count; ++k) {
                const size_t index = (block.vaddr - 0x1000) / 4 + k;
                if (index >= covered.size()) { property_ok = false; break; }
                ++covered[index];
            }
        }
        for (size_t k = 0; k < mixed.size(); ++k) {
            property_ok &= covered[k] <= 1 && (mixed[k] == 0 || covered[k] == 1);
        }
        for (u64 root : roots) property_ok &= starts.count(root) == 1;
    }
    std::cout << (property_ok ? "ok " : "FAIL ")
              << "256 mixed arrays preserve nonzero words and explicit roots\n";
    failures += !property_ok;

    const std::vector<u32> words{nop, 0, 0, 0, ret};
    const auto dir = std::filesystem::temp_directory_path() /
                     ("suyu-discovery-test-" + std::to_string(
                         std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto stats = EmitProject("synthetic", reinterpret_cast<const u8*>(words.data()),
                                   words.size() * 4, 0x1000, dir.string(), true);
    std::ifstream source(dir / "src/recompiled_synthetic_0.c");
    std::string body((std::istreambuf_iterator<char>(source)), {});
    const std::string trap = "recomp_unhandled(c,0x00000000U,g_module_base+0x1004ULL)";
    const auto first = body.find(trap);
    const bool ok = stats.emitted == 3 && stats.unhandled == 1 &&
                    first != std::string::npos && body.find(trap, first + 1) == std::string::npos;
    std::cout << (ok ? "ok " : "FAIL ") << "export retains exact first trap PC\n";
    failures += !ok;

    // Every instruction in a nonzero block is an independently valid indirect
    // target. Emit one body, route interior lookups back to it, and retain
    // later side entries even when an earlier word terminates as unhandled.
    const std::vector<u32> side_words{0xD2800020, 0xFFFFFFFF, nop, ret};
    const auto side_dir = dir / "side-entry";
    const auto side_stats =
        EmitProject("side", reinterpret_cast<const u8*>(side_words.data()),
                    side_words.size() * 4, 0x1000, side_dir.string(), true);
    std::ifstream side_source(side_dir / "src/recompiled_side_0.c");
    std::string side((std::istreambuf_iterator<char>(side_source)), {});
    const std::string fn = FuncName("side", 0x1000);
    const bool side_ok =
        side_stats.blocks == 1 && side_stats.emitted == 4 && side_stats.unhandled == 1 &&
        side.find("case 1U: goto _recomp_entry_1;") != std::string::npos &&
        side.find("case 2U: goto _recomp_entry_2;") != std::string::npos &&
        side.find("_recomp_entry_2: ;") != std::string::npos &&
        side.find("{0x1000ULL, 0x100cULL, " + fn + "}") != std::string::npos;
    std::cout << (side_ok ? "ok " : "FAIL ")
              << "all emitted instructions have side entries\n";
    failures += !side_ok;
    // Keep synthetic output for inspection; never remove user-owned paths.
    std::cout << "Synthetic export: " << dir << '\n';
    return failures ? 1 : 0;
}
