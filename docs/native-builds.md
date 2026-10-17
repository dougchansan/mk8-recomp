# Native foundations build checkpoint

This Windows x64 v6 prerelease contains 34 native CPU/service, window, texture and
Vulkan verification tools. It does not boot or play Mario Kart 8 Deluxe. Actual title code and data
are supplied locally; no game-linked executable is distributed.

The source checkpoint applies to suyu commit
`347fcf91d95cd39cb5c3db50f1b6d827ad699311`.
`native-checkpoint/source.json` records every changed file and the patch hash.
The release's `BUILD.json` records binary hashes and fresh verification commands.
The base source archive, complete patch, synthetic fixtures, dependency source
and license notices accompany the tools.

## Run the tools

Use Windows x64 with the Microsoft Visual C++ runtime. Vulkan tools also require
a Vulkan-capable GPU and its installed driver. They load the system Vulkan
loader; no emulator, game, keys or firmware is required.

From the extracted release directory:

```powershell
.\bin\native_platform_test.exe .\fixtures\platform
.\bin\native_primitive_test.exe clock
.\bin\native_ipc_test.exe
.\bin\native_event_test.exe
.\bin\native_wait_test.exe
.\bin\native_services_test.exe
.\bin\native_services_event_test.exe
.\bin\native_services_lifecycle_test.exe
.\bin\pending_test.exe
.\bin\native_pending_host_test.exe
.\bin\native_guest_callback_test.exe
.\bin\fence_test.exe
.\bin\memory_test.exe
.\bin\native_ownership_memory_test.exe
.\bin\native_ownership_memory_integration.exe
.\bin\native_ownership_accounting_test.exe
.\bin\native_graphics_probe.exe
.\bin\native_graphics_resource_test.exe
.\bin\native_nvn_resource_test.exe
.\bin\native_nvn_draw_test.exe
.\bin\native_nvn_frame_test.exe
.\bin\native_nvn_state_test.exe
.\bin\native_nvn_feature_format_test.exe --cpu-only
.\bin\native_nvn_feature_format_test.exe --gpu
.\bin\native_nvn_presentation_policy_test.exe
.\bin\native_nvn_presentation_conversion_test.exe
.\bin\native_nvn_presentation_fixture.exe --visible --frames 45 --exercise-resize
.\bin\buffer_init_test.exe
.\bin\native_texture_test.exe
.\bin\window_test.exe
```

The offscreen graphics tests verify their pixels. The presentation fixture
creates an owned window and displays GPU-produced RGBA8 and packed10 images;
it requires actual swapchain-maintenance1 support and reports unsupported
configurations explicitly. Its source stays on the GPU through presentation.
The final SDR8 display conversion quantizes packed10 output. The
window test briefly creates owned diagnostic windows and records real OS state;
it does not establish game input, rendering or foreground activation.
The feature/format fixture generates all inputs in memory. Its default and
`--cpu-only` modes run 148 CPU checks without creating a Vulkan device. Explicit
`--gpu` runs 16 authored draws with 549 pixel comparisons, covering BC4,
packed sRGB, swizzles, independent depth/provoking state, full uniform blocks,
blending and exact preservation of masked and untouched pixels. Computed colors
use documented UNORM8 tolerances; breakpoint-adjacent sRGB samples do not prove
floating-point precision. This mode requires the seven explicitly requested
features and fails on unsupported hardware. Custom-border checks cover creation,
metadata and capacity, with no ClampToBorder sampling claim. Explicit image-view
usage requires usable Vulkan 1.1 or later. The full declared uniform-block extent
is required even with robust buffer access enabled.
`native_texture_probe DESCRIPTOR32 STORAGE TEXELS JSON` converts caller-supplied
bounded texture data into unused output paths. Its output pair is not transactional:
a later failure can leave the first newly created file, while existing files are
preserved. `native_admit` and `native_nvn_pipeline_check` are tools
for caller-supplied inputs; a successful diagnostic is separate from gameplay.

## Prepare the source

Use Python 3.10+, Git, CMake 3.22+ and Visual Studio C/C++ build tools. Source
preparation creates a new worktree and verifies the resulting file hashes. It
preserves the original checkout and requires an unused output directory.

```powershell
git clone --no-checkout https://github.com/dougchansan/suyu-v0.0.4 suyu-source
git -C suyu-source fetch origin 347fcf91d95cd39cb5c3db50f1b6d827ad699311
python scripts/prepare-native-source.py --source ./suyu-source --output ./native-source
```

For the extracted release, run `prepare-native-source.py` from its root instead
of `scripts/prepare-native-source.py`. `--verify-only` checks the packaged patch
without creating a worktree. Source preparation needs no game inputs.

## Rebuild and verify the public tools

The packaging command requires existing Vulkan-Headers v1.4.345, Sirit
v1.0.5 Windows x64 and fmt12.1.0 header installations. It downloads nothing. Point `--vulkan-include`
at the directory containing `vulkan/vulkan.h`; point `--sirit-root` at the
installation containing `include` and `lib/sirit-static.lib`.
Point `--fmt-root` at its `include` and `LICENSE`; the texture component links no
fmt binary. The package carries these headers/license and the existing BC decoder's
MPL-2.0 notice and corresponding source in the base-source archive.

