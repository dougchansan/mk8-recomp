# Native runtime progress

Updated 2026-10-17. Repository: `dougchansan/mk8-recomp`. Goal: a standalone title-specific native application with static CPU code and native graphics, platform, audio, input and storage. Completion requires the supported gameplay path to run without CPU JIT, Maxwell/emulated GPU processing or the general emulator runtime.

The campaign is in component integration, before the first end-to-end native game
frame. Static CPU execution, native rendering/presentation fixtures and one
captured game-derived background draw and its native presentation work. The
source matches every packed reference pixel; live game-to-native graphics
integration remains open. Genuine title-sized allocator initialization and
registered callbacks now pass a bounded private CPU suite. The immediate
integration gates are native Device creation and live resource/draw routing. Native
audio, controls, save/restart and full standalone gameplay follow those gates.

The latest standalone service run opens native `aoc:u`, `pctl` and all three
restricted `pctl:s`, `pctl:a` and `pctl:r` ports. Each restricted context has its
own child, domain and close chronology, with its capability mask fixed to zero.
The primary child completes Initialize once. The restricted children are created
and retired without child-method calls; capability denial is tested CPU behavior,
not an observed reply from these game runs. Child, manager and session closes
reconcile before the owned Window is destroyed. No child query or AOC query is
credited.

The game now completes its actual SVC1 request for 3,250,585,600 bytes (3,100 MiB).
The paired raw requests and replies show kernel success, the guest heap base,
preserved other registers, and full native Windows committed-prefix/reserved-tail
coverage. The latest Root-reviewed boot also completes SVC13
(`SetThreadPriority`) for its owned main-thread handle, requesting priority 44.
The actual stored priority was already 44: this proves idempotent delivery,
without claiming a priority change or Windows scheduler mutation. All x1–x31
registers are preserved. SVC41 (`GetInfo` type 0) now returns success and the
title's declared core mask, 7, preserving x2–x31 and the owned policy state.
SVC15 (`SetThreadCoreMask`) now succeeds for handle 0x100, ideal core -1 and
mask 1. The owned state changes from ideal 0 to -1 while execution core stays
0; all x1–x31 are preserved. Execution continues to missing native `fsp-srv`
at 12,780,108 blocks, 9,052 slices and 180 SVC calls. All 10,653 inputs remain
unchanged and the process exits normally with reconciled teardown. Root reviewed
the current raw logs and hashes directly; no fresh independent-agent review is
claimed. The 65,536-slice cap, 30-second internal and 45-second Root deadlines
remain. GetInfo1 is CPU-tested but uncalled. No Windows affinity or scheduler
mutation is implemented or credited.

The latest native FSP build opens `fsp-srv`, returns its actual 4,096-byte pointer
capacity and converts its session to domain object 1. Control 2 then returns a
real new owned handle sharing that service/domain state. `SetCurrentProcess`
on the clone succeeds: its response commits one owned program/process binding,
and execution reaches command 1005, `GetGlobalAccessLogMode`. The run closes
normally at 12,783,762 blocks, 9,078 slices and 188 SVCs, with all 11,333 inputs
unchanged. The full fifteen-suite CPU run covers shared domain state, closing the
original alias, final object destruction and failed-write rollback. The native
runtime's explicit disabled access-log policy is being integrated; content reads,
mounts and saves remain unsupported.

All fifteen CPU suites pass, including 112 filesystem process-binding checks,
6,462 core-policy, 2,035 process-policy and 3,999 thread-priority checks. The
whole-heap assertions include two
separate real 3,100 MiB allocations, independently checking the whole Windows
allocation, growth, shrink, zeroed regrowth, SVC register behavior and ownership
failures. The check count includes memory-map inventory and can vary between
processes. The authored aggregate charge is 3,290,288,128 bytes under the 4 GiB
budget; it is distinct from the actual game-boot observation above.

Fresh SDK and shared-runtime archives pass compiled review alongside 57 retained
title objects: all 74 objects and five archive member sets reconcile, with the
canonical Device callback descriptor present in compiled data. The native game
consumer still needs that path linked and exercised; these archive results grant
no Device, rendering or gameplay success.

The guarded four-image Host has also absorbed the current focus, AOC and primary
parental-control services. All 24 strict C++ compilations and fresh stopped
controls pass independent review. Its latest original-entry run reaches the same
`pctl:s` boundary with 12,614,619 fenced blocks, 3,478 slices and 90 SVC calls.
All 3,570 raw ownership journal rows and 304 current run-input pins reconcile;
all four execution and memory bindings clear before storage teardown.

The native Vulkan Device bridge separately passes 42 CPU rejection checks and
43 actual Vulkan component checks, including factory creation, validated buffers,
exact readback, one retired presentation and revocation/drain/destruction.
The fresh 35-unit static-runtime build and all eight runtime fixtures now pass
independent review. The resource fixture exercises a real 64-byte GPU copy and
31 rejection cases, including denied backend access after parent retirement and
explicit lease release. The earlier failed test remains preserved; production
parent-lifetime checks were kept intact.

