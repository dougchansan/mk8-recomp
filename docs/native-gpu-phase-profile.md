# Sampled asynchronous GPU phases

`NX_GPU_PROFILE=4` adds phase timestamps to selected replay submissions. It uses
the existing per-slot, per-part query pools and reads results after the existing
fence retirement. It adds no query-read waits or forced rendering splits. Mode 3
continues to provide coarse asynchronous GPU timing; mode 1 retains the older,
more intrusive diagnostic behavior.

`NX_GPU_PHASE_EVERY` selects the sampling interval, default 127 submissions.
`NX_GPU_PHASE_MARK_LIMIT` bounds marks per part, default 1,024. A final mark is
reserved so a capped tail is explicit. Query labels, timestamps and context IDs
belong to their slot and part until retirement. Timestamp width and wrap checks
use the device's reported limits.

Run the reporter on a closed case:

```text
python scripts/native_gpu_phase_report.py <case>
python scripts/native_gpu_phase_report.py <case> --events <first> <last>
```

Event indices are one-based, manually reviewed captures. The scoped form applies
500 ms guards and correlates selected submissions against the whole log, so
delayed retirement records remain available. Input normalization handles both LF
and CRLF logs.

Only complete sampled submissions whose phase totals match the coarse part
totals contribute to reported costs. The reporter rejects or exposes duplicate,
orphaned, unavailable, missing, capped and nonconserving data. Means divide by
qualified sampled submissions, never by every presentation.

Render-pass labels name the first fragment shader in the pass. Other draws can
use different shaders, so the label does not identify the sole cause of a pass's
cost. Queue timestamps can include dependencies and stalls. They are distinct
from renderer CPU time, presentation intervals and display scanout. CPU submit
watermarks correlate jobs; they do not prove ownership of a simulation frame.

The authored GPU probe executes 24 submissions and 48 parts, validating the same
GPU-produced checksum across coarse, sampled, sparse, mark-cap, minimum-cap and
stream-cap controls. All 412 checks pass. Reporter regressions cover incomplete
groups, exact coarse matching, conservation, caps, duplicates, missing data and
the bytes-to-CLI CRLF path.

The initial sampled racing profile identified the counter-resolution tail,
leading to the [completed-query optimization](native-counter-retirement-performance.md).
Absolute timings remain subject to observer cost and scene variation. Keep the
profile disabled in plain gameplay controls.
