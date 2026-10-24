# Retained texture descriptor admission (R371)

R371 reduces copied-descriptor lookup work on the main thread. A separate
count-only diagnostic found about 17,075 retained-snapshot candidates per
stationary presentation and 18,074 during acceleration. Those counts identify
work to optimize; they are not clean frame-time measurements.

The admission index now includes the immutable copied descriptor fields:
pool, GPU address, offset, stride and the thirteen existing identity fields.
Lookup still holds the registry lock and revalidates current object type,
ownership, device and the complete descriptor. Multiple matching retained
snapshots still fall through to the original registry-order selection. Registry
generation, resource incarnations, image inheritance, draws, counters and fences
are preserved. Mutable view, pool-swizzle and image state are outside the key.
Future descriptor mutation must update the index under the same lock.

`NX_TEXTURE_SNAPSHOT_DESCRIPTOR_INDEX=0` retains the diagnostic pool/GPU grouping
inside the new tuple container. It is not a literal old-container baseline.
The comparisons below instead use R368, whose production runtime sources match
the published R366 implementation, against R371.

## Authored workload

Sixteen separate native processes run in repeated baseline/candidate/candidate/
baseline order. Each performs 65,536 checked lookups and 4,096 slot replacements,
with the same result checksum and lifetime assertions. Device initialization is
outside the timed work.

| Median measured phase | R371 change from baseline |
| --- | ---: |
| Lookup elapsed time | -95.22% |
| Lookup raw thread cycles | -95.20% |
| Slot replacement elapsed time | +15.66% |
| Slot replacement raw thread cycles | +14.38% |

All four blocks improve lookup cost and regress replacement cost. The more
selective index trades registration work for fewer lookup candidates. Short-phase
OS CPU accounting is too coarse for a meaningful percentage; raw cycle counts
are retained without converting them into elapsed time. This fixture alone
establishes no gameplay FPS improvement. Use the native fixture's existing
headless setup with `--texture-snapshot-workload` to exercise the workload.

## Closed gameplay comparison

Four processes run in baseline/R371/R371/baseline order, using matching private
save, shader, pipeline and driver-cache seeds at normal 60 Hz pacing. Each
captures prescribed stationary and held-accelerator windows on Mario Kart
Stadium. Stored screenshots occur only at the boundaries; no CPU function,
call-phase, context-sampling or candidate-count profiling is enabled. GPU
timestamps and read-only OS thread/process counters remain measurement overhead.
The continuous competing-build/game guard records no interruption.

All sixteen boundary images retain course, kart, HUD and lap 1/3. Acceleration
reaches wall contact; this is not a complete lap or physics-equivalence test.
The four stationary windows pass the original 0.99-1.01 HUD/host clock gate,
at rates 1.00040, 1.00727, 0.99939 and 0.99097 respectively. The table retains
all 10,344 complete intervals inside their observation-free timing boundaries.

| Stationary run order | Main CPU ms/present | Render CPU ms/present | Mean interval ms | p99 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Baseline A | 7.373 | 5.963 | 16.66684 | 20.4569 | 22.4413 |
| R371 A | 6.817 | 4.867 | 16.66642 | 20.5003 | 22.2089 |
| R371 B | 3.678 | 4.340 | 16.66634 | 19.8175 | 21.5698 |
| Baseline B | 5.396 | 4.893 | 16.81399 | 21.1183 | 134.3321 |

Equal-weight process-window means show main-thread CPU work falling from
6.384 to 5.247 ms/present (-17.81%), with raw main-thread cycles down 15.33%.
Both adjacent pairs improve: CPU -7.54%/-31.84%, cycles -7.78%/-25.63%.
Render-worker CPU falls from 5.428 to 4.603 ms/present (-15.20%); its raw cycles
fall 8.40%. Whole-process CPU accounting falls 17.56% in these stationary
windows. These thread times overlap and cannot be added into a frame budget.