All four complete static CPU modules now have independently reviewed generated
source with execution fences and whole-instruction memory guards. Their complete
compilation has also passed independent review: all 74 objects, four module
libraries and one shared runtime. The guarded native Host now links and passes
independent compiled-binary review. Fresh stopped controls and original-entry
startup now pass independent execution review as well.
The source audit retains
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
| Initial owned-display CPU entry | Six CPU suites pass, including 123 policy/wire, 264 identity and 1,751 relocation checks. A fresh process using the existing complete static images admits all four original NSOs and reaches WindowController command10 after 12,602,165 blocks / 77 SVCs. The empty-body unsupported request leaves TLS unchanged and receives no reply; normal exit10, ownership revocation and all 6,840 pinned inputs pass independent review | Historical preimplementation boundary, advanced by the following window-rights run. Explicit SingleApplication logical activity remains separate from raw OS focus. This does not execute the newly generated fenced images. |
| Concrete owned-window right and continued startup | Seven CPU suites pass, including 4,136 rights checks and six mutation controls. Independent fresh-run review verifies one successful command10 reply, one actual window-right acquisition, guest continuation and one teardown revocation. The next unsupported request is SelfController command13 after 12,604,731 blocks / 79 SVCs; its TLS remains unchanged with no reply. Normal exit10 and all 6,860 input pins match | Existing static CPU images and one real owned Win32 window; raw OS focus stays inactive. No input, native Device, GPU frame, saved-state initialization, strict ownership-accounting or general lifecycle support claim. |
| Owned-window focus policy and continued startup | Eight CPU suites pass. A fresh native process commits one real `SetFocusHandlingMode` command13 and one `SetOutOfFocusSuspendingEnabled` command16 reply, then reaches the `aoc:u` service request after 12,608,290 blocks / 83 SVCs. Its unsupported request leaves TLS unchanged and receives no reply. Normal exit10, all 6,970 input pins, unchanged OS foreground, right revocation and focus-authority retirement are recorded | Existing static CPU images and the explicit SingleApplication visible-run/hidden-or-minimized-park policy. Raw OS focus remains inactive; native Device, complete frames, input/audio/save and full guarded-image startup remain open. |
| Native AOC service connection | Nine CPU suites pass, including owned catalog, same-buffer NPDM identity, Count/BaseId and real event-registry transactions. Independent fresh-run review verifies `GetService("aoc:u")` returning a real session, successful response/kernel/SVC results and later `GetService("pctl")`. Normal exit10 after 12,609,347 blocks / 84 SVCs, all 7,466 pins and context retirement match | No AOC query method was called by this game run. Its explicit native mounted-base-game catalog makes no global installed-DLC claim. Next unsupported service is `pctl`; Device, complete frames, audio/input/save and guarded-image startup remain open. |
| Native parental-control manager and restricted ports | Actual startup verifies primary `pctl` and all three restricted ports, distinct child/domain transactions, primary Initialize success and complete closes | Restricted child methods and AOC queries remain uncalled; capability-denial checks are authored CPU tests. No global parental-control or gameplay claim. |
| Native heap SVC1 | All fifteen CPU suites pass; the heap fixture includes two whole-allocation 3,100 MiB VM observations. Actual game SVC1 succeeds; all 10,653 startup inputs match in Root direct review | The verified boot continues through thread-priority setup and the process core-mask query through SVC15 to missing fsp-srv. No native Device or game frame yet. |
| Native owned-thread priority | 3,999 CPU checks cover NPDM permissions, signed low32 ABI, error order, state and ownership. Actual SVC13 succeeds for handle 0x100 / priority 44, preserving x1–x31 and continuing to GetInfo0 | Idempotent delivery for the owned single-thread subset. No Windows priority change, inheritance or multithread scheduler claim. |
| Native process capability queries | 2,035 CPU checks cover declared masks, ABI, validation and ownership. Actual GetInfo0 returns mask 7 and continues to SetThreadCoreMask | GetInfo1 is CPU-tested but uncalled. No Windows CPU-mask query, affinity change or native concurrency claim. |
| Native owned-thread core mask | 6,462 CPU checks; actual SVC15 stores ideal -1 / affinity 1, preserves x1–x31 and resumes to fsp-srv | Logical owned-thread state and captured execution core 0. Migration outside that core and ambiguous NoUpdate-after-DontCare remain unsupported; no Windows affinity change. |
| Native filesystem process binding and session clones | 112 FSP checks and all fifteen CPU suites pass. Actual startup connects FSP, converts to a domain, clones its session and commits the process binding once | Next unsupported operation is GetGlobalAccessLogMode (1005). Native content/storage operations and title Device remain pending. |
| SDK Device callback archive integration | Seventeen fresh SDK/runtime objects and 57 retained title objects pass cross-header ABI, symbol and five-archive member/index review; compiled canonical callback descriptor matches | CPU archive facts only; native consumer linking, runtime callback and actual title Device admission remain pending. |
| Guarded startup with current services | All 24 strict C++ units, current inputs, three executables and stopped controls pass independent review. Actual original entry executes 12,614,619 fenced blocks / 3,478 slices / 90 SVCs; 304 run pins and 3,570 journal rows match, and all four bindings clear | Stops at missing `pctl:s`; native Device, GPU game frames, audio/input/save and supported gameplay remain open. |
| Native Vulkan Device bridge | Full 35-unit compiled-artifact review and all eight actual runtime fixtures pass. Includes 42 CPU rejection checks, 43 Vulkan component checks, a 64-byte GPU copy / 31 resource rejections, 76 state draws / 306,176 pixels, persistent images, presentation conversion and retirement. Every run's inputs match | Authored component inputs only; no title Device or native game frame. No validation layer, vendor/physical-device-class or broad recovery claim. The earlier failed resource test remains preserved; its corrected lifetime expectations keep production rejection checks intact. |
| Complete fenced static CPU source | Independent review covers all four modules: 1,076,832 blocks, 4,832,474 instruction entries and 65 body units. Whole-instruction memory checks cover 1,703,742 entries; 72 exact no-data prefetch hints and 42 explicit halting unsupported sites remain accounted for | Source coverage does not establish workload equivalence or native gameplay. Fresh guarded Host startup still requires validation. |
| Complete guarded static CPU build | Independent review verifies all 74 current objects and five archives against 8,214 inputs and 95 canonical source files. Module metadata, x64 COFF, static CRT, required exports, actual member bytes and all 213 remaining build commands match. Actual module entry offsets are confirmed from compiled return bytes. All 72 hints remain separately classified | Compilation and static inspection. The following Host link is now verified; fresh guarded original-entry execution is verified below. No live Device, game frame or performance result. |
| Guarded standalone Host link | Independent review verifies all 29 build steps, 6,892 current inputs, 22 strict C++ compilations with empty analysis reports, exact five-library linkage and distinct entry/metadata/layout executables. Both maps contain every required A2 member and suffixed API exactly once. Current-header layout data matches all 109 values and 36 OF / 39 MO inputs | The following actual stopped controls and guarded guest startup close the separate CPU execution gate. This link does not establish supported gameplay or live graphics. |
| Guarded original-entry startup | Independent review verifies actual stopped controls, all 109 layout values, genuine four-image bind/reject/restore/clear controls and a fresh original-PC process. That process executes 12,604,731 fenced original blocks, 3,433 slices and 79 SVCs before unsupported SelfController13. All 3,514 ownership journal rows and 215 current run-input pins match; original, memory and witness counters reconcile, and all eight exported binding epochs clear before storage teardown | This guarded candidate retains the earlier service baseline. The newer focus/AOC/PCTL implementations are being integrated into it. All four images are admitted; this path executes rtld/subsdk0/sdk while main original counters remain zero. No Device/provider, GPU, native game frame, audio/input/save or gameplay claim. |
| First guarded CPU compilation stage | Four fresh target objects form one shared-runtime library and the three-member `rtld` library. Independent COFF/archive checks verify x64, static CRT and required bindings. Clang/MSVC compile-only controls agree on 109 size, alignment and offset measurements, plus the fixed marker and interface hash arrays. Actual dependency records cover 290 canonical input paths | Historical first stage, now included in the complete build. ABI controls are inspected compiled data; they are not executed programs. |
| First large guarded CPU body | The largest `main` body, about 32.2 MB of generated source, compiles in about 110 seconds. Independent review verifies the 28,876,197-byte x64 object, static CRT, exact command and 168 input dependencies; prior libraries and controls remain unchanged | Historical first-body checkpoint, now included in the complete build. This build time is not a runtime-performance measurement. |
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

