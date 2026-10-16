# Native runtime progress

Updated 2026-10-15. Repository: `dougchansan/mk8-recomp`. Goal: a standalone title-specific native application with static CPU code and native graphics, platform, audio, input and storage. Completion requires the supported gameplay path to run without CPU JIT, Maxwell/emulated GPU processing or the general emulator runtime.

The campaign is in component integration, before the first end-to-end native game
frame. Static CPU execution, native rendering/presentation fixtures and one
captured game-derived background draw and its native presentation work. The
source matches every packed reference pixel; live game-to-native graphics
integration remains open. Genuine title-sized allocator initialization and
registered callbacks now pass a bounded private CPU suite. The immediate
integration gates are native Device creation and live resource/draw routing. Native
audio, controls, save/restart and full standalone gameplay follow those gates.

All four complete static CPU modules now have independently reviewed generated
source with execution fences and whole-instruction memory guards. Compilation
and fresh startup validation are the next CPU gates. The source audit retains
every discovered legacy code entry except 36 verified zero words, and reports
42 explicit unsupported sites that halt. Their absence from a supported gameplay
path still needs actual execution evidence.

## Verified foundations and their limits

| Evidence | Verified result | Limit |
|---|---|---|
| Native NVN builders in the actual-game reference replay | 90,320 native calls; 6,001-frame replay reached EOF; CPU JIT 0, imports 0; observed racing | Still uses reference graphics. This is not standalone native gameplay. |
| Public v6 feature/format fixture | 148 CPU checks and a separate 16-draw / 549-pixel / 196-check GPU run pass. Covers BC4 partial blocks/filtering, packed sRGB/swizzles, independent depth/provoking state, full uniform blocks, blending and exact untouched/masked bytes | Authored inputs only. Computed colors use UNORM8 tolerances; custom-border creation/metadata does not prove border sampling. Full declared uniform bounds remain required. |
| Native Vulkan triangle fixture | 1,152 red pixels and 2,944 black pixels | Synthetic rendering; no actual-game draw/present. |
| Textured native fixtures | Five draws, 20,480 pixels checked and ten rejection cases | Synthetic resource/draw coverage only. |
| Native-owned pool copy | 64-byte copy and 29 error cases | Bounded memory ownership/validation test. |
| Persistent native images | Three-draw producer/consumer fixture checks 4,096 pixels and 71 rejection cases; retained native image is sampled directly | Synthetic Stage A. No intermediate target readback/reupload; actual-game state and content history remain required. |
| Standalone actual CPU execution | Native SM/LM/APM and applet/domain bootstrap reach a real CommonStateGetter event. Actual Win32 inactive observation produces message15 and focus query2; guest then parks at 12,600,537 blocks / 77 SVCs. Deadline cancellation is accepted | No genuine active-focus transition, later service coverage, graphics or gameplay claim. Parked state and cancellation are distinct from guest exit. |
| Complete fenced static CPU source | Independent review covers all four modules: 1,076,832 blocks, 4,832,474 instruction entries and 65 body units. Whole-instruction memory checks cover 1,703,742 entries; 72 exact no-data prefetch hints and 42 explicit halting unsupported sites remain accounted for | Generated-source evidence only. The four new static libraries and their shared runtime still need compilation and fresh startup validation. This does not establish workload equivalence or native gameplay. |
| Public source/API tests | Synthetic API v2 identity/sealed-binding negotiation, full-range guards and bounded BufferInit checks pass | The miniature API fixture does not attest a complete actual title module or establish every required NVN contract. |
| Scheduled native API v3 | Nine return kinds, cancellation/commit races, stale completion and no-fallback behavior pass synthetic tests; emitted transport and CPU owner pass ordinary/chained FM1/GG1/FPX1 suites; 144 strict guard mutations reject | Real DeviceInitialize remains gated on provider effects, registered allocator semantics and ownership isolation. |
| Typed native renderer | Five suites pass, including 76 draws / 306,176 pixels / 66 rejection checks and 4,096 CPU packing roundtrips. Independent alpha blending, float2/3 vertices, swizzles/sRGB, filtering, inverted viewport and packed10 producer/sampling preserve declared semantics | Synthetic capabilities. Actual-game shader/state/resource/history and presentation remain separate gates. |
| Native texture conversion | 210 independent tiling cases and CLI checks pass; five recorded source textures decode with matching hashes after dependency simplification | CPU backing is not authoritative for all GPU writes. GPU-produced target contents are excluded; no frame-equivalence claim. |
| Native window observations | Real owned Win32 window, bounded pump/wait, overflow/close handling and owner lifetime pass strict compiler analysis and tests; actual inactive guest lifecycle exchange verified | No positive foreground activation, native input or rendering proof. |
| Owned shader-input construction | Explicit source/metadata factory shares the offline translator; synthetic ownership/rejection and authored-cache equivalence checks pass | Source-only in this package. Structural acceptance does not attest complete actual shader provenance or raster state. |
| Real shader translation and pipeline acceptance | Actual shader translation and pipeline acceptance verified separately | No actual-game native draw or presentation established. |
| Native Win32 presentation | Fresh v6 build passes eight NVN suites and an owned-window fixture: 45 queued/retired presents, three swapchain recreations, RGBA8 and packed10 sources, zero source readbacks; both cropped client images inspected | Synthetic default-feature presentation, separate from the new feature-profile fixture. Requires enabled maintenance1 support; validation layers disabled. Driver retirement can block teardown. Actual-game dispatch remains pending. |
| Guest callbacks and execution fence | Independently reviewed owner callback transport, per-image original/native approval, all-entry checks and unchanged default output; FM0 and FM1/GG1/FPX1 ordinary/chain tests pass | Serialized single-owner scope. Actual Device allocator routing and native Device creation remain closed. |
| Opaque-object memory and accounting | Independently reviewed 106-case preflight; 15,524 emitted integration checks and 4,102 accounting checks per chain mode. Full CPU/callback/pending regressions and seven production/three test analysis units pass | Protects native storage from guest accesses, including aliases and split instructions. It does not implement the native Device or a general concurrent scheduler. |
| Real SDK allocator initialization | Independently reviewed constructor: 408 checks across fresh and negative cases. Genuine 64 KiB supplied-heap initialization: 470 checks, with 105 callback blocks and 266 checked memory operations per successful run; parent context and heap boundary canaries preserved; zero SVCs/provider commits/GPU work | Earlier private static CPU diagnostic through an authored callback driver; Device callback integration is outside this fixture. |
| Genuine SDK bootstrap and raw allocator pair | Independently reviewed 1,934 checks across ten cases. Two fresh processes execute SDK OS initialization, constructor, 64 KiB cache-disabled Initialize, one raw allocation of 8,199 bytes at 16-byte alignment, exact Free once and Finalize with an empty live ledger. Each bootstrap uses 16 native queries; allocator phases use zero SVCs, provider commits or GPU work | Earlier private CPU fixture for one raw pair. General TLS, contention and Device integration remain separate gates. |
| SDK small blocks and three live allocations | Independently reviewed 3,490 checks across 16 cases. Genuine 160-byte and 256-byte allocation/free pairs, then simultaneous 160/256/8,199-byte allocations with exact reverse frees and empty Finalize. Original read-only class tables, surviving payloads, canaries and full callback restoration pass; copied runnable bundle passes a fresh 354-check three-live sequence | Earlier 64 KiB/cache-disabled fixture. General workloads, realloc, contention and Device integration remain unproved. |
| Title allocator and installed hooks | Independently reviewed 12,017 checks across 16 fresh CPU processes. Original title initialization establishes a supplied 24 MiB heap; genuine SDK registration installs the title callbacks. Raw and aligned allocations, stored raw-pointer headers, exact reverse frees, TLS errno and empty Finalize pass with three-image accounting. A copied bundle passes a fresh 1,162-check three-live sequence | Selected fresh cache-disabled instance and sizes, one owner, no general workload/realloc/contention or complete game startup. Provider work and Device/GPU admission remain zero. |
| Genuine SDK callbacks with strict accounting | Independently reviewed 2,794 checks across eleven fresh processes and 76 accounting observations. Both image adapters activate before SDK bootstrap. Each raw pair records 3,293 original entries, one accepted parent call, zero SDK native calls and zero provider work; callbacks and return markers retain exact credits. Copied bundle passes 394 fresh checks | Ordinary dispatch with callback budget one; one admitting image/provider and one operation per binding. Chain execution, title hooks, larger heaps and Device integration remain separate gates. |
| Genuine SDK chain accounting | Independently reviewed 1,674 checks across four fresh budget controls and 84 observations. One original same-image SDK call uses the existing chain helper; actual interval credits are 1/2/3/557 for budgets 1/2/3/4,096, with chain witnesses 0/1/1/1. Full raw-pair credits remain 3,293; copied budget-4,096 bundle passes 418 fresh checks | One direct SDK edge, one admitting image/provider and one operation per binding. General chaining, title hooks and Device admission remain unproved. |
| Native captured game draw | Independently reviewed native Vulkan submission completes one indexed draw with three image uploads and two readbacks. All 2,073,600 packed pixels / 8,294,400 bytes exactly match the reference, including alpha; every channel error is zero. Native binary uses fixed offline shaders and links no reference renderer or runtime shader compiler | One captured striped background layer on the verified device. Inputs originate from the reference capture; this is not a complete native frame, live game rendering or standalone gameplay. |
| Captured producer to native presentation | The same native image reaches 45 successful presentations, 45 raw successful queue results and 45 separately retired maintenance fences. Zero CPU pixel copies connect producer and presenter. The source remains exact at all 2,073,600 pixels; an owned 1920x1080 window witness was inspected | The selected background layer only. SDR display conversion is separate from exact source bytes; no complete game frame, live Device, scanout accuracy or performance claim. |
| Combined fenced pending-work accounting | Independently reviewed synthetic integration passes 1,094 checks and 98 observations in each ordinary/chain mode. Actual accepted calls, guest blocks, copied-input worker units and commits remain distinct through cancellation, revocation and stale completion | After-v5 private fixture with one admitting image/provider and one operation per binding. It does not establish general multi-provider attribution or Device/game integration. |
| Device-coordinator CPU lifecycle | Independently reviewed 22,824 checks across 76 fresh cases, with 408 closed raw/audited observation pairs. Callback returns, cancellation after effects, stale identity, chain-budget thresholds and teardown retain distinct approvals, accepted calls and completions | Private synthetic CPU model. Actual registry objects retain reservation/null-backend state and retire during teardown; this does not create a live native Device or supply an actual game request. |

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

