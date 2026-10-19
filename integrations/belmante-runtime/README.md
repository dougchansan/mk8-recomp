# Belmante runtime integration for MK8D

This experimental adaptation uses the MIT-licensed
[SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp)
runtime and AArch64-to-C++ recompiler, pinned in `UPSTREAM.json`. Preserve
`LICENSE.upstream`; local modifications are GPL-3.0-or-later. The recipe contains
no game code, assets or generated modules. No release or download is available.

Windows integration now passes 40 CPU/component suites and a 1,820-check native
GPU fixture on an RX 9070 XT, plus 300 checks through the synchronous submission
entry point. R176 reconstructs 182 generic adapted files. Native sample counts
and GPU-clock reports replace host placeholders. GPU comparisons replace the
inherited forced-on conditional renderer, including explicit gated clears and
original counter-to-predicate chains; regressions verify accumulation,
resets, query retirement and both submission paths. Its
RGB565 correction and bounded BC3 integer block views pass authored GPU encoding,
face/mip readback and compressed-sampling regressions. Integer fragment outputs
are compiled offline and checked against actual render-target types. The original
game run confirms corrected cup colors and colored lighting cubemaps. Diffuse
lighting remains too dark in the selection menu. R164 now presents a native
Mario Kart Stadium intro with a lit Mario and kart; countdown, driving and full
gameplay stability remain under verification. The run reaches the player grid,
then stops on a command-memory arena bounds check. R166 also restores native
EQUAL depth semantics and disabled-bias behavior with GPU regressions. The latest original
game run passes point setup and reaches original counter requests before a
missing HDR-compositor stage. All 122 remaining common vertex/pixel stages now
translate and pass module creation; the private cache holds 7,365 stages.
Native fixed point sizes now reach checked vertex outputs, while programmable
shader sizes are preserved; authored GPU tests verify exact point coverage. A real one-layer
2D-array sampling regression covers the latest race-loading rejection. Raw and
converted snapshot regressions verify all six faces at two mip levels; earlier
HDR face reports read only face zero and carry explicit errata. Optional uniform
captures copy actual GPU-bound ranges and retire them after their fence, with an
authored GPU-write/offset regression. Actual MK8D startup reaches its owned window,
Vulkan device/queue, three presentation textures, command-buffer setup, native
save mounts and first texture upload. Copied texture/sampler descriptors now
compare using the inspected SDK 53 layout. The inspected 2D packaged texture layout and shared debug-label storage are
integrated. With the offline graphics/compute cache and verified local Mii system data,
startup starts the SDK53 software audio thread, loads sound banks, native system fonts, UI and RaceCommon assets, and serves the pinned application display-version metadata before continuing into further RaceCommon resources. Native temporary Mii generation is integrated with the original algorithm/table attribution and no emulator runtime dependency. Native presented-texture readback now shows the MK8 Deluxe startup logo after resolving copied render-target descriptors. Scripted native input selects a built-in Mii and reaches the main menu. Races and full rendering correctness remain unverified.

The adaptation adds an MK8D target, loads and initializes main/subsdk0/sdk,
uses an existing checked LZ4 implementation without installing a package, and
serves RomFS reads directly from a locally supplied read-only image. Its parser
checks metadata bounds, names, parent links, cycles and file extents. File reads
are serialized, and Windows denies concurrent writable opens of the image.
Synthetic tests cover reads, EOF, concurrent reads and malformed metadata.
Additional regressions exercise the actual ELF loader, domain transport, response
commit ordering and real owned Win32 window validation.

Unknown imports, unknown IPC services/commands, unhandled SVCs and NVN tracing
stubs stop execution. Other upstream handlers still need semantic review. Missing required offline
shader translations now stop execution instead of publishing an incomplete program.
The builder adapter explicitly supports the inspected SDK API 53.105 and reads
constant device properties from its compiled getter. Implemented hardware limits
come from Vulkan; optional unsupported features are not advertised. Other SDK ABI
details, object behavior, shader coverage and complete audio/input remain open.
Basic native save-file persistence is tested; gameplay save/restart is unproved. The generated function
reports do not prove complete instruction coverage or correct execution. This
experimental runtime has a different memory/dispatch model from the existing
guarded runtime; it does not inherit that runtime's ownership verification.

See the [offline shader diagnostic](offline/README.md) for the existing compiler
bridge and its current specialization limits.

## Prepare locally

Supply a checkout at the pinned upstream commit and an unused local output path:

```powershell
python integrations/belmante-runtime/prepare.py --upstream C:/src/SuperMarioWonderRecomp --output ./local/belmante-mk8 --vulkan-registry C:/deps/Vulkan-Headers/registry/vk.xml
python integrations/belmante-runtime/generate.py --prepared ./local/belmante-mk8 --exefs C:/own-dump/exefs
```

