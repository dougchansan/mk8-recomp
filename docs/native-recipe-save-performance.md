# Recipe-save preparation on the frame path

R304 keeps the recipe engine's filtering and history merge off the presentation
caller. Previously `save_async` merged the entire captured recipe history while
holding the engine lock, then passed the completed recipe list to the saver
thread for serialization and atomic replacement. In the R302 candidate run, the
caller-side preparation averaged 22.156 ms across 68 saves (maximum 25.432 ms).

The caller now copies owned used/older/rejected records under that same lock,
with the same path, file information, dirty counter and saving admission. The
existing saver thread filters the copied rejected keys, merges the history,
serializes and performs the same atomic replacement. It never reads live engine
containers after releasing the lock. The synchronous save path remains as
before. No GPU work or GPU handle is moved to this worker.

| Caller preparation | Samples | Mean ms | p95 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: |
| R302, 2,980-state catalog | 68 | 22.156 | 23.983 | 25.432 |
| R304, before host-load intervention | 44 | 3.359 | 5.010 | 5.822 |

That is an observed 18.797 ms reduction per save preparation, or 84.8%. The
filtering/merge work still executes on the saver thread. This reduces a periodic
caller pause; it does not remove that much time from every frame or establish a
whole-game FPS gain.

Both runs used the same initial native pipeline cache and 2,980-entry optional
private catalog, 100cc and the same diagnostic profile. Background CPU load was
removed partway through R304, so the table uses only preparation events ending
before that intervention, with a two-second timing guard. The 76 events across
the whole R304 run average 3.585 ms. Later frame-rate readings are not a clean
A/B comparison with R302. Input histories and game paths also vary between runs.

This change preserves recipe value keys, rejected-stage decisions, first-use
order, run-count carry/saturation, the 8,192-entry and 64 MiB limits, schema and
atomic-write behavior. Rendering, generated CPU code, resource incarnation,
GPU fences, counters and simulation code are unchanged. The copied inputs add
some transient owned data; the filtering/merge cost and allocations still exist
on the saver thread.

Validation:

- Six new authored regressions check captured rejection keys, independence from
  later source-container mutations, move ownership, exact serialized bytes and
  zero/one-entry bounds. The recipe CPU suite passes 617 checks.
- All 44 CTest suites pass. GPU checks increase from 3,212 to 3,218 with those
  six checks included. Default, valid/corrupt seed and instrumented controls
  pass; 300 sync, 300 cutoff and 259 readback checks pass.
- Changed production source passes Clang analysis without diagnostics. Source,
  the three generated CPU archives, generated inputs and the original save
  remain pinned. The 214-file generic recipe reproduces the frozen build.
- A 1,501.8-second gameplay run reached all four Mushroom courses, with racing
  HUD captures reviewed. It ended at the intentional deadline during Thwomp
  lap 3; this is not a completed-cup claim. Fatal errors, shader misses, frame-ID
  gaps, record truncation and unavailable GPU query parts are all zero.
- The actual saved 3,023-entry gameplay catalog passes checksum validation and
  the native importer. Another 3,218-check GPU control passes with that file as
  its read-only seed; the input bytes remain unchanged.

Catalog coverage is separate from this source optimization. The preceding R302
comparison used 2,887 versus 2,980 exact private states on the same executable.
The expanded-catalog run had no draw-path creations in the four reviewed racing
windows. The old control had nine/254.945 ms in Thwomp and zero in Water Park and
Canyon. Different game paths prevent a causal FPS claim; the earlier Water Park
and Canyon creation hitches did not reproduce in that control. Captures still
produce additional states. No private catalog, shader, driver cache, game asset,
trace, screenshot or binary is distributed here.

The sustained 8 ms presentation goal remains open. Next measurements should
profile steady CPU replay/recording with host load controlled. The remaining
synchronous driver-cache extraction and disk write are separately measured
costs; this change does not alter their Vulkan synchronization.
