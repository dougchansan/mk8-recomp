# Faster copied-texture registry lookups

## R344: reuse the texture type query in registered-owner paths

R344 extends the existing virtual texture type query to four owner lookups inside
`texture_identity_locked`. It replaces RTTI at the direct lookup, positive-cache,
live-pool and retained-snapshot lookup sites. Null checks, complete descriptor
comparison, registry generation, lock ownership, device checks and first-owner
selection remain unchanged. The mixed-registry fallback already uses this query.
Only the existing Texture hierarchy overrides it. There is no additional cache
or index, and no change to GPU commands, resource lifetime or simulation cadence.

A bounded normal-60 diagnostic observed 65 render-target binds per sampled draw
callback. The 43 retained render-target samples total 1.182 ms of inclusive wall
time. Sampling is sparse and includes diagnostics; these observations motivated
the lookup benchmark and do not establish a per-frame saving.

| Authored lookup fixture | R342 median ns/query | R344 median ns/query | Reduction |
| --- | ---: | ---: | ---: |
| Default shared positive cache | 52.8605 | 46.3645 | 12.29% |
| Miss-heavy live-pool scan | 236.3175 | 157.5390 | 33.34% |
| Unchanged mixed-registry fallback control | 52,872.50 | 50,543.15 | 4.41% |

Each row uses eight separate processes in repeated baseline/candidate/candidate/
baseline order, 24 processes total. Every process passes its existing exact
identity checks. The unchanged fallback's movement shows measurement and layout
variation; its result is not an attributed optimization. Run the native fixture's
`--texture-identity-bench` control with default settings and with
`NX_TEXTURE_IDENTITY_SHARED_CACHE=0`, and `--texture-registry-bench` with
`NX_TEXTURE_POOL_INDEX=0`, using the existing headless configuration. Device initialization is
outside the timed region. These fixtures test lookup cost, not game FPS.

Four reviewed plain-profile Stadium Time Trials windows cover 7,626 complete
presentation intervals, each spanning at least 31 host seconds. They use the same
stationary start-line view with Mario and standard equipment; engine class was
not verified. CPU time comes from the held bootstrap/draw thread's OS counters,
with in-process CPU/phase/SVC profiling disabled. CPU and native presentation
boundaries are approximate, and this thread's CPU time is not the full critical
path. Raw cycle counts were not converted into time.

| Actual run order | Thread CPU ms/present | Mean presentation ms | p99 ms | Minimum ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| R342 baseline A | 8.003 | 16.66605 | 18.6577 | 13.8556 | 19.9727 |
| R344 candidate A | 8.213 | 16.66686 | 18.4768 | 13.6339 | 19.0221 |
| R342 baseline B | 9.206 | 16.66687 | 19.3027 | 9.7270 | 21.7890 |
| R344 candidate B | 5.683 | 16.66634 | 18.4880 | 13.7762 | 19.8891 |

All interval outliers are retained. Each window averages approximately 60 FPS;
HUD time advances 0.99957–0.99998 seconds per host second. Between-run CPU
variation prevents a whole-game CPU or FPS gain claim. The first baseline has a
startup-only competing-process check. Candidate A and baseline B were stopped
when an unrelated compiler appeared later, respectively over 60 and 13 seconds
after their reviewed windows. Their later portions remain excluded and flagged.
The final candidate has continuous process checks and closes without compiler
interference. Separate baseline/candidate motion observations show course/kart
geometry and timer progress; this is not complete simulation equivalence.

Validation: all 52 CTest suites, 3,253 native GPU checks, 1,549 counter checks,
existing phase controls, 300 sync, 300 cutoff and 259 readback checks pass. Copied
target tests preserve full descriptor matching, mutation rejection/recovery,
retirement, owner order, rehash and address reuse, including exact GPU
clear/readback. All 92 runtime objects were freshly built. The changed C++ unit
has zero Clang analysis diagnostics. Three generated CPU archives and all 406
generated inputs remain pinned. The public recipe reconstructs 224 generic files.

The supported claim is faster authored lookup fixtures with existing correctness
controls preserved. This run contains no GPU execution timestamps and establishes
neither the 8 ms nor the 4 ms presentation milestone. Correct render/simulation
decoupling remains open. One excluded candidate launch stayed at the title despite
acknowledged input; a separate input-record diagnostic and the final candidate
advanced normally. The isolated input failure remains unexplained.

## Earlier R313 fallback measurements

A later [clock audit](native-frame-clock-audit.md) found that the experimental
360 Hz override accelerates the race clock. The uncapped presentation figures
below describe throughput; they do not establish normal-speed gameplay FPS.

