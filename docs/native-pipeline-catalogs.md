# Course-entry pipeline stalls and read-only catalogs (R299)

Sweet Sweet Canyon and Thwomp Ruins had multi-second first-use graphics
pipeline creation bursts on the render thread while the game thread waited.
Historical detailed samples measured 2,806 ms of creation inside a 3,306 ms
presentation interval and 2,439 ms inside a 2,889 ms interval. Upload byte
counters overlap this work; they are not additional measured CPU duration.

Recipes captured in another data root were unavailable to the active run.
R299 adds optional `NX_PIPELINE_RECIPE_SEED` import, so a prior local capture
can prewarm known pipelines without sharing a mutable cache directory.

```text
NX_PIPELINE_RECIPES=1
NX_PIPELINE_RECIPE_SEED=/absolute/path/to/pipeline_recipes_v1.bin
```

The active root's entries retain priority, followed by the imported catalog.
The existing merge routine removes duplicates and retains the 8,192-entry cap;
run counts are preserved during import. The existing schema, payload checksum
and 64 MiB size checks apply. New observations are saved to the active root;
the imported file remains unchanged. An unreadable or invalid seed is logged
and the active root continues to work.

Only `nvn_pipeline_prewarm.cpp` changes relative to R284. Exact raw-state
matching, shader-content validation, four descriptor-set-layout checks and
existing waits remain intact. The same bounded background workers build fully
optimized pipelines. Generated CPU code, rendering, GPU commands, fences,
counters, simulation and dependencies are unchanged. No catalog, shader or
compiled driver data is distributed here.

## Gameplay verification

Both controls and the candidate start with the original 1,749-entry per-run
catalog and 14,839,604-byte application driver cache. The new treatment
deliberately adds a read-only target catalog, yielding 2,887 merged entries.
This is a coverage and scheduling improvement, not faster compilation per
pipeline. Mario/100cc, route, saved profile, display/input settings and plain
diagnostics match the controls; implicit caches use fresh owned directories.

| Course | R284 peak interval, two controls | R299 peak interval |
|---|---:|---:|
| Sweet Sweet Canyon | 3,028–3,444 ms | **417 ms** |
| Thwomp Ruins | 2,710–3,252 ms | **319 ms** |

These are 86% and 88% lower than the respective best baseline peaks. Sweet
Sweet Canyon creates no pipelines on the draw path in the candidate section.
Thwomp's former entry burst disappears; 11 later first-use pipelines total
291 ms, with a largest 60-presentation creation window of 271 ms.

Coverage matters more than catalog size: a 6,156-entry broad capture lacked
400 entries present in a verified four-course capture. Consolidating relevant
local captures produced the 2,887-entry target catalog, which covered 421 of
425 states missing from the broad catalog in the observed coverage audit.
Unknown states still use the normal compiler; first-ever uncaptured work is
not eliminated by this feature.

The 1,201-second candidate run reaches Thwomp lap 3/3 and is intentionally
stopped at its deadline, with no shader misses or logged fatal errors. It is not
a completed-cup claim. The same R299 executable also completed the cup in the
preceding broad-catalog control. Course-load sections include loading, intros
and results, and boundary windows can span sections. Pipeline durations
overlap delivery and must not be added to presentation intervals.

## Current racing readings

These are uncapped presentation-call measurements between visually checked
racing HUD captures, excluding the first capture-adjacent reporting bucket.
They are not GPU render or scanout times, and the minimum is an individual
interval rather than the reciprocal of an FPS window.

| Course | Observed racing average FPS | Minimum interval | Presentations sampled |
|---|---:|---:|---:|
| Mario Kart Stadium | 51.5 | 6.006 ms | 9,780 |
| Water Park | 58.8 | 6.555 ms | 9,300 |
| Sweet Sweet Canyon | 53.8 | 6.833 ms | 10,200 |
| Thwomp Ruins | 45.2 | 6.776 ms | 11,460 |

These bounded windows span lap 1 to lap 3; they are not entire-race averages.
No steady-FPS gain is established. Sustained smooth 60 FPS and correct 120 FPS
remain open. Earlier coverage readings for other courses are in
[main-thread measurements](native-main-thread-focus.md); their legacy logs
do not retain individual interval minima.

## Cost and correctness

Target prewarm takes 17.846 s, versus 2.55–3.69 s for the root-only controls.
It uses 35.601 s of overlapping worker CPU time and reports 4,329 MB private
memory afterward. All 2,887 recipes build with zero failures, rejections or
draw claims. This work finishes before racing, under the unchanged 8 GiB guard.
The tradeoff is additional early CPU work and cached pipeline memory.

All 44 component suites pass. All original 3,212 native GPU checks pass for
default startup, valid-catalog startup and corrupt-catalog fallback. The
300-check sync/cutoff and 259-check readback controls pass. Changed-source
Clang analysis is clean; all 363 frozen-build source hashes match. Three CPU
archives, 406 generated input pins and the source save remain unchanged. The imported catalog's hash
is unchanged after probes and gameplay.

Graphics libraries, compiler-optimization-disable flags and effective-state
aliases did not reliably improve both target hitches. Their code and
feature-specific tests remain in private prototypes; none is included in
this change. The shipped source retains the stock R284 renderer and admission.