Run in a compiler-enabled PowerShell, substituting your dependency paths:

```powershell
python scripts/package-native-checkpoint.py --source ./native-source --build ./local/native-checkpoint --output ./local/native-foundations-windows-x64 --vulkan-include C:/deps/vulkan/include --sirit-root C:/deps/sirit --sirit-license C:/deps/sirit-source/LICENSE.txt --sirit-source-archive C:/deps/Sirit-v1.0.5-source.tar.gz --fmt-root C:/deps/fmt
```

The command verifies the checkpoint, builds with two jobs, runs the synthetic
CPU, service, entry/API v2/v3, estimate, window, texture and graphics tests, then packages only an
explicit list of tools and generated synthetic fixtures. Failed tests prevent
package creation. The build directory retains detailed logs. A new output
directory is required for each package.
Serialize the complete package command against other compiler jobs when sharing
a two-job budget. Packaging also runs GPU and owned-window tests, including the
explicit feature/format `--gpu` command; a successful CPU-only invocation does not
substitute for hardware coverage. `BUILD.json` records that executable's hash
and parsed draw/pixel results.

Sirit's tagged source is available at
[`eden-emulator/sirit` v1.0.5](https://github.com/eden-emulator/sirit/tree/v1.0.5),
commit `b870b062998244231a4f08004d3b25151732c5c5`. Vulkan headers are available
at [`KhronosGroup/Vulkan-Headers` v1.4.345](https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.4.345).
The release includes GPL-3.0 license texts and the applicable header notices.

This verifies a fresh checkpoint build using the existing dependency
installations on the development host. Clean-machine setup, other compilers,
other operating systems and different GPU drivers remain unverified. Shader
capture tooling in the patch still depends on separately supplied, matching
shader-recompiler libraries; it is not part of this binary package.

The earlier v5 checkpoint added reviewed callback transport, per-image execution fences,
whole-instruction memory preflight, central opaque-object permissions and strict
Host accounting. It had 167 source files and preserved the 17 reviewed v4
presentation files literally. V6 has 168 source files: it updates 14 generic
graphics/NVN/shader files and adds one authored feature/format fixture. Window
and Presenter implementations retain their v5 bytes. The synthetic ownership tests cover 106 instruction cases,
2,544 strict guard mutations, 15,524 real emitted integration checks and 4,102
accounting checks in each chain mode. Guest authority is restricted to runtime
bus callbacks; native adapters need explicit grants. Lifetime grant storage is
bounded to 65,536 allocations per address space. Strict ownership currently uses
one serialized context/owner lane and disabled raw fast-memory shortcuts.

These opt-in fences require regenerated, attested images; old compiled code
cannot acquire their protection by replacing the host alone. RuntimeC and
GuestContext layouts and default emitted behavior remain unchanged. Callback
pointer extents remain limited to 64 KiB; the actual allocator's larger heap
argument is a separate integration gate. Combined pending-provider work
accounting remains separate from the verified synchronous accepted-call ledger.

The existing presentation fixture uses GPU-produced inputs without source
readback. Validation layers were disabled. Safe WSI teardown can wait for the
driver. The emitted and CPU-owner
v3 suites run with FM1/GG1/FPX1 using synthetic inputs. Real native DeviceInitialize,
live game-driven frames and gameplay remain open. Captured game draw/presentation
diagnostics and real allocator AOT are outside this release. Earlier v5, v4,
v3 and v2 releases remain separate immutable archives.
The development checkout records verified local binaries in the gitignored
`local/native-builds/latest.json` index. That index distinguishes public tools,
the game reference using reference graphics, and the standalone CPU diagnostic.
Game-linked binaries require local title inputs and are not uploaded.

The public checkpoint excludes the private exact-copy-loop experiment and all
title-specific descriptors, generated game code, shader caches, captures and
game-linked libraries. See [progress](native-runtime-progress.md) for the wider
native campaign and its remaining completion gates.

## Verified local native heap fixture

The ignored `local/native-builds/latest.json` index now includes
`verified-native-heap-cpu-diagnostic`. Its `path` points to the retained Windows
x64 `native_heap_test.exe`; run it without arguments from its recorded working
directory. The indexed copy has SHA256
`b6799a20db73460f310c04258fa4eecd96241508695943dd8ab6934d869f45b2`.

The reviewed run completes two separate actual 3,100 MiB allocations with full
Windows committed-prefix/reserved-tail checks, plus SVC, budget, growth,
shrink/regrow and ownership cases. It belongs to the eleven-suite CPU result;
its observed 2,221 checks include memory-map inventory checks whose count can
vary between processes. It creates no Window, executes no game/NSO code and
performs no GPU work. Closed logs and review receipts are retained beside the
binary. It is a private local verification tool and is excluded from public
release assets and playable-game claims.
