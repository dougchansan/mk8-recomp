# Native performance work

**R212 now advances into Water Park and completes its first lap.** A clean
24,000-target flight finishes Stadium first/15 points, performs the real native
480×270 RGBA8 readback, advances normally into the second Mushroom Cup race,
and drives through its underwater anti-gravity section. Reviewed frame 23,940
shows Water Park lap 2/3. The flight closes intentionally at 24,015 presentations
after 852.8 seconds without a fatal/crash, timeout or source/executable/saved-input/
foreground change. This verifies the transition and one second-course lap;
the complete second race and cup remain unverified. Native audio records
168,000 submitted / 167,992 completed packets at its last periodic sample,
with 7 dropped buffers in one of 84 reported mixer windows and zero underruns.
Subjective listening and physical controls remain separate gates.

A same-binary 6,000-presentation control removes per-draw frame-signature hashing
and reduces screenshots from every 60 to every 1,800 presentations. It closes
intentionally after 167.7 seconds without errors or pin/foreground changes.
The 3,900-presentation course range 2,101–6,000 averages about 38.5 FPS, compared
with 28.6 FPS in the earlier heavy-validation flight; median presentation time
is 0.50 versus 12.41 ms. These figures come from rounded 60-presentation reports
in separate shared-desktop flights with different NPC/scheduling outcomes and
two diagnostic settings changed. They do not isolate each diagnostic's cost or
establish a universal speedup ratio or sustained 60 FPS.

An independent native timestamp flight reports median queued GPU-part duration
17.05 ms/frame over 25 course windows (mean 16.25, range 10.03–20.28 ms), with
zero unavailable results. This indicates substantial GPU-side interval alongside
CPU replay. GPU preemption/clock effects remain possible; Windows clock/utilization
values are unavailable from this profiler's Linux-only counters, not measured zero.

The per-render-pass profile gives a different scope: median labelled total
4.36 ms/frame in ten course windows, with the largest label outside render passes
(clears/uploads/copies, 1.575 ms), followed by two full-resolution pass labels
(0.560 and 0.405 ms). Labels identify the first shader of a pass, not exclusive
shader cost; profiling and queue/clock differences prevent subtracting these
figures from the whole-part profile to assign a proven cause.

R213 therefore separates existing retired GPU timestamp intervals into replay
and presentation/non-replay submissions. It adds no GPU waits or queries and
does not change rendering or synchronization. The 40 suites, 2,261 GPU checks,
300-check sync/cutoff controls and GPU-busy/cache-disabled control pass; both
changed source analyses are clean. The actual 3,600-presentation classification
flight closes intentionally after 115.1 seconds without errors or changed pins/
foreground. The course windows report median replay 18.06 ms/frame versus
presentation/non-replay 0.05 ms/frame; class sums match their aggregate and all
query results are available. These are queued intervals, not isolated shader work.
The source recipe now reconstructs 193 generic adapted files.

R223 removes the uncached CPU-read penalty from diagnostic image downloads.
It prefers a host-visible, coherent and host-cached memory type, falls back to
the existing coherent selection, and adds explicit transfer-to-host dependencies
before the existing fence-completed reads. Upload buffers and native
TextureReadTexels staging are unchanged. `NX_CACHED_READBACK=0` retains the old
memory-type control. Synthetic masks reject cached-but-noncoherent candidates.

Three-repeat same-executable probes fill a 1920x1080 buffer on the actual GPU,
wait for its fence, convert mapped RGBA to RGB and verify every authored red pixel.
The measured median CPU conversion falls from 52.76 ms (type 1, flags 0x6) to
0.267 ms (type 3, flags 0xe). This isolates host read cost, not game FPS. A clean
6,000-target native flight preserves source/executable/input-save/foreground;
the final racing image was reviewed. Capture-adjacent 60-frame windows observe
presentation costs of 0.63/1.04/0.70/1.42 ms per frame, versus
13.31/12.52/12.81/13.43 ms in the preceding R222 flight. Separate workloads and
desktop scheduling remain limitations. Its 3,840-interval course window averages
53.93 FPS, with remaining hitches; sustained smooth 60 is not established.

All 40 component suites, 2,327 GPU checks, 300 sync/cutoff checks and the
259-check synchronous native texture readback control pass. Changed-source
analysis is clean. The source recipe reconstructs 198 generic adapted files.
The next trace samples compiled CPU calls and kernel waits with captures off;
coarse frame windows and inclusive call samples must not be treated as precise
CPU shares or per-frame percentiles.

