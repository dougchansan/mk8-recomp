# Remaining R353 runtime costs

The current verified runtime remains R353. A subsequent diagnostic run measures
GPU execution and CPU work after the scalar-property cache change. Two proposed
CPU optimizations fail matched controls and remain unpublished. Sustained 8/4 ms
presentation and normal-speed 125/250 FPS remain unverified.

## Racing scope and GPU measurements

The closed run uses the same R353 executable and frozen input/cache seeds as the
[scalar-cache comparison](native-texture-property-cache.md), with additional
CPU-region timers, indirect-call sampling and GPU phase timestamps. GPU phase
markers sample every 127 submissions. Profiling and periodic images add overhead;
this run does not replace the unprofiled performance comparison.

The reviewed Stadium Time Trials windows cover about 50 seconds at the start
line and 32 seconds with the accelerator held. The latter includes driving
through the underpass into a wall. Course, kart and HUD remain visible. The HUD
advances at approximately 1.0007 and 1.0001 times host time. These observations
do not establish full-lap, all-course, physics or audio equivalence.

| Racing window | Complete correlated presentations | Mean timestamped GPU work ms/presentation | GPU p99 ms | Mean presentation spacing ms |
| --- | ---: | ---: | ---: | ---: |
| Stationary start line | 2,933 | 1.004 | 1.160 | 16.666 |
| Held accelerator | 1,887 | 0.847 | 1.079 | 16.666 |

The analysis retains delayed GPU retirements from the complete closed log while
selecting submission IDs inside the racing windows. Half-second boundary guards
exclude adjacent presentation intervals. All correlated presentations in the
table have complete coarse GPU parts.

The whole run contains 174 complete sampled submissions, with no unavailable,
capped or invalid parts, missing sample-part sets, orphan segments or analysis
errors. That whole-run count includes menus/loading. Racing-only phase samples
number 23 and 15 respectively:

| GPU phase | Start line ms/sampled submit | Held accelerator ms/sampled submit |
| --- | ---: | ---: |
| Render passes | 0.719 | 0.602 |
| Outside render/compute regions | 0.239 | 0.200 |
| Compute | 0.039 | 0.040 |

Phase means divide by sampled submissions, not all presentations. Queue
timestamps include dependencies and stalls inside their measured regions.
Render-pass labels identify the first program, not every draw. Submission
watermarks do not establish simulation-frame ownership. These values are GPU
work measurements, not display scanout, a complete critical path or evidence
that the 8 ms presentation goal has been reached.

## CPU scopes and observer cost

The following are elapsed exclusive scopes across their calling threads, with
conservative guards around the reviewed windows:

| Scope | Start line ms/frame | Held accelerator ms/frame |
| --- | ---: | ---: |
| Draw | 2.514 | 1.297 |
| Texture identity | 1.931 | 2.174 |
| Upload deduplication | 1.186 | 0.613 |

Scopes on different threads overlap. Scope endings are assigned to reporting
buckets, so asynchronous boundary ambiguity remains. Do not add these values
to waits, GPU durations or inclusive parent scopes to manufacture a critical
path. The indirect-call sampler also reports inclusive cycles and only the top
eight targets per reporting batch; nested samples and missing tails prevent an
on-CPU percentage interpretation.

The weighted one-in-1,024 focus scope reports 4.750 and 3.907 ms/frame. That is
not a demonstrated amount of removable CPU work. In a separate 12-process
unprofiled/profiled/profiled/unprofiled control of the unchanged R353 focus fixture,
median gate cost is 12.99675 ns without profiling and 16.3265 ns with profiling,
an observed 25.62% increase. All 43 focus checks pass in every process. This
isolated fixture does not calibrate the weighted sample duration in the game;
the sampled timer/prologue and workload differences still limit interpretation.

## Rejected optimization candidates

R354 caches the current PID while retaining live window-owner, generation and
visibility checks. Twenty-four native processes in repeated
baseline/candidate/candidate/baseline order pass all 43 existing focus checks
without activating the fixture window. Median gate cost increases from
13.373 to 15.99875 ns. Four of six blocks regress and two improve. The candidate
does not demonstrate a reliable benefit and is not promoted or gameplay-tested.

R355 adds a negative host-object type hint to avoid virtual type queries on
known non-textures during mixed-registry scans. Positive hints still use the
virtual query, complete descriptor checks and legacy owner order. The hint
changes host object layout. A fresh 92-object runtime build, all 54 native
component suites and twelve 245-check property processes pass. Added cases
exercise derived textures and a sliced base with a positive hint but a null
virtual texture query.

The matched 20-process old/new comparison rejects that candidate too. The forced
scan uses 4,096 queries, 8,193 initially visited entries and the same checksum.
Its median cost increases from 28,862.65 to 32,120.45 ns/query, or 11.29%; all
three four-process blocks regress. The hot-cache movement is about 1% and is
not separately attributed. Layout and code changes are part of this comparison;
it does not isolate their individual effects. The candidate is stopped before
GPU/gameplay qualification and is not part of the public source recipe.

## Next measurements

Measure upload-deduplication front-cache hits, map lookups and new copies before
selecting an allocator or cache change. Retain complete reset/invalidation and
upload behavior in any control. CPU time for held individual worker-thread
lifetimes would also help distinguish main-thread work from replay work and
waiting. Optimizing a weighted elapsed scope alone is insufficient.

Independent rendering still requires proven simulation-state and render-state
ownership. The rejected broader update gate lost geometry despite a normal HUD
clock; its [clock](native-frame-clock-audit.md) and
[render-state](native-render-state-lineage.md) limits remain applicable. No wait,
draw, presentation or simulation cadence is changed by these measurements.
