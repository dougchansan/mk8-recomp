# Retiring completed sample queries

The GPU phase profile found 3.0-3.7 ms per sampled racing submission in the
counter-resolution tail. An authored workload isolated query copying as the
dominant cost. R324 collects eligible, completed query results at an existing
full retirement instead of resolving that tail on the GPU. Every original
precise occlusion query is still issued and ended.

A final part is eligible only when the existing synchronization already retires
it before a future replay can observe the counter: a required full wait, or a
nonpipelined replay with CPU write-backs that already gate the next replay.
Other submissions, intermediate parts and synchronous execution retain GPU
resolution with the original query WAIT flag.

The existing wait-all fence must succeed. Before reading, the runtime validates
the slot, reset epoch and every queued part through its owner. It
reads the ended query range as completed 64-bit hardware values before resetting the pool and requires
success. Vulkan documents both the availability requirement and the stale-value
risk around query reuse in
[vkGetQueryPoolResults](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetQueryPoolResults.html).

The logical sample count is the GPU subtotal plus the completed CPU delta.
Before a sample report reads the subtotal, an eight-byte inline update feeds the
unchanged GPU sum kernel. Reset discards the delta and clears the subtotal.
Earlier reports remain snapshots at their original command positions, even when
more draws follow. The conditional predicate occupies its separate word at
offset 8. Future readers of the private subtotal must merge completed values
first. The inline source is copied during command recording, as specified for
[vkCmdUpdateBuffer](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdUpdateBuffer.html).

GPU-only fallback uses aligned device-local scratch in the existing counter
allocation, with reuse barriers. `NX_COUNTER_HOST_RESOLVE=0` selects the old
collection path; `NX_COUNTER_LOCAL_RESULTS=0` selects the old temporary storage.
The default enables both changes. Native clock conversion, resource identity,
query coverage and the original fence predicates and timeouts are retained.

## Authored workload

| Two 512-query submissions | GPU-only control | Retired-query collection |
| --- | ---: | ---: |
| Submission wall time, ms | 14.369 | 2.660 |
| Timestamped GPU part, ms | 12.527 | 0.918 |
| Host query read and accumulation, ms | - | 0.0119 |

These are medians from 32 same-binary runs in repeated ABBA order. Both arms
use the device-local temporary storage, isolating retired-query collection. The wall
reduction is 81.5%. The small offscreen workload includes diagnostic and logging
overhead; it does not establish gameplay FPS. Each process passes 1,549 controls and
issues and ends 1,469 actual hardware queries. With host collection enabled,
1,300 retire on the host and 169 retain GPU resolution.

Controls cover variable rectangle coverage and query counts, reset before
observation, earlier-report immutability, eight queued submissions without a
retirement guarantee, write-back gating, mixed GPU/CPU results across parts,
rejected partial ownership and repeated scratch reuse. Report values and
write-back bytes match the control. Counts are never inferred from draw totals.

## Gameplay measurements

| Course | Mode | Intervals | Mean, ms | Minimum, ms | Maximum, ms | p99, ms | FPS |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Mario Kart Stadium | Baseline | 10,938 | 17.302 | 7.585 | 64.521 | 24.779 | 57.80 |
| Mario Kart Stadium | R324 | 11,318 | 16.735 | 8.279 | 58.764 | 19.494 | 59.76 |
| Water Park | Baseline | 10,618 | 17.807 | 5.530 | 47.124 | 30.543 | 56.16 |
| Water Park | R324 | 9,454 | 16.698 | 11.703 | 22.669 | 18.775 | 59.89 |
| Sweet Sweet Canyon | Baseline | 15,116 | 18.780 | 7.617 | 118.231 | 29.583 | 53.25 |
| Sweet Sweet Canyon | R324 | 13,194 | 16.808 | 12.076 | 145.859 | 19.505 | 59.50 |
| Thwomp Ruins | Baseline | 12,478 | 17.642 | 8.657 | 36.333 | 22.679 | 56.68 |
| Thwomp Ruins | R324 | 13,310 | 16.678 | 6.725 | 69.267 | 18.542 | 59.96 |

Racing bounds are manually reviewed lap-1-through-lap-3 captures, with 500 ms
guards for capture and buffered-log boundaries. Loading, menus, results and a
repeat-course tail are excluded. Baseline and candidate use the same executable,
original save, initial pipeline inputs and separate copies of the same warm
driver cache, at 100cc and 60 Hz. Driving paths and desktop load vary, so the
comparison is observational rather than an identical-state replay.

The largest isolated interval worsens in Canyon and Thwomp Ruins even though
their means and p99 values improve. Hitches remain: this comparison does not
establish their individual causes or uninterrupted 60 FPS.

| Course | Replay mean, ms | Replay p99, ms | Sampled submits | Query-copy ms/sample | Reduction ms/sample |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mario Kart Stadium | 2.220 | 4.125 | 90 | 0.084 | 0.014 |
| Water Park | 2.336 | 4.202 | 71 | 0.119 | 0.017 |
| Sweet Sweet Canyon | 2.241 | 4.574 | 100 | 0.105 | 0.018 |
| Thwomp Ruins | 2.363 | 4.598 | 102 | 0.114 | 0.021 |

Replay rows join complete GPU submissions using presentation watermarks. Phase
means divide by sampled submissions. These are distinct denominators; neither
is a calibrated display-scanout time. The sampled phase observer is separate
from both plain controls.

The earlier uncapped four-course means of 9.2-10.5 ms are throughput stress
measurements. A Time Trials audit found that raising display cadence accelerated
the race clock. Those values cannot be credited as normal-speed gameplay FPS or
as either presentation milestone. See the [clock audit](native-frame-clock-audit.md).

## Verification

R324 passes 44 CTest suites and 3,253 native GPU checks, including stock, valid
and corrupt seed controls. The GPU floor includes 35 new offline-service checks.
Measurements use the local Windows system; other hardware and drivers are not
covered by this performance comparison. Counter synchronization and cutoff each pass 300 checks; texture readback passes
259. The authored phase probe passes 412 checks. The Python reporters pass 28
tests, including CRLF, incomplete data, capped streams and conservation checks.

Clang analysis of all 11 final changed translation units reports zero diagnostics.
The final source hashes match the frozen build inputs.

The counter-hook header change received a full runtime rebuild; later successors
reused matching objects and rebuilt changed units. Generated CPU inputs, three
CPU archives and the original save remain pinned. The verified generic recipe
contains 214 files. Game data, generated CPU code, shaders, catalogs, caches,
captures, raw logs and binaries remain private.

The remaining goal is true 125 FPS first, then 250 FPS, at normal simulation
speed. Rendering and simulation still need independent scheduling; this change
does not implement that frame-loop work or prove an 8 ms or 4 ms milestone.
