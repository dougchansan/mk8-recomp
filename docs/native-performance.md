# Native performance work

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
