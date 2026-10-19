# Native runtime progress

Updated 2026-10-18. Repository: `dougchansan/mk8-recomp`. Goal: a standalone title-specific native application with static CPU code and native graphics, platform, audio, input and storage. Completion requires the supported gameplay path to run without CPU JIT, Maxwell/emulated GPU processing or the general emulator runtime.

## Belmante runtime adoption

The current source recipe is R176, reconstructing 182 generic adapted files.
All 40 component suites and the 1,820-check native GPU fixture pass, plus a
separate 300-check synchronous-submission regression. A production
codec API probe passes 309 checks for 32 owned decoded pictures, actual picture
IDs, planar pixel conversion, draining and mapped-memory revocation. The full
movie's 13,200 input-owned frame hashes match across default and interactive
native decoder modes.

R172 adds opt-in aggregate replay timings, buffered routine logs with immediate
fatal flushing, and a switch to disable per-frame stall tracing while retaining
the hang watchdog. The first live timing experiment exposed millions of focus
gate calls; the current profiler samples that gate once per 1,024 calls to reduce
measurement overhead. Reported sampled focus costs are estimates. R176 removes
the temporary per-opcode table/timers after they identify counter reporting as
the expensive state operation; aggregate and counter-phase timings remain.

Counter-report destination admission takes 3,241 ms across 300 reports in the
R174 diagnostic window, while all other report phases together take about 9 ms.
The successful-span cache introduced in R175 is invalidated by every tracked
guest-map or Win32 reserve/commit/decommit mutation. It caches neither failures
nor object liveness. Regressions cover guest permission revocation/restoration,
host decommit/recommit, cross-page spans and cross-thread invalidation. Both the
cached and uncached paths pass the expanded 1,820-check GPU fixture.

A same-executable R175 comparison over title frames 121–180 measures median
48.85 ms/frame with the cache against 219.45 ms without it, about 4.49 times faster.
Mean intervals are 60.30 and 230.14 ms, respectively. The captured frame 120 is
byte-identical across all 2,073,600 RGB pixels, and draw count/order match. This is
one sequential pair on a shared desktop, restricted to the title scene; it is
not a course-FPS or playable-game claim. The profiling-disabled control and an
async submission case also reach the identical title image. A longer async
menus/race-loading run is under verification; async remains opt-in.

Query pools now reset only slots used in their previous recording. An explicit
full-reset control remains available. Shrinking, empty and growing query sets
pass across pool reuse. This avoids redundant work but was not the main measured
counter-report cost. No counter wait, capacity or arena bound is relaxed.

The GPU clock now uses supported calibrated DEVICE-domain timestamps, with the
same timestamp units/epoch as submitted GPU queries and a submitted-query fallback.
An authored 64-read comparison measures 3.253 microseconds/read versus 92.617 for
the submitted path; both pass 1,761 GPU checks, including monotonic clock reads
and existing query bracketing. This is a local clock benchmark, not a game-FPS
measurement. The original arena bounds validation remains unchanged.

The focus gate combines duplicate ownership checks and locked sections without
caching liveness. Hidden-window parking, destruction rejection and reentrant
notification policy changes pass. Five alternating authored benchmark runs give
median 20.339 versus 34.107 ns/call on one thread, and 283.345 versus 515.977 with
16 threads. These figures do not establish its share of game-frame time.

Existing R164 heavy stadium windows measured host replay at 1.93–2.29 seconds
per frame, presentation at 140–185 ms and submission fence waits at 22–23 ms.
Warm pipeline creation was negligible. Finer timings are required before
attributing the replay cost to a particular function or adding another cache.
The later R166 run was intentionally stopped for this performance campaign;
its source, executable and saved input remained unchanged. It did not reproduce
the arena failure. Native gameplay, performance and CPU/GPU overlap remain open.

The R164 run presents its first native Mario Kart Stadium course intro.
Root review of original presented frame1615 verifies the track, lit Mario and
kart, spotlights, particles and50cc course UI. The lossless PNG and original RGB
hash are retained privately with the run receipt. This uses the unchanged static
CPU archives, native Vulkan backend and required offline shader cache; no game
state was patched to reach the scene. Countdown, driving, stability and complete
gameplay remain under verification. No release or download is available.

