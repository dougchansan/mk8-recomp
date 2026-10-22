# Scalar texture properties without repeated registry scans

R353 reduces repeated registry scans for scalar texture queries. Native component
and GPU controls pass, and both enabled runs in a same-binary
enabled/disabled/enabled Stadium comparison use less held main-thread CPU.
Presentation remains near 16.67 ms. Neither normal-speed 125/250 FPS nor the
8/4 ms presentation milestone is established by this work.

## Selection rules

The copied-descriptor positive cache normally expires on every exclusive object
registry lock. A subsequent scalar getter can therefore scan the complete
registry even when a previously matching owner remains live.

For getters whose result is part of the complete descriptor comparison, R353
may reuse that positive owner across registry generations. It resolves the
current registry entry under the existing shared lock and repeats the existing
type, device, readable-range and complete descriptor checks. Every complete
match has the same requested scalar value. No pointers or misses are cached.

This applies to dimensions, format, target, levels, samples, flags, stride,
memory-pool key, memory offset and depth/stencil mode. Storage size is absent
from the descriptor comparison, so its getter retains strict generation and
legacy first-match selection. Resource registration and binding retain those
same strict rules.

A scalar hit must **not update the cached generation**. Doing so could authorize
an old preferred owner for a later binding after a registry mutation. The new
regression creates equal descriptors with different storage sizes, forces a
different legacy winner using existing registry traversal order, and checks
storage queries and actual registration against that current winner.

`NX_TEXTURE_PROPERTY_CACHE=0` retains the previous generation requirement for
scalar getters. The default enables the optimization. Neither setting changes
GPU commands, waits, counters, resource lifetime or simulation cadence.

## Native controls

Twelve separate processes alternate the setting in repeated
legacy/candidate/candidate/legacy order. Each uses the actual compiled getter,
4,096 queries, 8,193 visited entries on the initial scan and a registry-generation
change before every timed query. Device initialization is outside the timer.

| Forced-churn property query | Median ns/query |
| --- | ---: |
| Legacy generation requirement | 45,806.5 |
| Revalidated scalar hint | 177.319 |

This fixture observes a 99.61% query-cost reduction. It deliberately exercises
registry churn and a long fallback scan; it is not a whole-frame or FPS result.
All 241 checks per process pass, including every compared field mutation,
retirement, wrong-type reuse, device finalization and strict storage/binding
selection after the preferred owner changes.

A separate 12-process no-churn control compares the actual previous R351 and
candidate binaries. Median cost is 94.156 versus 85.7085 ns/query, with variation
between blocks. This shows no observed aggregate regression in that control;
the smaller movement is not separately attributed to this optimization.

All 54 native Windows component suites and 19 existing GPU/control cases pass:
3,253 healthy GPU checks, the intended arena failures, 300 sync, 300 cutoff,
259 readback and existing counter/timestamp controls. Both changed-source Clang
analyses report zero diagnostics. All 92 runtime objects are freshly built;
406 generated CPU inputs and three CPU archives remain unchanged. The generic
source recipe contains 229 files.

The initial R352 fixture correctly rejected an unexercised ambiguity scenario:
inserting new nodes never changed its preferred owner. R353 strengthens that
setup using existing node order; production C++ is unchanged from R352.

## Reviewed gameplay comparison

The motivation is a closed normal-60 CPU investigation. Repeated process
context samples locate the full texture-identity registry scan most often among
resolved main-image samples. These samples include blocked contexts and capture
overhead; they are not an on-CPU ETW percentage. A separate racing window outside
those context samples observes 8.15 ms of held main-thread CPU per presentation
while presentation-call intervals remain about 16.67 ms. That window still
includes read-only controller observations and periodic screen captures.

Three subsequent runs use the same R353 executable, frozen input/cache seeds,
normal-60 settings, read-only observer and input schedule. The setting order is
enabled, disabled, enabled. Each run closes after about 421 seconds without a
competing workload. Single Player, Time Trials, Mario, standard equipment,
Stadium and the solo route are visually reviewed. Engine class is not verified.

The stationary start-line windows span about 50 seconds each. The held-accelerator
windows span about 33 seconds and include driving through the underpass into a
wall; they do not establish full-lap or all-course performance. All six reviewed
HUD clocks run at 0.9996–1.0003 times host time. Each window contains at least
1,944 complete, contiguous presentation intervals and no process-context capture.

| Window | Scalar cache | Main-thread CPU ms/presentation | Mean presentation interval ms | p99 interval ms |
| --- | --- | ---: | ---: | ---: |
| Start line | Enabled, first | 7.789 | 16.667 | 20.475 |
| Start line | Disabled | 11.284 | 16.667 | 20.474 |
| Start line | Enabled, repeat | 9.782 | 16.667 | 20.055 |
| Held accelerator | Enabled, first | 7.828 | 16.665 | 19.847 |
| Held accelerator | Disabled | 11.006 | 16.875 | 22.282 |
| Held accelerator | Enabled, repeat | 9.010 | 16.665 | 19.981 |

Both enabled runs use 13–31% less held main-thread CPU at the start line and
18–29% less on the held-accelerator route than the disabled control. Raw thread
cycles per presentation also fall in both enabled runs, without conversion to
frequency. There is substantial variation between the enabled runs; this small
bracketed sample does not establish a constant whole-game percentage or a
population confidence interval. The largest individual interval does not improve
uniformly: held-accelerator maxima are 31.104 and 29.443 ms enabled versus
29.221 ms disabled. These results do not establish a GPU execution-time gain,
physical scanout latency or removal of all hitches.

The final metadata launcher encounters a Windows sharing error. Its original
failure is retained; the already-running game, input replay, observer and
interference guard continue without restart. Held process handles verify their
terminal states and the intentional game closure. A bounded metadata replacement
retry subsequently passes a real Windows reader-sharing regression; persistent
and unrelated permission errors still propagate. The runtime source is unchanged
by that private harness repair.

Earlier wrong-menu and competing-build attempts remain excluded. An older R351
held-accelerator window also fails the normal-clock gate and is excluded from
that comparison. No failure is silently replaced with a passing result. No fatal
or shader miss is observed in the three accepted R353 game logs.

This is a scoped CPU improvement at the existing cadence. Rendering/simulation
decoupling and sustained 8/4 ms presentation remain open. Separate remote NVIDIA
device-creation faults remain unresolved; these Windows measurements do not
qualify that machine or establish native Linux gameplay.
