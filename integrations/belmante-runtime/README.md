# Belmante runtime integration for MK8D

This experimental adaptation uses the MIT-licensed
[SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp)
runtime and AArch64-to-C++ recompiler, pinned in `UPSTREAM.json`. Preserve
`LICENSE.upstream`; local modifications are GPL-3.0-or-later. The recipe contains
no game code, assets or generated modules. No release or download is available.

R212 now passes a clean 24,000-target flight from Stadium's first-place/15-point
results through native screenshot readback to Water Park driving, underwater
anti-gravity and a completed first lap (HUD 2/3). The full second race/cup is
unverified. A same-binary control without frame signatures and with captures
every 1,800 presentations observes about 38.5 FPS in its course window; separate
flights and changed diagnostics do not establish sustained 60 FPS. The optional
chunk-512 path reaches the unchanged 6 GB guard and remains disabled.

R213 adds opt-in replay versus presentation/non-replay attribution to existing
GPU timestamp results, without new queries/waits or synchronization changes.
Both changed sources have clean analyses and the full component/GPU/counter
controls pass. A 3,600-presentation course flight passes with class totals
matching the aggregate; median replay is 18.06 ms/frame versus 0.05 ms/frame
for presentation/non-replay. These intervals include synchronization/scheduling.

R223 prefers coherent CPU-cached memory for diagnostic image downloads and adds
explicit transfer-to-host dependencies. The old coherent memory selection remains
available with `NX_CACHED_READBACK=0`. Authored GPU-written full-HD pixel tests,
native captures and the full component/GPU/counter controls pass. Upload and native
TextureReadTexels allocation/lifetime remain unchanged.

R221 permits concurrent stable focus checks while preserving per-call ownership,
visibility and exclusive state transitions. R222 shares positive texture identity
entries across copied descriptors, with full field/type/generation revalidation;
`NX_TEXTURE_IDENTITY_SHARED_CACHE=0` selects the original cache control. Both have
authored concurrency/lifetime tests and passing full GPU/counter controls. Native
gameplay validation continues; microbenchmark ratios are not whole-game FPS gains.

R220 adds Windows high-resolution per-thread sleeps (`NX_HIGH_RES_SLEEP=0`
retains the old comparison path), opt-in actual caller delivery/CPU/wait/pacing
statistics (`NX_FRAME_PACING_STATS=1`), and an optional existing local driver-cache
directory (`NX_PIPELINE_CACHE_DIR`). Deadlines, GPU waits and simulation timing
stay intact. Timer overshoot falls sharply; overall racing rates remain variable
and sustained smooth 60/120 are unverified. See the performance document for the
same-executable controls and their limitations.

R218 enables exact sample-query grouping by default, preserving reset/report,
conditional, retirement and clear-exclusion behavior. Default/grouped 2,327-check
GPU controls and 40 suites pass; the hardened R217 long flight verifies a Stadium win and Water Park lap.
`NX_COALESCE_SAMPLE_QUERIES=0` selects the legacy per-draw diagnostic control.
`NX_COUNTER_QUERY_STATS=1` reports query/draw counts in existing timing windows.

R201 now completes a native three-lap Mario Kart Stadium race, with the FINISH
banner and first-place/15-point results reviewed. The 18,000-frame flight closes
without a fatal error; its 11,280-frame racing window averages about 25 FPS
(26.5 ms median interval), profiling disabled with sparse readbacks. Native save
commit and exact game-produced-save reload to mode selection are verified.
Audible audio, physical controls, broader gameplay and full-speed performance
remain open; no release/tag/download is available.