R164 continues through the course cameras to the player starting-grid view with
its first-race banner, then closes naturally with a command-memory arena bounds
failure before the countdown. The run lasts 4,193.906 seconds with source,
executable and saved profile unchanged. Its owned supervisor deadline was extended
without suspending title threads or modifying game memory; both supervisor and
extension guard finish after the title exits. Driving is not verified.

R166 preserves the original EQUAL depth comparison and ignores stored bias when
polygon offset is disabled. The inherited workaround previously changed EQUAL
to LESS_OR_EQUAL and implicitly enabled bias. A new authored GPU regression
reproduces both violations: matching geometry with disabled stored bias loses
all 16 expected pixels, while closer geometry passes EQUAL incorrectly. After the
correction, all eight depth cases pass through the production pipeline builder
and original recorded bias setter. All 39 component suites, 1,633 native GPU checks
and 300 synchronous checks pass, including the separate diagnostic-cutoff follow-through.
This is an API-correctness fix; its effect on selection-menu lighting is unproven.
The arena validation remains, with exact pool/offset/size/callback/LR diagnostics
for the next reproduction. No bound is relaxed to advance the game.

R162 passes the former point guard, executes the original samples-passed report
and records conditional-render requests during race loading. It closes naturally
after 1,825.422 seconds when the HDR compositor stage c61922efb137d6e4 is missing.
Source, executable and saved profile remain unchanged; foreground changes during
the run. The stage is hdr_compose/v210/pixel from the original common graphics
archive. Its code hash and every compiler-consumed control field match the live
capture. All 122 remaining vertex/pixel stages from that inventory now translate
and pass GPU module creation, growing the private cache from 7,243 to 7,365. One
geometry stage is outside this batch. Module acceptance gives no pipeline or
rendering-equivalence credit; the new original-game flight is in progress.

R164 retains full-HD HDR draw readbacks up to 16 MiB. The previous 4 MiB file limit
discarded model targets after copying them. An authored 1920x1080 GPU-clear test
checks the complete 16,588,800-byte half-float payload, header and EOF. The prior
writer fails that regression. Optional captures also include mip1. A bounded
draw-sample diagnostic reports mesh counts and depth state, and can turn off
before original counter requests; a regression verifies its cutoff followed by
the real 300-check synchronous counter path. The old cutoff behavior fails.
All 39 component suites, 1,567 native GPU checks and 300 synchronous checks pass;
the two changed implementation files analyze cleanly.

Actual R162 input captures contain finite, approximately unit normal/tangent
vectors in the captured GPU buffer prefixes. Those prefixes are not a complete
indexed-vertex proof because base vertex is unrecorded. Mario's face remains dark
in the presented image. Full model draw outputs, mip contents and sample coverage
are being collected to distinguish shading from raster/depth rejection. A native course intro is now verified; driving and a playable result remain
unverified.

R162 supplies the original NVN point size to native point pipelines. When the
offline vertex stage has no programmable PointSize output, a checked SPIR-V
interface adaptation stores the actual fixed size before each entry-point return.
Existing shader-written sizes are preserved, including block-member outputs.
Pipeline and module caches distinguish fixed sizes; physical size ranges and the
largePoints feature are checked. No Maxwell translation is added at runtime.

An authored GPU regression uses the production pipeline builder: size1 produces
exactly one red pixel, size3 produces nine, and programmable size3 still produces
nine when fixed state says1. The original SetPointSize recording/replay path is
included. CPU regressions cover structured early returns, helper functions,
existing outputs and malformed inputs. All 39 component suites, 1,560 native GPU
checks and 300 synchronous checks pass. Static CPU archives are unchanged. An
initial Windows paging-capacity compiler failure is preserved; the unchanged
source succeeds with one compiler on retry. Its original-game run passes point
setup before the later missing HDR-compositor stage described above.