Variation between processes is substantial. The result supports a scoped
admission-work improvement, with medium confidence in its gameplay magnitude.
It does not establish a universal CPU, energy or FPS improvement. The baseline's
134.3321 ms hitch is retained and remains unattributed; this change is not
claimed to fix that hitch. Candidate stationary mean intervals stay near
16.6664 ms, approximately 60 FPS. GPU replay averages 1.026 ms baseline and
1.027 ms candidate; presentation GPU work averages 0.148/0.150 ms. GPU work is
effectively unchanged. GPU timestamps include execution stalls and do not measure
monitor scanout or the entire host critical path.

| Accepted accelerating window | Clock rate | Main CPU ms/present | Mean interval ms | p99 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Baseline A | 1.00110 | 6.914 | 16.68856 | 20.5893 | 55.1106 |
| R371 A | 0.99980 | 3.437 | 16.66461 | 19.3071 | 21.0951 |
| R371 B | 0.99905 | 5.399 | 16.66744 | 20.0457 | 22.4211 |

The closing baseline's accelerating window advances at 0.98042x and is excluded
by the unchanged clock gate. These three accepted windows are descriptive,
not a complete balanced accelerating comparison. An earlier partial campaign
also retains a rejected 0.98393x stationary baseline and a launcher sharing
failure; neither is silently substituted into the balanced result.

Capture protocol 2 pairs status elapsed time with its own clock anchor and
brackets the complete capture/write transaction with CPU observations. Timing
selection excludes those observation envelopes plus the existing boundary guard.
Delayed-I/O controls cover the previous double-counted status time. The private
launcher also uses delete-sharing reads, writes the unchanged case identity once,
and requests owned-driver shutdown if a status replacement fails. A real-child
failure control verifies closure. These tooling changes do not alter game code,
the render resolution or the timing acceptance thresholds.

## Correctness and portability

R371 also fixes a pending-cancellation race in the native timed synchronization
wait. A host timeout could be returned after cancellation had arrived while the
kernel lock was held. The timeout condition now rechecks pending cancellation;
the existing deadline and ready-handle priority remain intact. A real-SVC
regression holds the lock across the deadline and covers timeout, cancellation,
readiness and readiness with a cancellation retained for the next blocking call.
The old implementation fails the cancellation case; R371 passes all fifteen
checks. Run `native_device_fixture --wait-timeout-cancel-race` to exercise it.

All 57 native Windows suites pass, an exact superset of the previous 56. Existing
3,253 GPU, 1,549 counter, 300 sync, 300 cutoff, 259 readback and six phase controls
pass, along with three snapshot-admission and five property-cache/index modes.
All 92 runtime objects were freshly built; the three changed C++ analysis units
have zero diagnostics. The 406 generated CPU inputs and three CPU archives stay
pinned, with no new DLL imports.

Seven bounded authored controls also pass on the network RTX 3090 under Wine 9
using the already-required current Windows C++ runtime DLLs app-locally. They
cover CPU, both GPU index modes, sync, cutoff, readback and phase work with the
validation layer active and no new Xid/validation errors. Earlier built-in Wine
C++ wait/prewarm failures remain recorded. This qualifies that configuration for
those controls; native Linux gameplay and network-machine FPS remain unverified.

The public recipe reconstructs 235 generic source files with the same generated
Vulkan dispatch, including from an output directory containing spaces. The
upstream MIT license keeps its words and attribution; canonical LF bytes and
the matching hash now survive both Windows checkout and Git archive export.
Two regression tests protect license/patch byte identities and checkout rules.
Public source-only and portable-path audits remain required before publication.

The publication CI also exposed six stale decode-coverage expectations: the
pinned strict-static emitter already translates fixed-point conversions that
the coverage fixture still expected to fall back. The fixture retains all 240
instruction cases and now checks the six emitted helper bodies, including widths,
scale, signedness, registers and FPCR/FPSR arguments. The old fixture reproduces
234/240; the corrected fixture passes 240/240 against the same emitter header.
This changes the authored test expectations and leaves emitted game code pinned.
The side-entry runner also explicitly returns success after validating its
expected guard fast-fail. Previously both controls passed but the CI shell
propagated the last native abort status. The exact rejection checks are retained.

True normal-speed 125/250 FPS and sustained 8/4 ms presentation remain open.
The next scheduling work still needs independently owned render preparation and
simulation state; a faster display event or repeated task/present is insufficient.