See [Vulkan host caching and coherence properties](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html).

R221 removes exclusive serialization from the stable focus-policy read path.
Every call still validates the owned live window and reads visibility. State
transitions, parking and reentrant notifications retain an exclusive recheck;
there is no cached liveness decision. A new eight-reader/40-visibility-toggle
regression verifies writer progress and notifications. Thirty focus checks pass.
Three-repeat 16-thread controls observe median 232 versus 119 ns/call; single
thread cost remains about 18 ns. These are focus microbenchmarks, not FPS ratios.
A clean 6,000-presentation game run observes 52.36 FPS over 3,840 course intervals
and 957 intervals above 20 ms, versus R220's separate 46.91 FPS/1,902-interval run.
Late windows reach 60 FPS. Shared-desktop/scene variation and first-use stalls
still prevent a sustained smooth-60 claim.

R222 lets identical copied SDK texture descriptors share a positive identity
cache entry by GPU/pool hash. Each hit still validates the current registry
generation, object type and every field used by the original matching predicate;
misses and raw object pointers are not cached. The bounded table has 64 entries
per thread. `NX_TEXTURE_IDENTITY_SHARED_CACHE=0` retains the original eight-slot,
descriptor-address control. A same-executable authored 100,000-query probe across
256 copies of 32 textures observes median 250 versus 56.75 ns/query, while changed
fields and retired resources are rejected and a restored descriptor resolves.
Default, legacy and cache-disabled 2,327-check GPU fixtures, 40 suites and
300-check sync/cutoff controls pass. Both changed production analyses are clean.

R222's first 6,000-presentation game run completes without fatal errors and with
source/executable/input-save/foreground preserved. Its timing is contaminated:
unrelated clang/linker processes were observed during part of the course. Later
windows recover to about 60 FPS after those processes exit; the overall course
rate is 49.70 FPS and does not establish an FPS comparison. The longer native
flight has now closed at 24,600 presentations/515.89 seconds: Stadium first with
15 points, native 518,400-byte readback, and Water Park lap 2/3, with zero reported
audio drops/underruns. Stadium/Water Park rates are 44.29/55.74 FPS with median
windows 59.8/60.0; a severe Stadium wait interval remains unattributed. Sparse
captures also contributed stalls. This is not sustained smooth 60 or a full cup.
Full smooth 60 and correct 120 remain open.

R220 fixes Windows sleep granularity using one high-resolution waitable timer
per calling thread. Deadlines use relative 100 ns units and QPC rechecks; failures
or unsupported systems fall back to the previous sleep path. This preserves
requested waits without spinning or changing the global timer resolution.
`NX_HIGH_RES_SLEEP=0` selects the old path for comparison. Actual same-binary
0.5/1/5 ms sleep probes observe medians 15.58/15.56/15.61 ms with the old path,
versus 1.00/1.52/5.51 ms with the new path. Four concurrent timer checks preserve
independent minimum deadlines.

`NX_FRAME_PACING_STATS=1` measures intervals and CPU time on the actual
presentation caller, replay-worker wait, pacing requests and sleep overshoot.
It works without Wonder-specific SDK addresses. Windows thread CPU accounting
now uses GetThreadTimes in its documented 100 ns units, rather than returning
zero. Page faults and context switches remain unavailable on Windows.

Two clean 6,000-presentation same-executable controls use identical input, saved
profile and driver-cache seeds, with captures/signatures/audio dumps disabled.
Early pacing overshoot averages 6.33 ms/frame with the legacy timer versus
0.256 ms with the new timer; course overshoot falls from 0.745 to 0.024 ms/frame.
Overall course rates are 49.68 versus 46.91 FPS across 3,840 measured intervals.
The shared desktop and scene workloads vary, and the new timer does not yet
establish an overall FPS improvement or sustained smooth 60. Both runs preserve
source/executable/input-save/foreground and stop intentionally at the target.
CPU/GPU work, required waits and remaining first-use stalls still need reduction.