R161 replaces the inherited forced-on conditional-render handler with GPU
predicates. The original SDK maps API modes 0/1 to equal/not-equal rendering.
An authored compute kernel compares the two 64-bit counter values while ignoring
timestamp words, using the latest GPU-written buffer ranges. Vulkan conditional
rendering gates the original draw calls. Conditional clears use explicit gated
attachment operations because load-op and transfer clears ignore the predicate.
Explicit enable/disable overrides also affect clears. Hardware support, source
bounds, readable permissions and contiguous virtual mappings are checked.

Native regressions verify positive and suppressed pixels/sample counts, high-word
differences, stale CPU shadows, a real counter-to-predicate-to-counter chain,
color/alpha-only/depth clears and explicit overrides. R161 passes 38 component suites,
1,262 native GPU checks and 300 synchronous checks. The synchronous fixture
uses its own transfer setup/readback resources and the real gpu_submit clear path.

R159 passes the previous array-texture rejection and closes naturally after
1,897.125 seconds on the next guard: point-primitive rendering needs a point-size
shader profile. Source, executable and saved profile stay unchanged; foreground
changes during this run. Corrected captures show populated, distinct HDR cubemap
faces and populated eye/skin textures. Model diffuse lighting and a native race
image remain unverified. R162 now implements checked native point-size outputs;
its original-game validation is next.

R156 closes naturally after 1,668.812 seconds with source, executable, saved
profile and foreground unchanged. It reaches the race-loading screen, then
rejects a shader requesting a 2D-array sampler with a bound single-layer 2D BC3
texture. R159 admits that valid one-layer array view while retaining format,
sample-count and layer bounds. A native GPU regression creates the real view,
samples the GPU-written image and checks both returned colors.

R159 also corrects diagnostic subresource selection: the raw HDR capture path
previously read face zero/mip zero for every requested face. The converted path
selected the mip but used base dimensions. Authored regressions verify exact
pixels, dimensions and file lengths for all six faces at two mip levels through
both paths. Earlier HDR reports are preserved with explicit errata; they do not
prove six-face HDR contents. The old code fails the new profile and capture
regressions. R159 passed all 38 component suites, 930 native GPU checks and the
separate 19-check synchronous probe before the later conditional implementation.

R151 passes the race UI shader and reaches a new missing API,
`nvnCommandBufferResetCounter`, with an actual samples-passed counter request.
The new native counter module uses precise Vulkan occlusion queries around
original NVN draws, accumulates ended query results on the GPU across reports
and submissions, and writes results through the checked GPU-written range path.
Timestamp reports use real top/bottom GPU queries, with an exact rational
conversion into native counter units. The current-timestamp getter also queries
the GPU. The inherited sequence-number/host-clock reports are removed.

GPU regressions check zero and exact positive sample counts, cumulative and
repeated reports, reset behavior, accumulation across submissions, and consistent
top/bottom/current clocks. Counter readbacks retire after the entire submission,
including its late graphics part. Both pipelined and synchronous paths are wired;
the dedicated synchronous regression reproduces the previous unbound-context
failure and passes after the fix. The inspected hardware has a 10 ns GPU tick,
64 timestamp bits, shaderInt64 and precise occlusion support. Counter types
0, 1 and 16 are supported; other statistics remain open. The new conditional
module has component GPU evidence; its original-game behavior is unverified.

R150 corrects a second color defect: the original RGB565 cup icons store red in
the low five bits, while the old Vulkan mapping used the high five bits. An
authored GPU regression reproduces the wrong packed red value on the old runtime
and checks all primary channels after the correction.

The lighting capture also shows a BC3 texture requested through an unsigned
integer view. The old runtime ignores that view and the sampled cubemap is black.
The new bounded path creates compatible block views, restricts image-view usages,
and sizes render areas in blocks. GPU tests write distinct colors into all six
faces at two mip levels, read back every compressed block and sample the result.
The two texture-compression pixel stages are rebuilt offline with explicit
unsigned outputs; incompatible fragment-output/target types stop execution.
All 88 runtime objects were rebuilt, and static analysis of the five changed
implementation files reports no diagnostics. The original-game capture now shows
colored compressed cubemaps and visible kart specular highlights. The 36 HDR
capture files contain nonzero RGB without NaNs or infinities, but all read face
zero because of the diagnostic bug corrected in R159. The 36 converted compressed
face readbacks contain color. Mario's face and diffuse lighting remain too dark.
The cup screen directly verifies the RGB565 correction: the mushroom is red,
the rims are gold and stars are yellow. Race rendering remains unverified.

