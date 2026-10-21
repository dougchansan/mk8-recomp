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
