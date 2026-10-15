# Native runtime progress

Updated 2026-10-15. Repository: `dougchansan/mk8-recomp`. Goal: a standalone title-specific native application with static CPU code and native graphics, platform, audio, input and storage. Completion requires the supported gameplay path to run without CPU JIT, Maxwell/emulated GPU processing or the general emulator runtime.

The campaign is in component bring-up, before the first end-to-end native game
draw. Static CPU execution and synthetic native rendering/presentation work.
The immediate title-integration gates are genuine allocator callbacks, native
Device creation and authoritative resources for an actual game draw. Native
audio, controls, save/restart and full standalone gameplay follow those gates.

## Verified foundations and their limits

| Evidence | Verified result | Limit |
|---|---|---|
| Native NVN builders in the actual-game reference replay | 90,320 native calls; 6,001-frame replay reached EOF; CPU JIT 0, imports 0; observed racing | Still uses reference graphics. This is not standalone native gameplay. |
| Native Vulkan triangle fixture | 1,152 red pixels and 2,944 black pixels | Synthetic rendering; no actual-game draw/present. |
| Textured native fixtures | Five draws, 20,480 pixels checked and ten rejection cases | Synthetic resource/draw coverage only. |
| Native-owned pool copy | 64-byte copy and 29 error cases | Bounded memory ownership/validation test. |
| Persistent native images | Three-draw producer/consumer fixture checks 4,096 pixels and 71 rejection cases; retained native image is sampled directly | Synthetic Stage A. No intermediate target readback/reupload; actual-game state and content history remain required. |
| Standalone actual CPU execution | Native SM/LM/APM and applet/domain bootstrap reach a real CommonStateGetter event. Actual Win32 inactive observation produces message15 and focus query2; guest then parks at 12,600,537 blocks / 77 SVCs. Deadline cancellation is accepted | No genuine active-focus transition, later service coverage, graphics or gameplay claim. Parked state and cancellation are distinct from guest exit. |
| Public source/API tests | Synthetic API v2 identity/sealed-binding negotiation, full-range guards and bounded BufferInit checks pass | The miniature API fixture does not attest a complete actual title module or establish every required NVN contract. |
| Scheduled native API v3 | Nine return kinds, cancellation/commit races, stale completion and no-fallback behavior pass synthetic tests; emitted transport and CPU owner pass ordinary/chained FM1/GG1/FPX1 suites; 144 strict guard mutations reject | Real DeviceInitialize remains gated on provider effects, registered allocator semantics and ownership isolation. |
| Typed native renderer | Five suites pass, including 76 draws / 306,176 pixels / 66 rejection checks and 4,096 CPU packing roundtrips. Independent alpha blending, float2/3 vertices, swizzles/sRGB, filtering, inverted viewport and packed10 producer/sampling preserve declared semantics | Synthetic capabilities. Actual-game shader/state/resource/history and presentation remain separate gates. |
| Native texture conversion | 210 independent tiling cases and CLI checks pass; five recorded source textures decode with matching hashes after dependency simplification | CPU backing is not authoritative for all GPU writes. GPU-produced target contents are excluded; no frame-equivalence claim. |
| Native window observations | Real owned Win32 window, bounded pump/wait, overflow/close handling and owner lifetime pass strict compiler analysis and tests; actual inactive guest lifecycle exchange verified | No positive foreground activation, native input or rendering proof. |
| Owned shader-input construction | Explicit source/metadata factory shares the offline translator; synthetic ownership/rejection and authored-cache equivalence checks pass | Source-only in this package. Structural acceptance does not attest complete actual shader provenance or raster state. |
| Real shader translation and pipeline acceptance | Actual shader translation and pipeline acceptance verified separately | No actual-game native draw or presentation established. |
| Native Win32 presentation | Fresh public build passes seven native graphics suites and an owned-window fixture: 45 queued/retired presents, three swapchain recreations, RGBA8 and packed10 sources, zero source readbacks; both cropped client images inspected | Synthetic presentation. Requires enabled maintenance1 support; validation layers disabled. Driver retirement can block teardown. Actual-game dispatch remains pending. |
| Guest callbacks and execution fence | Independently reviewed owner callback transport, per-image original/native approval, all-entry checks and unchanged default output; FM0 and FM1/GG1/FPX1 ordinary/chain tests pass | Serialized single-owner scope. Actual Device allocator routing and native Device creation remain closed. |
| Opaque-object memory and accounting | Independently reviewed 106-case preflight; 15,524 emitted integration checks and 4,102 accounting checks per chain mode. Full CPU/callback/pending regressions and seven production/three test analysis units pass | Protects native storage from guest accesses, including aliases and split instructions. It does not implement the native Device or a general concurrent scheduler. |
| Real SDK allocator initialization | Independently reviewed constructor: 408 checks across fresh and negative cases. Genuine 64 KiB supplied-heap initialization: 470 checks, with 105 callback blocks and 266 checked memory operations per successful run; parent context and heap boundary canaries preserved; zero SVCs/provider commits/GPU work | Earlier private static CPU diagnostic through an authored callback driver. Title-sized heaps and real Device callback routing remain unproved. |
| Genuine SDK bootstrap and raw allocator pair | Independently reviewed 1,934 checks across ten cases. Two fresh processes execute SDK OS initialization, constructor, 64 KiB cache-disabled Initialize, one raw allocation of 8,199 bytes at 16-byte alignment, exact Free once and Finalize with an empty live ledger. Each bootstrap uses 16 native queries; allocator phases use zero SVCs, provider commits or GPU work | Private CPU fixture. Small classes, multiple live allocations, 24 MiB heaps, general TLS, contention and Device integration remain separate gates. |
| Combined fenced pending-work accounting | Independently reviewed synthetic integration passes 1,094 checks and 98 observations in each ordinary/chain mode. Actual accepted calls, guest blocks, copied-input worker units and commits remain distinct through cancellation, revocation and stale completion | After-v5 private fixture with one admitting image/provider and one operation per binding. It does not establish general multi-provider attribution or Device/game integration. |