R151 adds an optional, bounded capture of the actual GPU-bound uniform ranges
before dynamic offsets are split out. It copies data on the GPU and writes it
only after the submission fence. An authored regression initializes the CPU
shadow, overwrites the source on the GPU and verifies the captured offset/range
contains the GPU result. Fragment uniform diagnostics now use NVN stage 1.
The two diagnostic files analyze cleanly; the descriptor-set file retains a cache
warning reproduced on its unchanged parent function. No new warning is attributed
to the capture path.

An R148 run passes the previous course particle shader and stops at a
missing stadium-flag stage. A wider inventory extracts 2,283 map-object stages;
all 2,053 missing stages now translate, including seven fixed control prefixes
whose complete source records are retained privately. Along with the other
Mushroom Cup models and effects, the private cache now holds 7,239 stages.
AMD accepts all 2,055 newly staged modules, including two integer-output
replacements. This is module acceptance, without pipeline or gameplay credit.
The R150 run passes that stage and reaches the race item-box UI before another
missing pixel stage. All four missing UI stages translate and pass module
creation, bringing the private cache to 7,243. R150 closes naturally after
1,743.813 seconds with source, executable, saved-input profile and foreground
unchanged. R151 then reaches the original samples-passed reset request.
R159 passes the sampler fix before reaching the point-primitive guard. No native race image or playable result is claimed.

The native buffer-to-texture operation now validates active recording, real source
pools, copied SDK texture descriptors and bounded copy regions. Actual Vulkan
readbacks check R8/RG8/RGBA transfers, submission-time CPU data, earlier GPU writes
and partial updates. The original game records both decoded video-plane copies
and advances to its color-correction shader. An earlier cache held 4,618
offline-translated stages. An explicit graphics V10 profile permits checked 3D
float sampling. The live lookup texture identified by the game is packed RGB11F;
the new bounded 8x8x8 path reuses existing block-linear conversion and exact float
widening. Authored GPU tests verify all voxels and sampling at distinct depths.

A timing-dependent native wait bug was isolated by tracing the original movie
client events. The old synchronization entry discarded a cancellation delivered
before the wait, losing a MultiWait notification. The new regression fails on
the old code and passes after preserving the pending cancellation. Tests cover
ready-object and zero-timeout precedence, one-shot/coalesced cancellation, an
already-blocked host thread, and invalid handles. No game event is forced.

The video color defect has now been isolated and corrected. The original SDK
returns planar color format 19 while the title requests NV12. Captured first-frame
luma matches the native decoder exactly, and the chroma matches planar U-then-V
rather than interleaved UV. The adapter retains the original SDK buffer/status
and format-query APIs, then converts only the caller-owned buffer for the native
AVC decoder's requested NV12 output. Both live upload planes now match the
canonical native-decoded NV12 frame byte-for-byte (466,944 bytes total), and a
reviewed native preview frame has no previous green/magenta corruption.
GPU-produced volume images remain authoritative; stale CPU backing is not used
to overwrite them. The R147 run reached the race-loading screen before another
missing offline vertex stage. R148 passed that stage and reached a missing
fragment stage from Mario Kart Stadium. The course archive contains 160 unique
stages; all 129 still missing from the previous cache translated successfully,
and the AMD driver accepted every resulting module. Module creation does not
verify pipelines or rendering. The unchanged R148 executable is being tested
with this expanded cache. Actual race rendering and the dark character-lighting
defect remain open.

The R146 full-game run directly presents changing movie-preview frames in the
Single Player menu, then renders native character/kart/glider and Mushroom Cup
selection. This is further native rendering progress, with explicit defects:
movie colors are green/magenta-corrupted and 3D character lighting is too dark.
The run closes intentionally after its scripted input reaches cup selection;
source, executable, saved-input profile and foreground identity remain unchanged.
Races and a complete playable application remain unverified. A follow-up captures
the actual transfer-plane bytes and lookup-image state, and extends the scripted
cup/race confirmations. No native tag, binary or download is published.