Windows integration now passes 43 CPU/component suites and a 3,212-check native
GPU fixture on an RX 9070 XT, plus 300 checks through the synchronous submission
entry point. R248 reconstructs 207 generic adapted files. Optional pipeline
recipes (`NX_PIPELINE_RECIPES=1`/`capture`, off by default) prewarm the graphics
pipelines a previous local flight used on bounded background workers; no recipe,
shader or driver cache is published. See `docs/native-pipeline-recipes.md`. Native worker creation
handles are released, thread descriptions are applied and concurrent priority
metadata access is synchronized. `NX_NATIVE_THREAD_PRIORITY=1` enables an
experimental bounded Windows priority mapping; it remains off by default after
the game controls showed no demonstrated FPS gain. Exact 16-byte counter
bindings avoid an unnecessary 64 KiB allocation tail; authored GPU tests verify
later padded reads preserve the result and zero tail. A stronger timestamp-record
dependency passes repeated exact hardware-query controls. Pool resource release
and chunked submission remain experiments; both are off by default. Native sample counts
and GPU-clock reports replace host placeholders. GPU comparisons replace the
inherited forced-on conditional renderer, including explicit gated clears and
original counter-to-predicate chains; regressions verify accumulation,
resets, query retirement and both submission paths. Its
RGB565 correction and bounded BC3 integer block views pass authored GPU encoding,
face/mip readback and compressed-sampling regressions. Integer fragment outputs
are compiled offline and checked against actual render-target types. The original
game run confirms corrected cup colors and colored lighting cubemaps. Diffuse
lighting remains too dark in the selection menu. R164 now presents a native
Mario Kart Stadium intro with a lit Mario and kart. R182 passes the old arena
stop and reaches countdown/driving; R187 reaches 6,000 frames without a fatal
error, including anti-gravity and the glider jump. R188 verifies the first
complete native lap with a clear 2/3 HUD at frame 6,420. A transient glider flash
needs reference comparison; broader race/cup coverage, full-speed performance and full
gameplay stability remain open.
R188 makes repeated routine NVN messages opt-in with `NX_TRACE_NVN_CALLS=1`.
R202 also verifies real XAudio2 processing silently with zero source-voice gain
(`NX_MUTED_AUDIO_DEVICE=1` plus `NX_MUTE=1`), including original game PCM and actual
completion callbacks. Default mute/headless behavior is preserved. The generated
10-second stereo capture is private; full-race device coverage passes with 129,992 observed completions and first-place
results. Subjective listening and physical controls remain separate gates. Optional pipeline-2/chunk-150 mode hits the memory guard
and is not promoted; the verified race configuration is async 2 / pipeline 0.
R205's actual repeat completes the race and first-place/15-point results. Result
advance then needs CPU texture readback. R207 supplies a bounded native GPU
readback for uncompressed 2D color storage, standard and strided, preserving
output canaries and waiting for actual GPU completion. Authored GPU pixels,
pending writes and rejection tests pass. Actual game readback returns a 480×270
RGBA8 image (518,400 bytes) and reached loading in R207, before that flight
stopped on a descriptor lifetime failure. R212 now verifies Water Park driving. R210/R212 add regression-backed stable
positive resolution within a replay and handle-cache expiration on replay serial,
including when the recording epoch repeats. The clean two-course R212 flight
passes with those fixes. A separate synchronous readback control passes 259 checks.
R205 also avoids retaining empty descriptor-handle lookups across a replay:
a late registration can now resolve its retained texture snapshot. The unchanged
runtime fails the focused regression; the correction passes native sampled-view,
GPU pixel and replay-visibility checks. A repeat first-race failure shows that
the earlier successful races do not establish full gameplay stability. The actual repeat reaches first-place results; the next unsupported readback
operation is addressed in R207 and transition validation continues.
R201 keeps the first positive post-mark texture incarnation stable through one
replay, including after finalization/cache eviction; misses and later replays
retain their current-state behavior. Authored native-view/red-pixel regressions
fail before correction and pass afterward, including with the fast cache off.
The actual full-race flight now passes, as recorded above.
R194 adds unique-match retained-snapshot admission hints with final-reference
retirement and unchanged full validation; its 900 profiled race frames average
about 21 FPS. Full-speed gameplay and broader coverage remain open.
R166 also restores native
EQUAL depth semantics and disabled-bias behavior with GPU regressions. An earlier original
game run passed point setup and reaches original counter requests before a
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
startup starts the SDK53 software audio thread, loads sound banks, native system fonts, UI and RaceCommon assets, and serves the pinned application display-version metadata before continuing into further RaceCommon resources. Native temporary Mii generation is integrated with the original algorithm/table attribution and no emulator runtime dependency. Native presented-texture readback now shows the MK8 Deluxe startup logo after resolving copied render-target descriptors. Scripted native input selects a built-in Mii and reaches the main menu. Full rendering correctness and broader race/cup coverage remain unverified.

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
