# Native performance work

Earlier builds reached native stadium rendering and the player starting grid.
R176 has not reached verified driving or playable performance. No release or
download is available.

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
| `NX_NATIVE_PERF=1` | Aggregate host timings for replay, draws, compute, textures, descriptors, pipelines, uploads, state, clears, copies, list lookup, presentation, clock reads, focus and logging |
| `NX_BUFFERED_LOG=1` | Buffer routine file logging, flushing on subsequent writes at least every 250 ms; fatal messages flush immediately |
| `NX_LOG_STDERR=0` | Suppress duplicate routine stderr messages; fatal stderr remains enabled |
| `NX_STALL_LOG=0` | Disable the per-frame stall sampler; the separate hang watchdog remains active |
| `NX_GPU_CLOCK=submitted` | Force the existing submitted-query clock for comparison |
| `NX_SPAN_CACHE=0` | Force uncached guest/host span validation for comparison |
| `NX_FULL_QUERY_RESET=1` | Reset entire query pools on reuse for comparison |

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
