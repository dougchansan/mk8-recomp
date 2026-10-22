# Presentation rate and simulation speed

The experimental `NX_DISPLAY_HZ` override changes the host display event and
presentation cadence. Raising it is not a verified high-refresh game mode.

R324 Time Trials Solo on Mario Kart Stadium, at 150cc with Mario and the standard
kart, tires and glider, produced these stationary HUD observations:

| Display override | HUD timer delta | Host wall delta | Approximate timer rate | Presentations/s |
| --- | ---: | ---: | ---: | ---: |
| 60 Hz | 31.401 s | 31.49 s | 1.00x | 59.88 |
| 360 Hz | 69.997 s | 31.52 s | 2.22x | 133.28 |

Host timestamps bracket each capture. Capture cost, displayed-image age and
frame quantization limit precision; these are approximate rates. The large
speedup is clear, and it agrees with the observed presentation rate divided by
60. The 360 Hz run's 7.503 ms mean interval therefore cannot establish
normal-speed 125 FPS. Earlier uncapped performance tables likewise describe
throughput, rather than validated normal-speed gameplay FPS.

The default 60 Hz behavior is retained. True 125 FPS and then 250 FPS require
rendering to advance independently of the original simulation cadence, with
clock, physics, camera, animation, input and audio verification. Repeated images,
extra simulation ticks or global OS-clock scaling do not prove that result.
The HUD timer alone also cannot prove preservation of every physics behavior.

Opening this verification mode exercised two previously unsupported friend
service calls. The native offline provider now retains a session-owned local
presence snapshot and accepts an idempotent close of the offline session. It
validates the selected local user, request layout, process identity, buffer extent
and reply commitment. The fixed presence buffer and command layouts are recorded
in [SwIPC interfaces](https://reswitched.github.io/SwIPC/ifaces.html) and
[SwIPC types](https://reswitched.github.io/SwIPC/types.html). The implementation
does not claim an online session or transmit presence. Thirty-five added controls
cover malformed requests, identity, commitment, repeated close and revoked
storage. The Time Trials path then reached racing without a native fatal error.

Future performance reports must state the pacing and simulation qualification.
An isolated minimum, GPU-only time or accelerated race does not prove a sustained
8 ms or 4 ms presentation target at normal speed.

## Further private cadence experiments

Subsequent closed, stationary Time Trials Solo captures on Stadium used the
R330 renderer and private opt-in scheduling prototypes. The prototypes remain
unpublished and disabled; the public recipe and normal 60 Hz play setting are
unchanged.

| Condition | HUD delta | Host delta | Approximate timer rate | Presentations/s | Qualification |
| --- | ---: | ---: | ---: | ---: | --- |
| R330, 60 Hz, passive worker profiling | 46.324 s | 46.305 s | 1.0004x | 60.00 | Stationary clock baseline |
| Private nested-cohort governor, requested 125 Hz | 75.370 s | 42.574 s | 1.7703x | 106.22 | Rejected: accelerated clock |
| Private broader update governor, requested 125 Hz | 31.907 s | 31.893 s | 1.0004x | 124.77 | Rejected: missing rendered geometry |

The baseline's 2,777 contiguous intervals averaged 16.6669 ms, with p99
18.7728 ms and minimum 12.9982 ms. These measurements include diagnostic
overhead and do not cover moving physics or other courses.

The broader governor's 3,978 contiguous intervals averaged 8.0149 ms, with p99
15.8882 ms; about 49.7% exceeded 8 ms. Reviewed images alternated between a
visible course/kart and black geometry with HUD/effects retained. Those intervals
include incomplete rendered states and cannot establish a performance gain or
normal-speed 125 FPS. Timer agreement alone did not make the experiment correct.

Host intervals use capture midpoints, with displayed HUD rounding, capture
duration and image age limiting precision. Neither prototype provides verified
interpolation, moving physics/input/audio equivalence or the 8 ms/4 ms milestones.
The next scheduling change must separate simulation from the preparation that
rendering needs on every presentation. A zero owner-thread draw count is
insufficient evidence for that boundary; see the
[worker profiling limits](native-main-loop-profiler.md#worker-ownership-and-render-preparation).