The subsequent private native replay now passes. Fresh offline translation uses
the exact closed shader bytes, their original instruction addresses and recorded
specialization. A separately reviewed access proof bounds full uniform reads to
560 of 768 captured vertex bytes and all 64 fragment bytes, including every
branch and all four vertex inputs. It requires no invented uniform padding.

On the verified AMD device, native Vulkan recreates the observed sRGB/BC4 images,
swizzles, sampler, viewport, clip/provoking state and separate RGB/alpha blending.
It loads the actual inherited destination, submits one indexed draw, waits for
its fence and closes the native readback. Independent comparison finds zero
different packed pixels across all 2,073,600 pixels; all 8,294,400 output bytes
match. Native output and actual readback provenance are independently reviewed.
The selected image is a striped background layer, separate from the final
Stadium racing frame inspected after the reference replay.

The same native producer image now also reaches the existing native presenter.
Independent review verifies 45 successful presentations, 45 raw successful queue
results and 45 separately signaled retirement fences on one native device. The
producer and display conversion draws are counted separately. No CPU readback
or reupload transports the source between them. An owned 1920x1080 client
capture shows the striped layer; source pins are released after presentation
drains and before the final diagnostic readback. All source bytes remain exact.
The SDR display conversion and eight-bit quantization are separate display
policy, with no scanout accuracy or performance conclusion.