`NX_PIPELINE_CACHE_DIR=<existing directory>` optionally retains the local
driver-generated pipeline cache across build/test data roots. The default stays
the data root, `NX_PIPELINE_CACHE=0` still disables it, and Vulkan performs cache
compatibility handling. A preceding same-executable cold/warm pair reduces
course pipeline creation from 3,358 to 434 ms for 114 versus 113 pipelines, but
observes 50.12 versus 46.90 FPS; this is a cache-cost result, not an FPS speedup.
Never publish or bundle the generated cache as part of this source recipe.

All 40 suites, 2,327 GPU checks (including external-cache/disabled-cache arms),
300 sync and 300 cutoff checks pass. Windows CPU-work/sleep accounting and
concurrent timer probes pass; analyses of the four changed production units
are clean. The verified recipe now reconstructs 197 generic adapted files.

The next performance target is stable 16.67 ms frame delivery. Eventual 120 FPS
requires 8.33 ms CPU/GPU critical paths and verification of MK8D simulation,
physics, race clock, audio and input timing. The existing experimental display
rate setting alone is not verified 120 FPS support. No release or download.

References: [Windows high-resolution timers](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw),
[relative timer deadlines](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimer),
[thread CPU-time units](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes),
[Vulkan cache compatibility](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineCacheCreateInfo.html).

R218 enables grouping of consecutive counted draws by default into one precise
occlusion query within a render pass (`NX_COALESCE_SAMPLE_QUERIES=1`). Queries
close before pass end or internal helper work; explicit counted Draw brackets
remain distinct from masked or ordinary clears outside those brackets. Both
`vkCmdClearAttachments` paths are excluded because Vulkan permits driver-dependent
clear contributions. Reset/report boundaries, conditional rendering, cross-submit
accumulation, query capacity, CPU-visible writeback and required waits remain.
Explicit `NX_COALESCE_SAMPLE_QUERIES=0` retains the per-draw diagnostic control.

Authored tests verify 1/2/5/17 consecutive draws return 16/32/80/272 samples,
and two counted draws around either a masked helper or an ordinary ClearColor
return 32. All 40 suites and 2,327 GPU checks pass in default/grouped modes,
including grouped GPU-busy/cache-disabled controls and 300-check sync/cutoff
controls. Analyses of the five changed production units are clean. The recipe
reconstructs 195 generic adapted files; public CPU ABI/archives are unchanged.

An earlier same-executable R216 pair covers 1,500 course presentations: legacy
observes 38.36 FPS, median queued replay 16.46 ms and 897.52 precise queries/frame;
grouped observes 43.28 FPS, 8.27 ms and 208.28 queries/frame. The title workloads
average 897.52 versus 916.44 draws/frame; separate process/NPC/clock/scheduling
variation remains. Late grouped windows reach 57–59 FPS, not a sustained-fullspeed
claim. This pair precedes R217's ordinary-clear hardening and is provisional;
the hardened R217 long flight has now completed 24,600 presentations in
508.83 seconds without fatal errors. Stadium finishes first with 15 points, then
the native 480x270 RGBA8 readback succeeds and Water Park reaches lap 2/3.
Racing averages are 50.02 FPS over 11,280 Stadium presentations and 53.52 FPS over
6,600 Water Park presentations. Median 60-presentation windows are 58.6 and
59.5 FPS; these are not individual-frame percentiles or sustained 60 FPS proof.
Median GPU replay costs are 2.66 and 3.88 ms. Signatures were disabled but captures
still occurred every 600 presentations. Four audio drops and zero underruns were
reported; the real XAudio2 device was muted, so listening remains unverified.
R218's default/legacy/cache-disabled GPU checks, sync/cutoff checks and native
readback controls pass. The capture-free and timer comparisons are recorded above.
Full second-race/cup completion, smooth 60 and physical input remain open.
No release.