The reviewed 100cc Stadium diagnostic found a costly fallback in copied
texture descriptor admission. It averaged 7,734 registry entries per fallback
lookup. Fully contained diagnostic batches recorded 2.517 ms of fallback wall
time per presentation interval, almost entirely on the main entry thread.
The presentation caller is a different thread. These overlapping timings include
clock and counter overhead and cannot be added to the presentation interval.

R313 replaces the fallback loop's per-object RTTI search with a virtual type
query. Ordinary objects return null; textures return their texture base. The
loop follows the same registry order and returns the first complete match.
Direct-key, positive-cache, live-pool and retained-snapshot paths are unchanged.
Copied descriptors still require a readable guest span, live device, current
object type and complete descriptor match. Positive-cache hits still require
the current global registry generation. Snapshot ownership, resource
incarnations, GPU work, fences and simulation clocks are unchanged.

| Isolated fallback fixture | Old implementation | R313 | Reduction |
| --- | ---: | ---: | ---: |
| Time per lookup, microseconds | 96.125 | 50.726 | 47.2% |

The fixture forces a generation change before each of 2,000 lookups. It selects
matching owners near the end of a mixed registry and requires at least 4,096
visited nodes; the measured runs traverse 8,193 nodes. The table gives medians
across 32 runs in repeated ABBA order. GPU device initialization is outside the
timed region. Both implementations pass the same 38 controls for first matching
owner, rehash, all 13 descriptor fields, stride, retirement, wrong type and owner
address reuse. Run `native_device_fixture --texture-registry-bench` with the
existing headless native fixture configuration to reproduce it.

A separate diagnostic run measured the changed path during racing. Both reviewed
windows span Stadium lap 1 to lap 2 at 100cc. Registry wall time per visited
candidate fell from 23.607 ns to 13.954 ns, a 40.9% observed reduction. Almost
all measured identity work remained on the main entry thread. The mean number
of candidates per fallback also changed from 7,734 to 6,888. Scene, path, cache
locality and scheduling vary, so this diagnostic does not isolate a total
frame-time gain. Timestamped GPU replay parts averaged 5.830 ms before and
5.191 ms afterward in these windows; the source change does not optimize GPU
commands. Queries were collected at existing fence retirement without adding
waits. No query result was unavailable.

Two plain-profile 100cc Mushroom runs used the same initial native pipeline
cache, optional 2,980-state catalog, separate exact warmed driver-cache clones,
360 Hz pacing setting and secondary-monitor shared-memory input. Reviewed HUD
bounds span racing lap 1 to lap 3. The first and last 500 ms are trimmed to
allow for the logger's 250 ms flush interval.

| Course | R308 mean ms | R313 mean ms | R308 p99 ms | R313 p99 ms |
| --- | ---: | ---: | ---: | ---: |
| Stadium | 13.787 | 12.837 | 23.329 | 21.458 |
| Water Park | 12.569 | 13.092 | 24.148 | 22.482 |
| Sweet Sweet Canyon | 14.439 | 14.947 | 25.471 | 26.122 |
| Thwomp Ruins | 15.108 | 13.519 | 23.645 | 21.762 |

These mixed results do not establish a causal whole-game FPS improvement.
A restarted background connector was stopped during baseline Stadium under
explicit authorization and recorded locally. The other reviewed baseline
course windows follow that intervention. Driving paths, items and GPU workload
also vary. Candidate racing-window maxima were 25.171, 25.674, 29.970 and
26.546 ms respectively. The controlled claim is the lookup benchmark, supported
by the separate live path measurements. Sustained 8 ms presentation and the
subsequent 4 ms target remain open; minimum intervals and GPU-only durations do
not establish either target.

Validation:

- All 92 runtime objects were freshly compiled because the internal host Object
  vtable changed. Cached runtime objects must be rebuilt after this header
  change. Only the Texture hierarchy should override the type query.
- All 44 CTest suites, 3,218 GPU checks with default, valid-seed, corrupt-seed and
  measurement-enabled settings, 300 sync, 300 cutoff and 259 readback checks pass.
  Every benchmark process passes the 38 new identity controls.
- Clang analysis reports no diagnostics in the changed production translation
  unit or test fixture. Three generated CPU archives, generated inputs and the
  original save remain pinned. The 214-file generic recipe reproduces R313.
- Both plain runs completed the four reviewed courses and returned through the
  cup/menu flow. The candidate also loaded Stadium again before the driver
  recognized closure; no additional Stadium racing window is claimed. The
  diagnostic stopped at its requested 180-second deadline. All three closed logs
  have zero fatal errors, shader misses, frame-ID gaps or truncation. GPU queries
  are intentionally absent from the two plain controls.

Private game data, captures, shaders, catalogs, caches, logs and binaries are
excluded from the public recipe.
