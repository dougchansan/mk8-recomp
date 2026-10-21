# Stable focus-policy reads

A later [clock audit](native-frame-clock-audit.md) found that the experimental
360 Hz override accelerates the race clock. The uncapped presentation figures
below describe throughput; they do not establish normal-speed gameplay FPS.

The focus gate is called about 98,000 times per presentation frame in the
reviewed R304 Stadium CPU profile, aggregated across threads. Its sampled,
weighted timing includes observer overhead and is not an additive frame budget.
R308 targets the shared read lock taken for stable policy values.

A thread keeps a value-only policy view. Writers mark an atomic revision odd
before changing view fields under the existing exclusive lock and publish an
even revision afterward. A reader refreshes its view under the read lock when
the revision changes. Every enabled call still queries the current window owner,
process identity, window generation, visibility and iconic state. It rechecks
the revision before using the policy view. Every rejected/stale view goes through
the existing exclusive path, which revalidates the current owner before failing
or parking. Notifications still run outside the lock. Liveness and visibility
results are never cached.

| Unprofiled focus-gate fixture | R304 production policy | R308 | Reduction |
| --- | ---: | ---: | ---: |
| Single caller, ns/call | 13.231 | 10.333 | 21.9% |
| 16-thread aggregate cost, ns/call | 87.820 | 14.105 | 83.9% |

The baseline executable uses the new tests with unchanged R304 policy code.
These are medians from 32 alternating real-window fixture runs. The parallel
figure measures aggregate throughput, not individual call latency. Every run
also passes the policy controls and uses an owned, non-activating window.

Two plain-profile 100cc Mushroom runs used the same initial native pipeline
cache, optional 2,980-state catalog, separate exact warmed driver-cache clones,
360 Hz pacing setting and secondary-monitor shared-memory input. Reviewed HUD
bounds span racing lap 1 to lap 3. The first and last 500 ms are trimmed to
allow for the logger's 250 ms flush interval.

| Course | R304 mean ms | R308 mean ms | R304 p99 ms | R308 p99 ms |
| --- | ---: | ---: | ---: | ---: |
| Stadium | 13.171 | 12.943 | 24.383 | 23.529 |
| Water Park | 12.992 | 12.998 | 22.211 | 20.652 |
| Sweet Sweet Canyon | 13.378 | 14.997 | 25.694 | 27.243 |
| Thwomp Ruins | 15.186 | 12.588 | 25.166 | 25.579 |

These mixed results do not establish a causal whole-game FPS improvement.
Paths, items, scene workload and host load vary. A background connector restart
was stopped during the baseline and recorded locally. Canyon's replay and fence
wait costs also increased in the candidate; neither is an isolated measure of
focus-gate cost. The claimed gain is the bounded gate-operation benchmark. The
sustained 8 ms presentation milestone and subsequent 4 ms target remain open.
No fastest interval or GPU-only duration substitutes for those targets.

Validation:

- 42 focus controls cover hidden-window parking, policy changes observed by a
  persistent reader, notifications, reentrant callbacks, window destruction,
  rebinding, and eight readers during 32 owner rebinds with old-window destruction.
  The unchanged baseline passes the same new tests. Profiling-enabled controls
  also pass.
- All 44 CTest suites, 3,218 GPU checks in default/valid-seed/corrupt-seed and
  instrumented modes, 300 sync, 300 cutoff and 259 readback checks pass.
- The changed production source passes Clang analysis without diagnostics.
  Source, three generated CPU archives, generated inputs and the original save
  remain pinned. The 214-file generic recipe reproduces the frozen candidate.
- Both gameplay runs reached all four courses and returned through the cup/menu
  flow. Closed runs recorded zero fatal errors, shader misses, frame-ID gaps or
  truncation. GPU timestamps were intentionally disabled for these plain controls.

Only the policy implementation and its test differ from R304. Generated CPU,
rendering, simulation, GPU fences/counters, descriptor/resource incarnation and
shader code are unchanged. Future writers must publish every cached policy-field
change under the same lock and revision protocol. Do not cache live/visible
booleans or reject an invalid cached owner before the locked current-owner check.

The separate CPU profile also observed texture-identity lookup work. Its cache
conservatively invalidates on global registry changes and can scan a pool on a
miss. That is a measurement target, not permission to weaken generation or
complete-descriptor validation. Private captures, assets, shaders, catalogs,
cache files, logs, screenshots and binaries are not distributed here.