Synthetic fixtures are deliberate tests of implemented behavior. Their success cannot substitute for game rendering, service coverage, audio, input or persistence.

## Actual-game capture status

Capture 2 is diagnostic evidence: 31 draw records and a first submission were observed. The old harness aborted on a foreign-job check, so that capture is **not a replay pass**.

Capture 3 finished the bounded 6,001-frame reference replay with exit 0 and no harness invalidations. Its final frame shows Stadium racing. It captured 237 graph nodes, five submitted draws with eligible snapshots, ordered uniform updates and 44,230,424 resource bytes; observer exceptions and unmatched returns were zero. Resource/shader/texture interpretation still needs validation, so the capture reports `replay_ready=false`. These are inputs for native rendering; the successful replay still used the reference GPU path.

Capture 4 also completed 6,001 frames with exit 0 and no harness invalidations. Its new bounded sidecar verifies the live default device provider, flags 0, successful reference initialization and five actual texture-bank setup packets. The 512-event cap was reached after setup, so it does not establish complete later query/reinitialization coverage or native device initialization.

Capture 5 r2 reached EOF but exceeded its shutdown bound. It remains a failed capture; its progress records are not final status. The corrected r3 build completed 6,001 frames with exit 0 and no invalid conditions. It captured two closed 1920x1080 packed10 target pairs around selected producer draws and independently matched shader bytes, program offsets, viewport state and source prefixes. The before/after pairs contain 2,073,600 and 385,640 changed packed pixels. They do not prove first-producer history, a consumer, sampled texture/UBO authority or native rendering equivalence.

The r3 allocator witness found installed title hooks and sampled cache mode zero at both Device entry and matched return. It observed no selected TLS-cache callback. This narrows the next analysis to the cache-disabled CentralHeap route; live lock/free-span state, complete helper closure and paired allocation/cleanup semantics still require proof. These private game-linked diagnostics are excluded from release assets.

Capture 6 completed a fresh 6,001-frame reference replay with exit 0, no harness
invalidations, zero JIT and zero unresolved imports. Independent review approves
bounded input authority for one selected draw: four buffers and two sampled
images totaling 5,530,476 bytes, plus a closed packed10 destination pair from
before and after that draw. Payload hashes, actual descriptors/views/samplers,
physical allocation separation and shader/pipeline provenance match. Both
recorders reached their drained terminal state; the final racing image was
inspected. The two destination snapshots differ at 2,073,600 packed pixels.

This private observer retains both cache locks and all copy resources through
one final GPU drain. The mapped uniform inputs use the narrowly reviewed
host-coherent permanent stream with stable owner, range and tick; other mapped
backings are declined. The enabled build preserves class layouts and replaces
exactly four host video-library members; all other host members and all five
guest/runtime archives match the previous reference build.

