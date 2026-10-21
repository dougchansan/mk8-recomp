# Faster copied-texture registry lookups

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