## Release policy and local builds

All five native-foundations prereleases, their 15 download assets and their tags
have been removed at the project owner's request. No replacement native tag or
download will be published until the complete native game is working. The gate
includes static CPU execution, live native graphics, platform services, audio,
controls, saves and restart through the supported gameplay path. The campaign
has not met that gate.

Historical component source and verification records remain in the repository.
Development binaries, private diagnostic bundles and closed verification receipts
remain local for continued implementation and regression checks. See
[development build notes](native-builds.md) for the policy and retained source.
No game code, generated game archives, caches, NSOs or keys are published. Keep
locally supplied title data separate from public source; see [LEGAL.md](../LEGAL.md).

## Completion gates

| Area | Remaining proof |
|---|---|
| Static CPU integration | Complete guarded compilation and bounded original-entry startup pass; extend the supported gameplay path without reaching an unsupported site |
| Native graphics | Actual-game resource/state/shader/draw/submit/present coverage without Maxwell processing |
| Native platform | Required service, memory, scheduling and synchronization behavior without the general emulator runtime |
| Audio/input/storage | Audible output, usable controls and save/reload persistence through native adapters |
| Standalone package | Reproducible supported build and boot/menu/race/restart sequence with all required native dependencies accounted for |

Performance measurements remain useful diagnostics. Correctness and native dependency coverage determine completion. September 2026 suyu-hosted FPS figures in the [README](../README.md) are historical results, not measurements of a completed native port.
