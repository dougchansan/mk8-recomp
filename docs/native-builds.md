# Native foundations build checkpoint

This Windows x64 v4 prerelease contains 27 native CPU/service, window, texture and
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
.\bin\native_graphics_probe.exe
.\bin\native_graphics_resource_test.exe
.\bin\native_nvn_resource_test.exe
.\bin\native_nvn_draw_test.exe
.\bin\native_nvn_frame_test.exe
.\bin\native_nvn_state_test.exe
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

This v4 checkpoint adds reviewed native presentation to the v3 scheduled API
transport, native event waits, window observer, typed persistent images and
texture converter. All 27 tools were rebuilt from sanitized v3 source plus exactly
17 reviewed presentation files. Seven native graphics suites passed; the fresh
visible fixture queued and retired 45 presents, recreated three times and performed
zero source readbacks. Root inspection verified the two owned-client captures.
Validation layers were disabled. Safe WSI teardown can wait for the driver.
The emitted and CPU-owner
v3 suites run with FM1/GG1/FPX1 using synthetic inputs. Real native DeviceInitialize,
actual-game draw/presentation and gameplay remain unproved. Later callback and
execution/memory ownership work is outside this release. Earlier v3 and v2 releases
remain separate immutable archives.
The development checkout records verified local binaries in the gitignored
`local/native-builds/latest.json` index. That index distinguishes public tools,
the game reference using reference graphics, and the standalone CPU diagnostic.
Game-linked binaries require local title inputs and are not uploaded.

The public checkpoint excludes the private exact-copy-loop experiment and all
title-specific descriptors, generated game code, shader caches, captures and
game-linked libraries. See [progress](native-runtime-progress.md) for the wider
native campaign and its remaining completion gates.