This closes captured-input native drawing and presentation for that layer.
Complete actual-game producer/consumer history, live native Device/resource
routing, complete game frames and standalone gameplay remain open. The public
foundations release uses authored fixtures; game-derived shaders, captures and
diagnostic binaries stay private and are indexed locally.

A private CPU fixture starts with genuine SDK OS initialization.
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

A separate independently reviewed extension passes 3,490 checks across 16 cases.
It adds original read-only class tables, fresh 160-byte and 256-byte allocation
pairs, and two fresh runs with three simultaneous allocations followed by exact
reverse frees. Every surviving payload is checked after later allocations and
frees; Finalize follows an empty allocation ledger. Table corruption rejects
before guest execution, table writes are denied, and missing table access
quarantines with its partial phase recorded. A copied private bundle passes a
fresh 354-check three-live sequence from its own directory.

Those earlier CPU proofs cover the reviewed sizes on a 64 KiB cache-disabled heap.
A separate ordinary-dispatch fixture composes the genuine raw SDK
pair with strict Host accounting from before bootstrap: 2,794 checks in eleven
fresh processes and 76 accounting observations pass independent review. Accepted
calls derive from actual captured requests, tokens and transport counters;
source approvals, guest entries and provider work remain separate. Quarantined
failure diagnostics retain counters without restoring execution authority.