Generation requires an existing Python Capstone installation. Prepared source
uses the existing native Windows toolchain and Vulkan headers. Configure with
`-DNX_GAME=mk8d -DNX_TESTS=ON`, `-DNX_GEN_DIR=<prepared>/private/gen` and
`-DNX_VULKAN_SDK=<installed Vulkan headers root>`. Limit compilation to two jobs
in total and avoid other compiler work concurrently. Run CTest after building.
Preparation regenerates `vk_gen.h/.cpp` using the supplied registry. It must
match the installed headers; verification here used version 1.4.345. No additional
Vulkan library is installed. `NX_PREBUILT_MODULE_DIR` is only for local reuse
when generated inputs and runtime headers are hash-verified against the build
that produced those libraries. A normal build compiles the generated modules.

The optional `native_sdk_mii_probe <private-analysis-directory>` executable checks
the generated native Mii layout using the supplied SDK's statically compiled
verifier and parameter accessor. It loads private module images but does not run
title entry or initialize a GPU. Its inputs must match the generated libraries.

Keep generated code, module images, builds and game data under ignored local
paths. `NX_DATA_ROOT` selects the local directory containing `analysis/`;
`NX_ROMFS_IMAGE` selects the supplied RomFS image. `NX_NPDM` and
`NX_NPDM_SHA256` select and pin its matching metadata (recorded by generation).
Optional `NX_MII_ROMFS_IMAGE` and `NX_MII_ROMFS_SHA256` select a privately supplied,
verified system-data image. No system contents or keys are distributed.
`NX_NACP` and `NX_NACP_SHA256` supply and pin the matching private application control metadata.
`NX_NACP` and `NX_NACP_SHA256` supply and pin the matching private application control metadata.
`NX_FONT_BUNDLE` and `NX_FONT_BUNDLE_SHA256` select and pin the privately prepared native font bundle.
`NX_SHOW_INACTIVE=1` creates the owned window without foreground activation;
focus policy parks hidden/minimized execution.
`native_device_fixture` runs CPU rejection checks; its explicit `--gpu` mode
creates and drains a native Vulkan device without loading guest modules. Use fresh output/state paths.
Native startup, live rendering and gameplay must be verified before these
instructions can be treated as a working-game build guide. The project's full
native release gate remains unchanged.

The R132 adaptation selects the SDK's already registered software AVC component
for generic AVC decoder requests using the original String8/vector APIs. A
fresh saved-profile run creates that real codec and renders the native main menu,
but repeated scripted A presses do not advance it. Decoder configuration, decoded
video and races remain unverified. No development binary or download is published.

The Windows native AVC bridge uses built-in Media Foundation through the original
H264SwDec API contract. Its private-input production probe checks 32 decoded
pictures, SDK picture identity, NV12/planar conversion, draining and mapped-memory
revocation (309 checks). A full 13,200-picture test checks pixel-hash parity across
default and interactive decoder modes. These receipts remain local. No codec
package is installed, and no movie data is included in this recipe. This bridge
has not established video presentation or gameplay in the full game yet.

The R137 game probe decodes two pictures and retrieves an SDK output buffer before
stopping at the missing native `nvnCommandBufferCopyBufferToTexture` upload. This
is not a rendered-video or gameplay result. The native GPU upload is the next gate.

API references: [Windows H.264 decoder](https://learn.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder),
[interactive decoder mode](https://learn.microsoft.com/en-us/windows/win32/medfound/codecapi-avlowlatencymode),
and [original decoder API contract](https://android.googlesource.com/platform/frameworks/av/+/0c1bc74/media/libstagefright/codecs/on2/h264dec/inc/H264SwDecApi.h).

The R146 source adds native buffer-to-texture transfers with checked source pools,
copy regions and copied SDK descriptors. Native GPU fixtures verify submitted CPU
data, earlier GPU writes and partial updates. Graphics V10 profiles admit checked
3D float samplers. Complete API RGBA8 volumes and the bounded 8x8x8 packed RGB11F
lookup storage use native upload/sampling paths; authored tests check every voxel,
float conversion and distinct depth samples.

Wait cancellation now remains pending until a blocking synchronization consumes
it. A deterministic pre-wait test failed on the old kernel and passes after the
fix, alongside ready-object/poll precedence and cancellation of a blocked host
thread. The native GPU fixture now passes 461 checks and all 36 suites pass.
These results do not establish native movie presentation or races. The current
full-game probe is checking that final video handoff without diagnostic readbacks.

The R146 original-game run now presents a changing movie preview and native 3D
character/kart/glider and cup-selection menus. The movie has visible color
corruption and the 3D character lighting is dark. Those defects and race loading
remain open; this is not a playable-game or release claim.

R148 corrects the observed native movie layout mismatch: source color format 19
is planar, while requested movie format 4 needs NV12. The original SDK getters
provide real geometry and writable caller-buffer bounds. Chroma is interleaved
without changing luma, decoder parameters, underlying codec metadata or returned
SDK error status. Authored canary/rejection tests and a complete real captured
frame conversion pass. The live upload Y and UV planes match canonical NV12
exactly, and the native preview's prior green/magenta corruption is resolved in
the reviewed frame. All 37 suites and 479 native GPU checks pass; dark 3D lighting
and race rendering remain open. No download is published.