The earlier R132 saved-profile run directly renders the native main menu and
creates the real bundled Google AVC component. The movie uses Main-profile AVC
with CABAC, which the bundled baseline parser rejects. The Windows bridge uses
the operating system's native Media Foundation decoder behind the original
H264SwDec interface, preserving the original movie/OMX lifecycle and parameters.
A separate full-movie test decodes all 13,200 pictures with unique input ownership;
pixel hashes match between the default and interactive decoder modes. Interactive
mode prevents the default buffering from exhausting the SDK-facing pending-input
bound. Transport timestamps start at zero and map back to real SDK picture IDs.
No movie data or decoder-success status is fabricated. Subsequent R148 evidence
above verifies native video presentation; races remain unverified.

The SDK's real component registry already contains the software decoder, but its
separate MIME matching table still chooses NVIDIA even with software preference.
The adapter adds the existing software candidate through the original SDK
String8 and vector copy/destruction APIs. Explicit component requests and
hardware-only filters retain their original behavior. It preserves the original
movie parameters and MIME factory, without inventing codec objects, statuses or
frames. The public recipe contains generic adapter code only; development
executables, game libraries and assets remain private.

The isolated Windows target uses MIT-licensed SuperMarioWonderRecomp revision
`010ed7a596a621c15d3a8dfa5795ca8c3668b066`, retaining its attribution. MK8D's
main/subsdk0/SDK form three static libraries and a local standalone executable.
The Vulkan dispatch table is regenerated for the already installed headers.

Thirty-six CPU/component suites pass, including SDK 53 texture/sampler
comparison, focus and exit policies, HID/touch configuration, save-file I/O,
parental-control adaptation, loader, IPC, RomFS and native device admission.
A separate 461-check GPU fixture creates and drains a real device and queue on
an RX 9070 XT, exercises command-memory callbacks and rejects unsupported
configuration without loading guest modules. Font-bundle and application-control
parsers cover valid and malformed inputs. The real image's 1,122 directories,
6,597 files and eight native sample reads match an independent parser.

Actual MK8D startup enters `nnMain`, mounts its content, creates an owned native
window and Vulkan device, initializes its queue and three presentation textures,
acquires a texture and configures command buffers. It mounts CommonSave/UserSave,
finds the Ghost directories, and creates and commits its user data file. Texture
cache setup now handles copied texture and sampler descriptors and reaches the
first texture upload. Debug labels now share owned native object storage.
Packaged texture layouts carry block height into storage sizing and upload.
With the local offline graphics cache, startup passes the previous textured
shader setup, raw compute program setup and system-resource mounting. It now
starts the SDK53 software audio render thread and loads sound banks, then reaches
UI and RaceCommon asset loading. The pinned application-control reader answers
the display-version request using privately supplied title metadata. Startup
continues through menu, Mario, kart and 3D Mii assets. The native random-Mii
request succeeds. Game-driven Vulkan draws and presentation are now logged.
The early black readbacks were superseded by directly reviewed native frames:
the startup logo and title screen, then the main menu after scripted built-in
Mii selection. A saved-profile restart reaches the same menu and the original
game reports its second startup. These results establish native menu rendering
and basic profile persistence; races and complete gameplay remain unverified.

The genuine compiled SDK graphics initializer now runs instead of the inherited
no-op. The native driver client retains the real 16 MiB transfer-memory work
buffer and process capability, binds its resource user to the actual process ID,
and receives a kernel-supplied PID. Allocation policy commits after its reply.
The next hardware video-device request is unavailable on the host; it must not
receive a fabricated successful device handle.


The [offline shader diagnostic](../integrations/belmante-runtime/offline/README.md)
uses the existing shader-only compiler libraries. Thirteen authored parser/profile
cases pass. The cache contains 4,487 pretranslated graphics/compute shader stages from inspected startup
assets, with Vulkan module acceptance and no pipeline/draw credit. Explicit
profiles cover normalized 2D/cube/array float sampling and depth comparisons on 2D or
array textures; live bindings must match before upload/draw. Cubemap storage uses
six faces. This is not complete shader coverage or proof of draw-state
compatibility. Seven older raw compute kernels now translate offline with actual
workgroup/shared metadata and checked native buffer bindings. The 3D noise-volume
path uses complete single-level RGBA8 API texels; an authored 4x4x4 volume passes
all 256 bytes through native Vulkan upload/readback. Raw tiled volumes, multisample
layouts and remaining resource classes still require integration.

