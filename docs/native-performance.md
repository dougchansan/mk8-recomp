# Native performance work

The current runtime has reached native stadium rendering and the player starting
grid. It has not reached verified driving or playable performance. No release or
download is available.

## Measurements and changes

Heavy stadium windows in the earlier R164 run spent 1.93–2.29 seconds per frame
inside host command replay, compared with 22–23 ms in the recorded submission
fence waits. Presentation cost 140–185 ms. Warm pipeline creation was negligible.
Replay is elapsed host time, including nested work and descheduling; these numbers
do not identify a particular replay function or prove GPU saturation.

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
within-submission upload/descriptor caches already perform reuse; add or change
invalidation only where finer measurements justify it. CPU/GPU overlap remains an
experiment until retained resources, writebacks, counters and actual game images
are verified. Do not remove waits or bounds checks merely to advance a test.
