// Generate a synthetic module whose valid entry sits after an unhandled word.
// No cartridge content is used.
#include <cstdint>
#include <string>
#include <vector>

#include "core/recompiler/arm64_to_c.h"

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    constexpr std::uint32_t code[]{
        0xD2800020, // mov x0, #1 -- must not execute for the interior entry
        0xFFFFFFFF, // unhandled -- must not hide the later entry
        0xD503201F, // nop -- interior entry
        0xD65F03C0, // ret
        0x00000000, // post-return padding stays outside the lookup
        0x91000400, // add x0, x0, #1 -- root reached by the bounded module slice
        0x14000001, // b +4
        0x17FFFFFE, // b -8
        0xD503201F, // final block root
        0xD65F03C0, // final block interior entry and index high bound
    };
    const auto stats = suyu::recomp::EmitProject(
        "side", reinterpret_cast<const std::uint8_t*>(code), sizeof(code), 0x1000,
        argv[1], true);
    return stats.blocks == 4 && stats.emitted == 9 && stats.unhandled == 1 ? 0 : 1;
}