The application-control adapter checks the supplied NACP digest, exact extent
and display-version string before replying. Its metadata remains private.

The font adapter maps seven privately supplied, verified system fonts into native shared memory and serves font metadata and language queries. No font contents are distributed.

The optional system-data adapter reads the owner's existing Mii archive through
an identity-pinned read-only image. Its local extraction verified the NCA section
hash and 357 IVFC blocks; neither system contents nor keys are published. Native
system mounts and ordinary file calls now use the same image reader. The local
user avatar database is empty. Requests that include the built-in default source
now return its six canonical presets instead of an empty list. Temporary
random-character generation uses a standalone
adaptation of the existing Mii algorithm and tables, with retained attribution.
Its regression checks 6,144 generated characters across descriptor combinations
and rejects invalid descriptors (24,580 checks). Database mutation and other
Mii service operations remain incomplete. FSP process
binding commits after its reply, and archive output is staged until reply completion.

Native audio now resolves the inspected SDK53 renderer and execution entry by
symbol, validates its layout getters, and decodes that SDK's command format.
The earlier audio-startup timeout is gone in the latest probe. The render thread
runs and sound-bank reads advance, but these probes are muted and do not prove
audible output or complete audio correctness. Local play-report and offline-friend
interfaces have begun integration; their remaining calls still stop explicitly.

The owned window does not activate or change the foreground. An expanded-cache
probe hit its 30-second bound while loading assets. A fresh 120-second probe
then reached menu-course assets and stopped at a missing shader after 40.9 seconds,
without a timeout. A later run with native Mii generation reaches the ZCull
storage query after 64.2 seconds. Owned processes closed, source and executable hashes stayed
unchanged, and the foreground stayed unchanged. These
observations establish startup progress, not correct rendering or full gameplay.

The adaptation reads DT_INIT from each module, reuses the existing native
parental contexts with separate restricted ports, derives supported capabilities
from Vulkan, and implements the inspected SDK 53.105 builder/descriptor layouts.
Native queue ownership, command-memory callback dispatch, scratch reservations,
focus/exit subscriptions, desktop input configuration and durable basic save-file
writes are integrated. Tests cover descriptor changes, ignored fields, signed
zero and NaN comparison, and basic save write/close/reopen behavior.

The ZCull storage-size query accepts a live texture or an exact SDK descriptor
copy that still names a live native texture. It reports no additional
client-managed metadata allocation. The SDK53 texture wire layout now puts the
device pointer at +8, the GPU address at +0x80 and depth/stencil aspect bits at
+0x54; a dedicated authored regression checks those offsets.

Line width is recorded and replayed against the native pipeline's supported
unit width. Positive finite point-size state is recorded, replayed and supplied
to checked native point-size vertex outputs; unsupported device sizes stop explicitly.
Other line widths and malformed state stop explicitly. Vulkan image
[memory requirements](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryRequirements.html)
account for resource allocation; these adaptations do not prove depth rendering.

A cubemap can supply a cube-array shader through a checked one-cube, six-face
native Vulkan view. Missing or incompatible required sampled views now stop;
they cannot substitute a dummy or a differently shaped base view. Ordinary
floating-point depth reads use the depth aspect with comparison disabled,
separately from comparison sampling. Native fixtures create the cube-array view
and verify all 64 bytes of an authored D32 upload/readback. This does not prove
the title's depth results or image correctness.

Copied sampler descriptors now retain their inspected SDK53 values instead of
falling through to default state. The decoder validates fields and the native
device, and pool registration checks its range. The actual comparison-sampler
binding advances with its original mode/function. Copied texture getters and
pool registration resolve the complete descriptor against a live native resource;
pool finalization retires its owned snapshots. Regression checks cover copied
queries/registration, malformed descriptors, pool bounds and retired identities.
Longer runs advance beyond 512 native presents while loading menu assets, then
previously crashed on a null Mii-model object. Those earlier presented-texture readbacks were
black; a file interrupted by the crash is excluded from image verification. Missing required resources still fail;
no fallback has been added for the earlier missing material binding.