See the [Vulkan query scope and sample-count rules](https://docs.vulkan.org/spec/latest/chapters/queries.html).

A bounded async-2/pipeline-0/chunk-512 control passes the authored GPU tests but
reaches the unchanged 6 GB guard at 6.22 GB after at least 3,360 presentations;
recent course windows fall near 12 FPS. It is rejected. Default chunk 0 remains
in use, and no memory limit is raised. No release, tag or download is available.

The R201 profiling-disabled validation completes all three laps of Mario Kart
Stadium and renders first place/15-point results. It reaches 18,000 target frames
and closes intentionally after 770.9 seconds without a fatal/crash message or
source/executable/saved-input/foreground change. Frames 2,101–13,380 provide
11,280 racing intervals: mean 40.010 ms (about 25 FPS), median 26.5 ms. Sparse
readbacks remain enabled. This is an observed flight, not a controlled old/new
FPS ratio or a sustained full-speed claim.

The native game also flushes a post-finish save; a fresh process reloads the
exact game-generated copy and reaches mode selection. Audible output, physical
controls, broader gameplay and full-speed tuning remain open. No release or
download is available.

R202 also completes the three-lap race with the real XAudio2 device active at
zero voice gain: Mario first, 15 points, target reached at 18,015 frames, closed
intentionally after 663.8 seconds without a fatal/crash or changed source,
executable, saved input or foreground. The last periodic sample records 130,000
submitted / 129,992 completed native audio packets. All reported mixer windows
have zero dropped buffers and zero underruns. The same 11,280-interval racing
window averages 34.122 ms (about 29.3 FPS), median 21.3 ms; separate flights do
not establish a controlled audio or version speedup. The original SDK mix and
real device consumption are verified together with the race. Subjective
listening, physical controls, sustained full speed and wider gameplay remain open.

R207's real results transition now verifies the native readback: a 480×270
RGBA8 texture, 518,400 output bytes and completed GPU fence. A reviewed frame
then shows next-course loading. Loading stops at frame 16,378 on another required
array binding: the handle cache retains a valid texture pointer while a later
lookup of its key returns nothing. That flight completes the race first/15 points
but closes naturally at this loading blocker after 615.1 seconds, with pins and
foreground unchanged. This does not establish second-course gameplay.

R210 retains every first positive object selection until its replay ends, rather
than only post-mark fallback selections. An unchanged-runtime regression loses a
selected current object after generic registry retirement and fast-cache eviction;
the fix preserves its native view and red GPU pixels through retirement/recreation,
while current and next-replay state remain correct. That generic helper sequence
is a demonstrated resolver invariant failure, not a proven game producer sequence.

R212 also expires the descriptor-handle cache on the actual replay serial. Separate
replays may share one recording epoch; an unchanged-runtime regression retains
the first replay's post-mark descriptor after real slot clearing in the second
replay with the same epoch. The corrected second replay sees the cleared slot.
Positive choices remain stable inside one replay, and missing handles remain
retryable. Existing deferred ownership and shader/type checks remain in force.
All 40 suites, 2,261 GPU checks and 300-check sync/cutoff controls pass, including
all 2,261 GPU checks with the resolver fast cache disabled. A standalone synchronous
readback probe passes 259 real-GPU pixel/row-gap checks. Incarnation analysis is
clean; descriptor-set analysis retains one inherited nullable set-cache-slot
warning. The clean two-course R212 flight now passes as recorded above;
the complete second race and cup remain open.

R205's actual repeat now completes all three laps, FINISH at frame 13,860,
and Mario first/15-point results at 15,720. Normal result-advance input then
reaches the previously unsupported `nvnTextureReadTexels` operation at frame
16,120. The process closes naturally after 616.3 seconds; source, executable,
saved input and foreground remain unchanged. This verifies another completed
race and identifies a separate transition blocker, rather than a clean full
probe pass. Its racing window averages 35.770 ms (about 28 FPS), median 22.9 ms,
with frame signatures and sparse readbacks still enabled. No controlled speedup
or universal descriptor-stability claim follows from one additional pass.

R207 implements a bounded native GPU-to-CPU texture readback for uncompressed
2D color storage, including strided output. It waits preceding replay/GPU work,
applies pending texture writes, copies the actual native image to coherent
staging, and waits for a GPU fence before writing output rows. It preserves row
gaps and image layout; invalid regions, strides, output permissions and resource
profiles reject. Converted/compressed/depth/multisample/array/3D storage remains
outside this profile. Authored GPU regressions verify red subrectangle pixels,
strided canaries, pending blue texel writes and rejection without output changes.
The unchanged runtime fails the required-HLE regression first. All 40 suites,
2,240 GPU checks and 300-check sync/cutoff controls pass; all 2,240 GPU checks
also pass with the object resolver fast cache disabled. Analysis of the changed
Vulkan unit retains one inherited portability-chain dead-store warning. Those pipelined GPU tests did not separately exercise synchronous readback; the
subsequent standalone 259-check probe and actual game readback pass as recorded
above. Second-course gameplay remains unverified.

R205 fixes a second replay-cache edge case found during a repeat flight.
That R202 run stops during the first race at frame 4,081, before course advance:
a required array binding is missing while a retained registered snapshot exists.
An authored regression on unchanged production reproduces a stale empty handle
after a real pending descriptor registration commits in the same replay. The
handle cache now retains successful selections and retries missing ones, matching
the existing empty-at-mark lookup rule. It does not substitute missing resources
or relax shader/type checks. Tests verify the retained snapshot's metadata,
sampled view, exact red GPU pixels, positive selection after slot replacement,
and the replacement in the next replay. All 40 suites, 1,964 native GPU checks
and 300-check sync/cutoff controls pass; the full GPU fixture passes with the
object resolver's fast cache disabled too. Static analysis retains one pre-existing
nullable set-cache-slot warning outside this change. Actual repeat-race validation reaches first-place results. Next-course
validation continues with the new readback path. The regression does not prove universal
gameplay stability or fully establish the real failure's timing mechanism.

R201 adds a regression-backed lifetime correction alongside the performance work.
A temporary texture first resolved after the replay mark could disappear after
finalization and fast-cache eviction, despite its old object remaining deferred.
The first positive empty-at-mark selection is now stable for that replay; misses
are not retained, and the selection is discarded at replay end. Native GPU tests
cover finalization, replacement at the same address, cache eviction, real red-pixel
readback and current/next-replay visibility. All 40 suites, 1,950 GPU checks and
300-check sync/cutoff regressions pass. The 1,950-check fixture passes with the fast
resolver cache disabled as well. The actual full-race flight now passes, as recorded above.

R194 reduces the measured retained-texture admission hotspot. Its course probe
reports median identity time of 8.363 ms/frame, compared with 315.509 ms/frame in
the earlier R189 probe. The 900 measured race frames average 47.630 ms (about
21 FPS), median 35.1 ms. Different flights are not a controlled FPS speedup ratio.
Full-speed gameplay and broader coverage remain open; no download is offered.

R187 completes a profiling-disabled async flight to its 6,000-frame target without
a fatal error, stopping intentionally after 1,273.4 seconds. Native frames show
scripted racing through the first bend, anti-gravity section and glider jump.
The last reviewed image shows first place and Lakitu's 2/3 sign, with the HUD
still at 1/3. The longer R188 run verifies a complete first lap: frame 6,420
shows the HUD at 2/3 and continued driving in second place. A transient glider
frame has severe brightness clipping, with normal colours returning next; an
expected-game reference is needed before calling it a backend fault. Complete
races, visual correctness and playable performance remain open.
No release or download is available.

R188 retains native null sampled views and empty-draw handling, with 40 suites,
1,902 native GPU checks and separate 300-check sync/cutoff regressions passing.
It makes routine callback/scratch/property messages opt-in. Tracing off and on
produces identical GPU test receipts on the same executable. R187 produced
808,139 callback messages and a 39.2 MB log; this proves repeated diagnostic work,
not a measured frame-rate cost or speedup from removing it.

R187 race windows of 600 frames each range from 183.4 to 371.6 ms mean interval
(2.69–5.45 FPS). These are different sections of one race, with sparse image
readbacks enabled, not a controlled optimization comparison. A read-only
10.25-second CPU-accounting window during frames 3609–3650 shows one unnamed
thread using 9.625 CPU seconds (about 94% of one core); the identified replay
thread uses 0.625 CPU seconds (about 6%). This records CPU consumption, not a
function-level or GPU critical-path diagnosis. Call-level profiling is next.

R188's longer profiled flight reaches all 7,200 target frames without a fatal
error and is intentionally stopped after 1,527.4 seconds. Source, executable,
saved input and foreground remain unchanged. Across 87 reported 60-frame course
windows ending between frames 2,039 and 7,199, median focus time is an estimated
47.87 ms/frame, presentation exclusive time 16.80 ms/frame, draw exclusive time
11.33 ms/frame and logging 0.032 ms/frame. These regions overlap/nest and the focus
figure is sampled; do not add them into a CPU percentage or critical-path total.

R189 adds opt-in target-recording, texture-identity and SDK-view-read regions.
It also drops one redundant window query: `GetWindowThreadProcessId` returns
zero for an invalid handle, so the preceding `IsWindow` is unnecessary. Live
ownership/generation and visibility checks remain on every gate. Five alternating
benchmark pairs give single-thread medians 18.41 versus 20.204 ns/call and
16-thread medians 246.06 versus 267.856 ns/call. This reduces the authored gate
cost by about 8–9%; a game-frame speedup remains unmeasured. See the
[Windows API return contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid).

The completed R189 phase probe reaches its 2,700-frame target without a fatal
error after 471.4 seconds, with source/executable/save/foreground unchanged.
Across 15 reported 60-frame course windows ending at frames 1,859–2,699, median
texture-identity admission is 315.509 ms/frame, versus 0.265 ms/frame for original
SDK view reads. Target recording is 313.466 ms/frame inclusive and 1.034 ms/frame
exclusive: descriptor resolution dominates that path. These are elapsed scope
timings; do not sum nested or cross-thread values.

A private, component-verified experiment narrows the search using the existing
memory-pool index while retaining fresh full-descriptor checks and snapshot
fallback. It preserves all title pixels on the same executable, but the course
still spends 304.608 ms/frame in identity resolution across its 15 measured
windows. Pool narrowing alone has not resolved the main bottleneck. The next
probe records whether expensive results come from pool candidates or retained
resources reached through the full registry. No course-speedup claim is made.

The follow-up bounded trace identifies the missing resources: all 96 captured
full-registry results are retained E-key snapshots, while their pools have
1, 41 or 42 indexed live keys and the registry contains about 34,460 objects.
These are the first successful results per route/thread, not hit-ratio samples.

R194 adds a separate `(pool, GPU address)` snapshot admission index, maintained
at snapshot creation and final-reference retirement. Each hint is revalidated
against current reference ownership, registry type and every existing descriptor
field. Only a unique full match bypasses the full scan; ambiguity keeps the
legacy search. The existing generation cache, bounds/permission/device checks,
replay epochs and fallback remain. The flush index is unchanged. The completed
route probe is removed from production; aggregate phase timers remain opt-in.

The actual indexed course probe reaches 2,700 target frames without a fatal
error, stopping intentionally after 132.6 seconds (last presented frame 2,729).
Source/executable/save/foreground remain unchanged. Fifteen 60-frame windows
ending at 1,859–2,699 give median identity 8.363 ms/frame and target recording
7.677 ms/frame inclusive, compared with the earlier 315.509/313.466 ms figures.
These are elapsed, overlapping scope measurements, not additive CPU percentages.

A profiling-disabled same-executable title comparison over frames 121–180 gives:

| Snapshot admission | Median interval | Mean interval |
| --- | ---: | ---: |
| Legacy search control | 46.80 ms | 86.31 ms |
| Indexed search | 19.05 ms | 57.90 ms |

Frame 120 is identical across all 2,073,600 RGB pixels, with the same draw count
and shader order. It is one sequential title pair on a shared desktop; this does
not establish a full-race speedup or full rendering correctness. The fixture's
three alternating pairs pass 1,930 checks through both search modes, including
pending ownership, last-reference retirement, duplicate candidates and different
shapes sharing an address. The normal fixture passes 1,928 checks. Local lookup
microbenchmarks are retained as controls, not substituted for course timing.

The old code forced a zero instance count to one. GPU readback reproduces the
unintended pixel for both indexed and non-indexed draws. The corrected replay
skips zero counts/instances before resource resolution, retaining positive-draw
validation and positive single-pixel controls.

The later positive particle failure is distinct: 408 indices/one instance request
a cube view from a live BC3 one-layer 2D/array image. The reference Vulkan renderer
selects a null view when the requested typed view does not exist. Native
robustness2 nullDescriptor support now supplies the defined zero reads without
dummy images, shader retyping or manufactured cube faces. Explicit handle 0
samples also use this feature. Unregistered nonzero handles still fail, and the
path is unavailable when the physical device lacks/does not enable the feature.
GPU controls retain valid 2D/cube colours and verify null 2D/cube/shadow values.
See [native null-descriptor semantics](https://docs.vulkan.org/guide/latest/robustness.html).

## Measurements and changes

Heavy stadium windows in the earlier R164 run spent 1.93–2.29 seconds per frame
inside host command replay, compared with 22–23 ms in the recorded submission
fence waits. Presentation cost 140–185 ms. Warm pipeline creation was negligible.
Replay is elapsed host time, including nested work and descheduling. Subsequent
per-opcode and phase timings narrow a major early-scene cost to counter-report
destination admission: 3,241 ms across 300 reports, compared with about 9 ms for all
other report phases combined. This does not prove GPU saturation.

R175 caches successful span validations while the runtime's memory generation
is unchanged. Guest-map changes and every Win32 reserve/commit/decommit invalidate
all thread caches; concurrent mutations disable cache admission. Failures and
object liveness are not cached. Permission revocation, host backing removal and
restoration, cross-page spans and changes from another thread have regressions.
Future host mapping/protection changes must participate in this invalidation.

R179 also reuses validation throughout the exact intersection of the guest-VM
and Windows regions whose uniform permissions/protection were checked. Its
default still invalidates on every tracked mutation; it never expands beyond
either region boundary. Texture comparison uses a shared registry lock, avoiding
unnecessary generation changes. A regression verifies that read-only comparison
does not invalidate the registry and still observes changed values.

One same-executable comparison of title frames 121–180 gives:

| Validation | Median frame interval | Mean frame interval |
| --- | ---: | ---: |
| Uncached control | 219.45 ms | 230.14 ms |
| Successful-span cache | 48.85 ms | 60.30 ms |

That is a 4.49-fold reduction in median interval for this scene. Both runs retain
17 draws with the same shader-order signature, and frame 120 is byte-identical
across all 2,073,600 RGB pixels. It is one sequential pair on a shared desktop,
with profiling enabled. The profiling-disabled control and async case reach the
same image; course performance and full-game correctness remain open.

Query-pool reuse resets only previously used slots, retaining full capacity and
initial pool reset. Shrinking, empty and growing query sets pass. This reduces
redundant work, but the measured reset-recording cost was small compared with
destination validation. R176 removes the temporary per-opcode diagnostic layer
while retaining aggregate and counter-phase timing.

R172 introduces aggregate inclusive/exclusive timings and reduces optional logging
overhead. Its calibrated device clock avoids a submission for each current-clock
read when supported. An authored 64-read comparison measured 3.253 versus 92.617
microseconds/read. Both paths passed clock monotonicity, query bracketing and the
other GPU fixture checks. This is a local benchmark, not a game-frame speedup.

The focus gate now performs ownership, visibility and policy evaluation under
one locked section per iteration. It still checks display liveness and supports
notifications that reenter the policy. Five alternating authored benchmark runs
measured medians of 20.339 versus 34.107 ns/call on one thread and 283.345 versus
515.977 ns/call with 16 threads. Actual frame-rate effects remain under test.

## Diagnostic controls

| Variable | Behavior |
| --- | --- |
| `NX_NATIVE_PERF=1` | Aggregate host timings for replay, draws, compute, textures, descriptors, pipelines, uploads, state, clears, copies, list lookup, presentation, clock reads, focus, logging, target recording, texture identity and SDK view reads |
| `NX_BUFFERED_LOG=1` | Buffer routine file logging, flushing on subsequent writes at least every 250 ms; fatal messages flush immediately |
| `NX_LOG_STDERR=0` | Suppress duplicate routine stderr messages; fatal stderr remains enabled |
| `NX_TRACE_NVN_CALLS=1` | Enable routine command-callback, scratch-reservation and device-property messages |
| `NX_STALL_LOG=0` | Disable the per-frame stall sampler; the separate hang watchdog remains active |
| `NX_GPU_CLOCK=submitted` | Force the existing submitted-query clock for comparison |
| `NX_SPAN_CACHE=0` | Force uncached guest/host span validation for comparison |
| `NX_TEXTURE_POOL_INDEX=0` | Force legacy whole-registry copied-texture search instead of pool/snapshot hints |
| `NX_FULL_QUERY_RESET=1` | Reset entire query pools on reuse for comparison |
| `NX_SPAN_RANGE_CACHE=0` | Retain only exact-start span reuse, disabling checked-region reuse |
| `NX_CPU_CALL_PROFILE=1` | Sample indirect compiled/HLE calls every 4093 calls and rank inclusive thread CPU cycles |
| `NX_SVC_PROFILE=1` | Existing marked-thread SVC wall profiling, extended with CPU-cycle totals |
| `NX_NATIVE_NULL_DESCRIPTORS=0` | Disable native null views; unbound/incompatible required views retain their strict stop |

Other diagnostics remain opt-in and can be omitted from timing runs. Profiling is
off by default. Synchronous log behavior remains the default. Forced process
termination may discard the final buffered routine messages.

Focus profiling samples one in 1,024 calls and scales the sample count and duration.
Those values are estimates. Other timing regions are inclusive and exclusive of
instrumented children on the same thread; regions on different threads can
overlap. Scopes enter the reporting window when they finish. Counters are collected
independently, so concurrent window boundaries are approximate. Do not sum all
inclusive numbers as a frame-time total. Use profiling-disabled controls to check
measurement overhead.

The CPU-call profile is per thread, bounded to 4096 targets per window, with
dropped samples reported. Its cycle totals are inclusive and sampled, not CPU
percentages. SVC profiling retains its 30-microsecond filter and marked-thread
scope. Both use [thread CPU cycles](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime)
on Windows and report successful CPU samples explicitly; cycles must not be
converted into nanoseconds. These profiles are off by default.

The earlier R179 async flight reaches the native player grid before the preserved
arena check. Course windows report 8.3–9.9 FPS, host replay 5.10–7.42 ms and fence
wait 14.21–15.14 ms per frame. Earlier runs used different scene/timing conditions,
so these numbers are not a controlled course-speedup ratio. The failure requests
512 bytes at offset 65264 in a 65536-byte pool, exceeding its extent by 240 bytes.
R182 handles this exact advertised-minimum case by exposing only the physically
backed tail as native recording capacity. The original wrapper's available 112
bytes are rounded to 512 for the call while 272 physically remain. All consumed
ranges stay inside the pool. Negative regressions retain rejection of end-of-pool,
arbitrary oversize and exhausted-tail recordings. It now reaches countdown and
scripted driving through the first bend, then stops on a cubemap/2D particle
binding mismatch. Complete race and playable performance are still open.

The calibrated clock uses only the Vulkan DEVICE domain. It preserves the same
units and epoch as command timestamps, performs no CPU-clock extrapolation, and
retains the submitted-query path when the extension/domain is unavailable, a call
fails or reported deviation exceeds 1 ms. See the Vulkan specification for
[device-domain timestamp comparability](https://docs.vulkan.org/refpages/latest/refpages/source/VkTimeDomainKHR.html)
and [calibration uncertainty](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetCalibratedTimestampsKHR.html).

## Remaining work

Compare identical source, executable, inputs, scene/frame ranges and diagnostic
settings. Record cache warmup and rendering evidence. Existing pipeline/view and
within-submission upload/descriptor caches already perform reuse; the measured
validation cache avoids duplicating those layers. CPU/GPU overlap remains an
experiment until retained resources, writebacks, counters and actual game images
are verified. Do not remove waits or bounds checks merely to advance a test.

## Further controls and native audio-device verification

The existing combined pipelined/chunked mode passes authored GPU/counter controls
with chunk size 1, but actual MK8D with async 0 / pipeline 2 / chunk 150 stops at
frame 1,330 when private memory reaches 6.10 GB against the 6 GB guard. Baseline
async 2 / pipeline 0 completes its 2,700-frame control. No course-speedup comparison
is valid and the experimental combination remains disabled; the guard is not
raised. Its growth mechanism remains undiagnosed.

R202 adds an opt-in silent real-device check, `NX_MUTED_AUDIO_DEVICE=1`, alongside
`NX_MUTE=1`. The source voice is set to gain zero before it starts; normal muted
and headless behavior is preserved. XAudio2 accepts the original SDK renderer's
PCM and returns real buffer-completion callbacks. The 1,000-frame game probe
reaches its target without a fatal/crash or changed source/executable/save input/
foreground; logs show 6,000 submitted and 5,990 completed packets at the last
periodic sample. The private 10-second WAV is 48 kHz, stereo, signed 16-bit,
480,000 frames, with 862,411 nonzero samples, peak 9,588, RMS about 1,055 and zero
clipped samples. This proves data/device processing, not subjective listening.
An authored device probe also rejects invalid buffers and returns four distinct
contexts with queue zero. Failed submissions now release their packet and queue
count, and the renderer recycles rejected blocks. Hotplug/recovery is unverified.

See the XAudio2 contracts for [zero-gain silence](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2voice-setvolume)
and [data lifetime through OnBufferEnd](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-submitsourcebuffer).