A further title-specific CPU fixture now passes independent review: 12,017
checks across 16 fresh processes. Original title code initializes a supplied
24 MiB cache-disabled heap, and genuine SDK registration installs the actual
title allocation/free callbacks. The complete original table initializer runs.
Three simultaneous allocations include aligned storage whose raw-pointer header
is written and consumed by original code; exact reverse frees leave an empty
ledger before real Finalize. Payloads, boundary canaries, full callback context,
TLS errno and three-image execution/memory accounting pass. A separately copied
runnable bundle passes a fresh 1,162-check three-live sequence.

This covers the reviewed fresh instance and sizes. Full startup, general
workloads, realloc, general ELF/dynamic TLS, contention, post-effect recovery and
native Device allocation policy/live routing remain open. These game-linked CPU
diagnostics and inputs stay private and are indexed locally, outside the public
foundation tools and verification manifest.

Decoded draw state establishes source-alpha blending, a destination-multiplication pass, float2/float3 vertices, linear repeat/clamp sampling and a negative-Y viewport transform. There is no Clear in the submitted graph, and full destination-independent overwrite is unproved. A faithful native replay requires the earlier attachment contents or a complete overwrite proof; an invented black target is not a valid substitute.

The next graphics gate is live game-to-native Device/resource/draw/presentation integration, extending beyond the captured background replay. Native audio, input, saves and overall gameplay closure remain pending.

## Public checkpoint and prerelease

The checkpoint targets suyu source revision:

```
347fcf91d95cd39cb5c3db50f1b6d827ad699311
```

Source changes are represented by `src/patches/native-runtime.patch` against that exact base. The 168-file v6 checkpoint updates 14 generic graphics/NVN/shader files and adds one authored fixture; 153 subjects retain their v5 bytes. Window and Presenter implementations retain their v5 bytes. Synthetic CPU, service, entry/API v2/v3, estimate, callback, fence, memory and accounting checks are recorded alongside graphics/window validation. [Verification](../native-checkpoint/verification.json) records the public binaries and source correspondence. These notes do not claim a verified setup on a fresh machine.

The [prerelease `native-foundations-v6-20261015`](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v6-20261015) contains 34 Windows x64 verification tools, synthetic fixtures and corresponding source. It is **not a playable MK8D build**. The new explicit GPU fixture passes 16 authored draws and 549 pixel comparisons; a separate rebuilt Presenter completes 45 queued/retired presentations. The source archive disables global Git line-ending conversion and matches 5,389 exported base files to the pinned pristine checkout. The final archive retains all 34 fresh-built binaries and the original 40 build/test records while repacking the base source and packaging helper. The shader-input factory is source-only; matching offline compiler libraries, real allocator AOT and game-linked diagnostics are excluded from release binaries. Earlier [33-tool v5](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v5-20261015), [27-tool v4](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v4-20261014), [24-tool v3](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-v3-20261014) and [13-tool v2](https://github.com/dougchansan/mk8-recomp/releases/tag/native-foundations-20261014) checkpoints remain immutable.

The development checkout also keeps a gitignored `local/native-builds/latest.json` index with exact public-package and private diagnostic executable hashes. Game-linked CPU/reference builds remain local; only tools with synthetic inputs and publishable corresponding source become release assets. See [build instructions](native-builds.md) for the current public package.

The public checkpoint excludes the private exact copy32 matcher and its tests because their provenance is unsuitable for this checkpoint. That omission does not erase the preserved private experiment. No game code, generated game archives, caches, NSOs or keys are published. Keep locally supplied title data separate from public source and release packages; see [LEGAL.md](../LEGAL.md).

## Completion gates

| Area | Remaining proof |
|---|---|
| Static CPU integration | Compile the four complete fenced images with one coherent runtime, then verify fresh startup and the supported path without reaching an unsupported site |
| Native graphics | Actual-game resource/state/shader/draw/submit/present coverage without Maxwell processing |
| Native platform | Required service, memory, scheduling and synchronization behavior without the general emulator runtime |
| Audio/input/storage | Audible output, usable controls and save/reload persistence through native adapters |
| Standalone package | Reproducible supported build and boot/menu/race/restart sequence with all required native dependencies accounted for |

Performance measurements remain useful diagnostics. Correctness and native dependency coverage determine completion. September 2026 suyu-hosted FPS figures in the [README](../README.md) are historical results, not measurements of a completed native port.