Read-only copied-descriptor queries now use shared locks and an eight-entry
per-thread identity cache. Every hit rechecks the descriptor and object generation;
changed and retired identities still fail. Graphics allocator registration now
validates callback targets and executes the inspected compiled SDK setter instead
of discarding the callbacks. The missing graphics initialization is addressed by the native driver-client work below.

A separate owner-input CPU probe runs generated Mii data through the actual
compiled SDK raw verifier and parameter-accessor constructor/check. Both accept
all 6,144 authored descriptor/seed combinations, and an invalid control is
rejected. It does not run title entry or initialize a GPU. This rules out the
tested generated character layout as the cause of the current model failure;
an owned-process hardware trace then identified an all-zero game-time character
record. The default-source query had incorrectly returned an empty list. It now
returns six validated preset records with source tags and staged buffer writes;
the fresh native startup log confirms a count of six. All six also pass both
compiled SDK checks. The next actual startup run passes the former null-model failure and reaches
`nvnCommandBufferClearTexture`. Visible rendering still needs verification.

Texture clears now record the captured view, region and float color and replay
through native Vulkan. The current profile handles full XY subresources at one
mip, selected array/cube layers, full RGBA masks and supported uncompressed color
formats. Other clear profiles stop explicitly. A transfer-ordering regression
verifies all 128 bytes of a two-layer image: the selected layer clears to magenta
while the blue layer remains unchanged. Upload and clear have an explicit memory
dependency even when they would otherwise use the same image layout.
The next actual clear uses sRGB RGBA8 storage with a linear RGBA8 view and a zero
clear color. That alias is now supported for RGB endpoint colors, whose stored
encodings agree. Intermediate converting colors remain rejected. An authored
native sRGB upload/linear-view clear verifies all 64 stored bytes become zero.


Recorded sync fences now publish ordered native Vulkan fences after their
enclosing submit completes replay. Queue fences use the same native completion
objects. Waiting observes replay readiness and GPU completion, including in the
pipelined fixture; recording alone does not signal a fence. Conditions are
conservatively completed at the end of the enclosing submit. Regression checks
cover unissued waits, malformed arguments, submit/wait completion and polling.
The latest closed title run passes the observed clear and fence API, then stops
at an unresolved nonzero HDR-composition sampled-texture handle.

A new regression reproduces premature deletion of a saved texture descriptor:
a copied descriptor can resolve through a pool-owned snapshot, but replacing a
secondary slot incorrectly classified that snapshot as a retired texture. Cleanup
now requires the exact retired-texture key prefix. The original pool retains its
snapshot until pool finalization; the regression fails before this fix and passes
after it. All 35 component suites and 213 native GPU fixture checks pass.
The fresh title run passes the previous HDR binding failure and continues through
Luigi/kart loading into Peach resources, then stops at another missing offline
shader after 258.5 seconds. All 15 complete presented readbacks remain black;
there is still no verified game image.

Render-target recording now resolves copied color/depth descriptors to native
texture identities, using the same checked descriptor lookup as sampled resources.
Previously a copied target's raw pointer survived recording but could not resolve
to a backend image. An authored regression fails before the fix; the corrected
recorded clear verifies all 64 bytes of a red RGBA8 native target readback.
Command recording, attachment counts and descriptor-array bounds are checked.
The actual-title probe now resolves copied render targets and produces a complete
1920x1080 presented-texture readback showing the MK8 Deluxe startup logo. Root
inspection confirms a native startup image; menu/race playability and pixel
equivalence are still unverified. The additional
162 driver-family offline shader stages translate and pass native module creation;
module acceptance alone does not establish rendering correctness.