These are authoritative reference inputs for a bounded native draw replay.
The capture itself uses reference graphics and establishes no native draw,
frame equivalence, complete producer history or native gameplay. The next step
is to bind the captured inputs and inherited destination contents through native
Vulkan while preserving the observed shader and raster semantics. The public
v5 assets remain unchanged; game-linked captures and diagnostics stay private.

The next private CPU fixture now starts with genuine SDK OS initialization.
The native kernel supplies its registered main-thread handle, actual OS thread
identity and BCrypt entropy; original SDK code writes the thread, TLS, lock,
once and heap structures. The bootstrap argument is a scalar thread handle.
Static call adapters preserve the callback owner's register contract, and
phase-specific instruction and whole-memory gates limit execution and access.

The independently reviewed ten-case proof passes 1,934 checks, including two
fresh successful allocation/free/finalize sequences, six rejection routes and
cancellation before allocator effects. Completed child ledgers are parsed and
hashed after process exit. A separately copied runnable bundle also passes a
fresh 242-check allocation pair. Payload, canaries and the full parked parent
context survive. The earlier constructor and initialization proofs remain
preserved; their block counts use a different root inventory and should not be
compared with the new per-instruction admissions.

This closes a bounded raw allocation pair on a 64 KiB cache-disabled heap.
Small allocation classes, multiple live allocations, general ELF/dynamic TLS,
contention, post-allocation failure recovery, the 24 MiB title-sized heap and
native Device creation remain unproved. Actual Device callback scheduling and
the larger-span capability remain integration gates. These game-linked CPU
diagnostics and their inputs stay private and are indexed locally; they are
outside the immutable public v5 binaries and verification manifest.

Decoded draw state establishes source-alpha blending, a destination-multiplication pass, float2/float3 vertices, linear repeat/clamp sampling and a negative-Y viewport transform. There is no Clear in the submitted graph, and full destination-independent overwrite is unproved. A faithful native replay requires the earlier attachment contents or a complete overwrite proof; an invented black target is not a valid substitute.

The next graphics gate is a faithful actual-game draw/submission/presentation path with complete inherited state and resources. Native audio, input, saves and overall gameplay closure remain pending. A functioning static CPU or native Vulkan fixture alone does not close those gates.

## Public checkpoint and prerelease

The checkpoint targets suyu source revision:

```
347fcf91d95cd39cb5c3db50f1b6d827ad699311
```

Source changes are represented by `src/patches/native-runtime.patch` against that exact base. The 167-file checkpoint includes the reviewed ownership/callback source and preserves the 17 v4 presentation files. Synthetic CPU, service, entry/API v2/v3, estimate, callback, fence, memory and accounting checks are recorded alongside graphics/window validation. [Verification](../native-checkpoint/verification.json) records the public binaries and source correspondence. These notes do not claim a verified setup on a fresh machine.

The [prerelease `native-foundations-v5-20261015`](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v5-20261015) contains 33 Windows x64 verification tools, synthetic fixtures and corresponding source. It is **not a playable MK8D build**. It adds the reviewed callback and ownership layers to native presentation, scheduled API transport, event waits, typed persistent images and texture conversion. The shader-input factory is source-only; matching offline compiler libraries, real allocator AOT and game-linked diagnostics are excluded from release binaries. Earlier [27-tool v4](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v4-20261014), [24-tool v3](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v3-20261014) and [13-tool v2](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-20261014) checkpoints remain immutable.

The development checkout also keeps a gitignored `local/native-builds/latest.json` index with exact public-package and private diagnostic executable hashes. Game-linked CPU/reference builds remain local; only tools with synthetic inputs and publishable corresponding source become release assets. See [build instructions](native-builds.md) for the current public package.

The public checkpoint excludes the private exact copy32 matcher and its tests because their provenance is unsuitable for this checkpoint. That omission does not erase the preserved private experiment. No game code, generated game archives, caches, NSOs or keys are published. Keep locally supplied title data separate from public source and release packages; see [LEGAL.md](../LEGAL.md).

## Completion gates

| Area | Remaining proof |
|---|---|
| Native graphics | Actual-game resource/state/shader/draw/submit/present coverage without Maxwell processing |
| Native platform | Required service, memory, scheduling and synchronization behavior without the general emulator runtime |
| Audio/input/storage | Audible output, usable controls and save/reload persistence through native adapters |
| Standalone package | Reproducible supported build and boot/menu/race/restart sequence with all required native dependencies accounted for |

Performance measurements remain useful diagnostics. Correctness and native dependency coverage determine completion. September 2026 suyu-hosted FPS figures in the [README](../README.md) are historical results, not measurements of a completed native port.
