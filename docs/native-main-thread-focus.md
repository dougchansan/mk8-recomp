# Main-thread profiling and focus gates (R284)

R284 moves the optional profiling frame and the parking/notification loop out of
the ordinary focus gate. Every call retains the same shared lock, live-window
owner/generation check, visibility checks and policy decision. The slow path
retains its exclusive recheck, notifications outside the lock, reentrant callback
behavior and five-millisecond parking sleep. Generated CPU code is unchanged.

## Main-thread evidence

A private R283 diagnostic samples one focus gate in 4,093 and reports phases by
host thread. The startup main thread is TID 65000. Ten course reports spanning
frames 4,800–10,200 estimate about 37,600–49,100 gates per frame on that thread.
Sampled mean phases range from 96.7–112.0 ns for acquiring the shared lock,
59.4–96.9 ns for ownership, 73.1–82.7 ns for visibility and 78.4–87.8 ns for
release/other work. These include observer overhead, clock quantization and
descheduling; they are not isolated API costs. The diagnostic is disabled in
all speed comparisons and is not part of the published runtime.

The R282 gate's compiled entry saves eight integer registers and reserves 216
stack bytes, including machinery for paths ordinary calls do not take. R284
outlines those paths without caching policy/window state or reducing checks.

## Measurements and correctness

Short alternating controls first observe single-thread median gate cost
22.42 → 19.50 ns, with noisy 16-thread results 117.18 → 119.58 ns. To investigate
that possible regression, a second identical authored fixture lengthens each
single-thread run to 10 million calls and each 16-thread run to 3.2 million
total calls. Eight measurements per revision/mode, in balanced alternating order:

| Gate benchmark | R282 | R284 |
| --- | ---: | ---: |
| Single thread | 25.288 ns | 23.896 ns |
| 16 threads, aggregate throughput | 114.337 ns | 113.572 ns |

The longer single-thread median is about 5.5% lower. Contended throughput is
effectively unchanged; there is no demonstrated contention or whole-game FPS
gain. The fixtures use a real owned visible window and retain assertions for
parking/resume, visibility changes, reader/writer concurrency, notification
delivery, reentrant callbacks and rejection of a destroyed window. The same
controls pass with aggregate native profiling enabled.

Both revisions pass 44 component suites. R284 passes 3,212 native GPU checks,
300 counter synchronization checks and the zero-cutoff/300-counter control.
Static analysis of the changed focus and queue units reports zero diagnostics.
The generic recipe reconstructs 212 files; generated CPU inputs/archives are
unchanged. No release, tag or binary download is provided.

Two 180-second plain Stadium controls use the same saved input and immutable
pipeline seed, with heavy attribution off. Both captures show Mario at lap 2/3;
both runs have zero logged fatal/crash or shader miss. In the selected 91 windows
(5,460 intervals; reporting frames 3,600–9,000):

| Gameplay observation | R282 | R284 |
| --- | ---: | ---: |
| Median reported FPS | 65.9 | 63.8 |
| Lowest reported FPS window | 24.4 | 31.7 |
| Mean presentation-call interval | 15.891 ms | 15.989 ms |
| Intervals over 20 ms | 868 | 1,165 |
| Largest interval | 741.187 ms | 621.929 ms |
| Minimum presentation-call interval | unavailable | 0.957 ms |

This pair does not demonstrate a whole-game FPS gain. Race progress, items,
scheduling and unrecorded engine-class selection limit comparison. The isolated
single-thread gate reduction is the optimization result. The 0.957 ms minimum
is an isolated call-entry interval, not GPU render duration, monitor scanout or
a sustainable frame rate. Post-flight source/executable/generated-input/archive/
saved-profile pins are stable, and synchronous readback passes 259 checks.

## Per-course readings and frame-time limits

The retained sweep covers 21 distinct courses across R278 and R280. The table
uses the first occurrence of each course, not the later repeats. FPS is reported
in 60-frame windows, and these load-based sections include loading/results and
varying diagnostics. Values are not clean race benchmarks or R284 performance.
Twisted Mansion has a collected shader miss under the existing skip-and-collect
mode. Baby Park is partial. Remaining courses are unmeasured.

| Course | Revision | Median FPS | Lowest FPS window |
| --- | --- | ---: | ---: |
| Mario Kart Stadium | R278 | 72.1 | 27.3 |
| Water Park | R278 | 99.5 | 41.4 |
| Sweet Sweet Canyon | R278 | 88.4 | 14.8 |
| Thwomp Ruins | R278 | 81.2 | 15.8 |
| Mario Circuit | R280 | 81.9 | 35.4 |
| Toad Harbor | R280 | 50.8 | 20.4 |
| Twisted Mansion | R280 | 75.3 | 22.7 |
| Shy Guy Falls | R280 | 73.8 | 11.7 |
| Sunshine Airport | R280 | 82.6 | 14.3 |
| Dolphin Shoals | R280 | 62.0 | 12.8 |
| Electrodrome | R280 | 70.5 | 13.7 |
| Mount Wario | R280 | 54.0 | 10.9 |
| Cloudtop Cruise | R280 | 45.9 | 16.0 |
| Bone-Dry Dunes | R280 | 44.3 | 15.9 |
| Bowser's Castle | R280 | 43.0 | 11.8 |
| Rainbow Road | R280 | 44.4 | 15.7 |
| Yoshi Circuit | R280 | 29.0 | 9.3 |
| Excitebike Arena | R280 | 27.6 | 14.7 |
| Dragon Driftway | R280 | 32.3 | 13.1 |
| Mute City | R280 | 32.1 | 12.4 |
| Baby Park, partial | R280 | 28.4 | 12.1 |

The fastest individual presentation interval was not retained for these sweeps.
It cannot be recovered by inverting the lowest FPS window: that gives the
slowest 60-frame average, not a minimum individual frame time. Retained longest
hitches include 722.7 ms in Stadium, 363.9 ms in Water Park, 3,306.2 ms in Sweet
Sweet Canyon, 2,889.2 ms in Thwomp Ruins, 737.9 ms in Mario Circuit and 616.0 ms in
Toad Harbor. Later individual hitch maxima are unavailable after the detail cap.

R284 appends `min <milliseconds> ms` to the existing optional
`NX_FRAME_PACING_STATS=1` 60-presentation report, alongside its mean/peak/late
count. This records the fastest actual presentation interval in every reported
window, not a reciprocal-FPS estimate, without adding waits or GPU work. These
are intervals between presentation-call entries, not GPU render or scanout times. The
reporter preserves these minima and delivery peaks after detailed hitch logs
are capped. It excludes windows spanning course boundaries from course-specific
minima/maxima and leaves legacy minima unavailable. Four regressions cover legacy
logs, detail caps, boundaries and new minimum/maximum aggregation.

Remaining work: exclusive causes of the multi-second hitches, main-thread work
beyond focus gates, video-panel menu profiling and reliable complete course
coverage. This change does not establish that focus gates caused the large stalls.
