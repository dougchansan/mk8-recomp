# Dispatch diagnostics and sweep attribution (R282)

R282 moves optional CPU-call profiling and codec tracing out of the ordinary
indirect dispatcher. The static generated CPU modules are unchanged. Every call
still checks focus, resolves the same compiled function or HLE target, propagates
the same registers and retains unsupported-target failures. Profiling and tracing
remain available through `NX_CPU_CALL_PROFILE=1` and `NX_TRACE_CODEC`.

The R280 CPU diagnostic estimates millions of dispatch/focus calls per 60-frame
window. The old dispatcher carries profiling state and codec string lifetime
handling even when both diagnostics are disabled. The outline keeps that state
in separate helpers. This targets dispatch overhead; it does not remove the
focus checks that also cost substantial time.

## Authored dispatch measurement

The real loader is exercised with authored compiled/HLE functions and a stub
focus gate. The fixture checks one focus call per dispatch, return registers,
an untouched register, relocation and required-import failures. Plain, profiling,
tracing and combined controls pass, including eight expected profiling samples.

Eight alternating baseline/candidate runs each dispatch 20 million compiled calls
and 20 million HLE calls. Medians across four runs per revision:

| Dispatch | R281, unchanged R280 loader | R282 |
| --- | ---: | ---: |
| Compiled target | 6.649 ns | 3.521 ns |
| HLE target | 7.042 ns | 4.555 ns |

These are isolated dispatch measurements, not whole-game FPS gains. To reproduce,
build the recipe's `loader_symbol_test` and set `NX_DISPATCH_BENCH=20000000`.
Do not enable CPU-call profiling for the timing comparison.

Both revisions pass 44 component suites. R282 also passes 3,212 native GPU checks,
300 synchronous counter checks and the zero-cutoff/300-counter control. Changed
loader static analysis reports zero diagnostics. Generated-source/archive hashes
and source hashes remain stable through the builds.

## Gameplay controls

Two 180-second plain flights use the same saved-profile input, immutable pipeline
seed, 360 Hz display ceiling and closed-loop input driver. Heavy per-frame thread
attribution is off. The revised driver chooses engine class once per menu visit,
so it cannot repeatedly send Down while that screen transitions. An earlier pair
selected different characters and is excluded from performance comparisons.

The revised pair shows Mario in Stadium in both captures and has zero logged
fatals or shader misses. Over 91 reporting windows (5,460 intervals, reporting
frames 3,600–9,000):

| Observation | R281 | R282 |
| --- | ---: | ---: |
| Median reported FPS | 59.5 | 61.3 |
| Mean presentation interval | 18.426 ms | 15.906 ms |
| Intervals over 20 ms | 1,909 | 1,427 |
| Largest interval | 680.774 ms | 713.502 ms |

These sequential desktop flights differ in race progress, items and scheduling;
engine class was not independently recorded. They establish a clean gameplay
control and observations, not a causal whole-game FPS gain. The isolated dispatch
reduction above is the verified optimization. Source, generated archives,
executables and the saved-profile input remain unchanged after the runs. The
post-flight synchronous readback control passes 259 checks.

## Limits of the four-hour R280 sweep

The run reached its time limit after 40 course loads, covering 17 distinct
courses. Repeated races and ceremony loads mean this is not an all-cup result.
There was no logged fatal; one missing shader was collected under the existing
skip-and-collect diagnostic mode. That run does not establish complete rendering.

The original per-course report counts individual `slow frame:` lines, but those
lines stop after a process-wide default budget of 5,000. Later courses therefore
appeared to have zero hitches. In fact the 600-present summaries contain 360,968
late intervals. Across those summary buckets, presenter kernel time totals
5,559,881 ms and replay back-pressure only 49.3 ms. Wake records predominantly
name the main game thread. This supports investigating the main-thread dependency;
it does not identify an exclusive function cost or prove all hitches CPU-bound.

The largest retained Mario Circuit interval is 737.85 ms, alongside 151.77 MB
uploaded and 83.68 ms pipeline creation on the worker. Toad Harbor's largest
retained interval is 616.02 ms, alongside 126.52 MB uploaded and only 0.08 ms
pipeline creation. Worker counters overlap presenter time. These observations
distinguish pipeline activity from large transfer bursts without proving the
transfers caused the entire stall. Later-course maxima are unavailable once the
detail budget is exhausted.

`python scripts/native_sweep_report.py <closed-drive-directory>` preserves
summary buckets and their course-boundary ambiguity, emits observed maxima and
marks the reporting limitations. Its regressions cover exhausted detailed logs,
boundary buckets, repeated loads, path slash variants and coarse CPU-time residuals.
Never add detailed counts to summary counts: they overlap. The final incomplete
600-present bucket can be absent. Load-based sections include loading and menus.

First occurrence of each course in this diagnostic, with reported 60-frame FPS
window median/minimum (including loading/results, not an isolated race benchmark):

| Course | Median | Minimum |
| --- | ---: | ---: |
| Mario Circuit | 81.9 | 35.4 |
| Toad Harbor | 50.8 | 20.4 |
| Twisted Mansion | 75.3 | 22.7 |
| Shy Guy Falls | 73.8 | 11.7 |
| Sunshine Airport | 82.6 | 14.3 |
| Dolphin Shoals | 62.0 | 12.8 |
| Electrodrome | 70.5 | 13.7 |
| Mount Wario | 54.0 | 10.9 |
| Cloudtop Cruise | 45.9 | 16.0 |
| Bone-Dry Dunes | 44.3 | 15.9 |
| Bowser's Castle | 43.0 | 11.8 |
| Rainbow Road | 44.4 | 15.7 |
| Yoshi Circuit | 29.0 | 9.3 |
| Excitebike Arena | 27.6 | 14.7 |
| Dragon Driftway | 32.3 | 13.1 |
| Mute City | 32.1 | 12.4 |
| Baby Park (partial) | 28.4 | 12.1 |

Remaining work includes complete cup coverage with reliable selection, profiling
the main thread's remaining work, targeted multi-second hitch measurements with
sufficient detail retention, and attribution of slow video-panel menus. The
shadow-companion copy path is unchanged because this evidence does not establish
it as the bottleneck. No release, tag or binary download is provided.
