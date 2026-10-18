# Belmante runtime integration for MK8D

This experimental adaptation uses the MIT-licensed
[SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp)
runtime and AArch64-to-C++ recompiler, pinned in `UPSTREAM.json`. Preserve
`LICENSE.upstream`; local modifications are GPL-3.0-or-later. The recipe contains
no game code, assets or generated modules. No release or download is available.

Windows integration now passes 29 CPU/component suites and a 118-check native
GPU fixture on an RX 9070 XT. Actual MK8D startup reaches its owned window,
Vulkan device/queue, three presentation textures, command-buffer setup, native
save mounts and first texture upload. Copied texture/sampler descriptors now
compare using the inspected SDK 53 layout. The inspected 2D packaged texture layout and shared debug-label storage are
integrated. With the offline graphics/compute cache and verified local Mii system data,
startup starts the SDK53 software audio thread, loads sound banks, native system fonts, UI and RaceCommon assets, then stops at display-version metadata. No native game frame is established.

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

Keep generated code, module images, builds and game data under ignored local
paths. `NX_DATA_ROOT` selects the local directory containing `analysis/`;
`NX_ROMFS_IMAGE` selects the supplied RomFS image. `NX_NPDM` and
`NX_NPDM_SHA256` select and pin its matching metadata (recorded by generation).
Optional `NX_MII_ROMFS_IMAGE` and `NX_MII_ROMFS_SHA256` select a privately supplied,
verified system-data image. No system contents or keys are distributed.
`NX_FONT_BUNDLE` and `NX_FONT_BUNDLE_SHA256` select and pin the privately prepared native font bundle.
`NX_SHOW_INACTIVE=1` creates the owned window without foreground activation;
focus policy parks hidden/minimized execution.
`native_device_fixture` runs CPU rejection checks; its explicit `--gpu` mode
creates and drains a native Vulkan device without loading guest modules. Use fresh output/state paths.
Native startup, live rendering and gameplay must be verified before these
instructions can be treated as a working-game build guide. The project's full
native release gate remains unchanged.
