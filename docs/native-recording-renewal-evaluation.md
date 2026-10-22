# Empty-recording allocation evaluation

R350 excludes the experimental reuse of an empty unpublished recording. The
isolated BeginRecording API becomes cheaper, but complete recording controls
do not establish a reliable benefit. The production recording path remains the
verified R347 implementation. Sustained 8/4 ms presentation and correct
125/250 FPS remain open.

## Native gameplay controls

Four closed Stadium Time Trials flights use baseline/candidate/candidate/baseline
order, the same seeded save, pipeline catalog, shader cache and normal 60 Hz
configuration. In-process CPU, phase and GPU profiling are disabled. An owned
bootstrap-thread handle supplies CPU durations and raw cycles; identical window
captures add overhead. Each flight has a stationary start-line window and an
acceleration window that ends in wall contact. Engine class was not independently
reviewed for these four flights.

| Condition | Baseline main CPU, ms/present | Candidate main CPU, ms/present | Baseline presentation mean, ms | Candidate presentation mean, ms |
| --- | ---: | ---: | ---: | ---: |
| Stationary | 5.8763 | 5.7652 | 16.6658 | 16.6660 |
| Acceleration and wall contact | 7.2441 | 6.0930 | 16.6646 | 16.6713 |

The eight windows retain 13,778 presentation intervals, with no frame-ID gaps,
fatal errors, shader misses, truncation or sampled competing builds/games.
Reviewed HUD-clock bounds span approximately 0.9973–1.0022 game seconds per host
second. Presentation percentiles use individual intervals with 500 ms removed
at each capture boundary. Main CPU is measured separately; it is not renderer
CPU, GPU time or an additive part of those overlapping measurements.

The stationary CPU difference is about 0.111 ms, or 1.89%, across only two flights
per executable. Baseline acceleration CPU varies from 6.66 to 7.83 ms; one
candidate acceleration capture starts about 19 seconds later after a controller
file-read failure. Those acceleration group means do not establish a causal
improvement. The stationary p99 presentation ranges overlap: 20.469–20.735 ms
baseline and 20.594–20.626 ms candidate. No presentation improvement is claimed.
These short controls are not complete raced laps or deterministic replays.

## Why the API result was insufficient

The candidate saves one allocation only when the recording is unique, empty and
unpublished. Shared, published, nonempty, blob-bearing and null cases retain
fresh allocation. Reset preserves all fields and identity guarantees. The native
strict Begin-only median changes from 47.926 to 20.683 ns/call. A separate
record-only checksum control, whose timer excludes Begin, instead changes from
70.859 to 75.090 ns/record in a 16-process repeat.

A private lifecycle driver therefore includes Begin, the original command/blob
record loop and End in the same timed scope. Each process retains all 49,286
assertions, 37,888 records, checksum 557526016 and 192 valid raw cycle samples.
The actual previous and candidate command objects share the same driver and
91 other runtime objects; all 99 common runtime headers match. A separate
same-binary control toggles reuse. Raw cycles remain cycles.

The first 24 lifecycle controls are noisy. An additional 64 controls pin only
the fixture's main thread to one logical processor in each measured L3 group
(96 MiB and 32 MiB). Each pinned comparison has eight processes per variant.
Previous/candidate medians are 87.561/101.609 ns per record on the first selected
processor and 104.409/105.643 on the second. Same-binary controls also vary.
Pinning does not establish a consistent benefit or fully explain the spread.
Allocation alignment, cache placement and scheduling observations do not by
themselves prove causality. The candidate is excluded instead of promoting the
Begin-only ratio as a frame-time gain.

## Retained verification improvements

R350 carries the bounded uniform observer and its 32 configuration checks. The
authored counter geometry helper now closes an existing rendering scope before
opening a different target and shares the current scope for consecutive counted
draws. Query counts, assertions, draws and retirement waits remain intact.
All 53 native Windows suites and 17 GPU/control cases pass, including 3,253 GPU,
300 sync, 300 cutoff, 259 readback and six 412-check phase modes. Three source
analyses report zero diagnostics. The primary Windows hardware checks do not
use a Khronos validation layer; earlier software validation established the
fixture nesting error and qualified the repair.

The exact R350 executable also closes a separate final correctness smoke with
2,932 reviewed racing intervals. Stationary and acceleration presentation means
are 16.66547 and 16.66484 ms, with p99 20.5286 and 20.4948 ms. Both capture pairs
show normal HUD-clock speed, coherent scenes and no fatal error, shader miss or
guard exclusion. This smoke is separate from the four-flight comparison and
does not establish a performance improvement.

The earlier joint CPU/GPU capture verifies the observer, but its capture budget
fills within one observed frame and receiver identity remains unresolved. See
[render-state ownership](native-render-state-lineage.md) and
[capture configuration](native-timing-measurements.md#bounded-gpu-uniform-capture).
Private profilers, compiled title modules, selectors, payloads, shaders, caches,
captures and binaries are excluded from the source recipe.