Scripted native input now reaches the Select a Mii dialog, whose six built-in
portraits render in the presented texture. A later run stops at a sampled texture
in the menu-course model. Its descriptor was re-registered into its own slot,
which deleted the saved source before attempting to clone it. Saved descriptors
now retain one reference per registered slot; replacement acquires its reference
before releasing the previous one. Copies can share the saved native texture
until the final owning slot is finalized. Regressions cover self-registration,
secondary-slot replacement, two-pool sharing and last-owner retirement; all 256
bytes of a rendered texture survive the self-registration test. All 35 component
suites and 224 GPU checks passed in that revision.
The checked native run then selects a preset and reaches the main menu; selecting
Single Player stops at the next unsupported application-service request. Races
remain unverified.

The application termination-result setter now validates and stages its opaque
32-bit value until after the IPC reply. Normal Windows exits report the stored
result, while explicit host errors retain their own code. The setter itself does
not terminate the process. The inspected SDK wrapper and
[AM command table](https://switchbrew.org/wiki/AM_services#IApplicationFunctions)
identify this operation. The game reports `0x2a2` (userland assertion) before
the original SDK aborts on its missing graphics-initialization precondition.

Reopening the game's own saved profile exposed Mii UpdateLatest. Its captured
SDK53 request contains an 88-byte character record followed by a 32-bit source
flag. The native user database is empty: checked refresh requests return the
not-found result without inventing a user record or modifying the input. Invalid
characters and malformed requests are rejected. The subsequent GetIndex lookup
also validates the character and reports no user-database entry without inventing
an index. Component checks cover these
results and staged termination-state updates. All 35 suites and 277 GPU/component
fixture checks pass. The fresh saved-profile probe is testing restart recovery
and continuation into Single Player; full save/reload gameplay remains open.

A saved-profile run then exposed a registration lifetime gap: the source could
retire while compiled SDK view getters ran outside the object-registry lock.
Registration now holds a scoped descriptor reference across those getters and
transfers it to the destination slot at commit. Invalid slots and repeated commits
are rejected; abandoned registration releases its reference. A deterministic native
fixture retires every source-pool owner between capture and commit, verifies the
rendered image remains live, and checks the final abandoned-owner cleanup. The
saved-profile startup has passed the earlier registration exception and returned
to the title screen. Its game-generated report increments the saved startup count
from one to two. Main-menu/race continuation is still being tested. Full resource ownership
and rendering equivalence remain separate, unfinished work.

The original abort message states that `nv::InitializeGraphics` is required.
Its inherited no-op has been replaced by a checked call to the statically compiled
SDK initializer. A native driver client now validates and retains the owned process
and the game's real 16 MiB transfer-memory work area after reply commit. It also
binds the native applet resource-user identity and records the requested allocation
margin policy for device setup. Abandoned preparation releases its reservation;
closing the caller's work-memory handle does not retire the service's reference.

The process pseudo-handle now resolves to a typed object carrying the host process
ID. GetProcessId validates process/thread capabilities, and HIPC supplies the owned
sender PID rather than trusting packet placeholder bytes. An authored packet test
rejects PID spoofing. The actual SDK initializer now returns and native title
rendering continues; all 35 suites and 315 GPU/component checks pass. The fresh
probe is identifying the first device needed by the video path. Native device-open
and video-decode backends remain unfinished and do not return fabricated FDs.
Recording an allocation policy does not establish firmware-memory allocation.

Missing required shader translations and unknown calls remain visible failures.
Shader/draw/present coverage, GPU upload/layout correctness, allocator callback
semantics, synchronization, complete input/audio and save/restart through gameplay
remain open. Some inherited handlers are placeholders or still need semantic
review; reaching a later boundary does not validate those handlers.

This runtime has a different memory/dispatch model and does not inherit the old
guarded runtime's ownership or floating-point verification. All prior candidates
and the partial atomic storage-read successor remain retained.

See the [integration recipe](../integrations/belmante-runtime/README.md).
Game code, generated modules, metadata, assets and binaries remain local.
No tag or download is published.

## Existing runtime campaign

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
| Public v6 feature/format fixture | 148 CPU checks and a separate 16-draw / 549-pixel / 213-check GPU run pass. Covers BC4 partial blocks/filtering, packed sRGB/swizzles, independent depth/provoking state, full uniform blocks, blending and exact untouched/masked bytes | Authored inputs only. Computed colors use UNORM8 tolerances; custom-border creation/metadata does not prove border sampling. Full declared uniform bounds remain required. |
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
