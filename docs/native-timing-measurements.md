# Native timing measurements

R300 adds opt-in frame and event records to the unchanged R299 rendering and
simulation paths. `NX_MEASURE_FRAMES=1` records individual presentation-call
intervals, caller CPU time and submission watermarks. `NX_MEASURE_EVENTS=1`
adds timed replay/present jobs, per-submit replay/encoding/fence/upload counters,
asset reads, pool initialization, image allocation, pipeline creation, ASTC
conversion and texture uploads. Allocation coverage is those specific runtime
sites, not every application or driver allocation. Upload scopes include any
conversion they perform; nested events overlap and must not be summed.

`NX_GPU_PROFILE=3` uses existing asynchronous Vulkan timestamps. Results are
read after the slot's existing fence retirement, with no added query wait.
R300 retains submit/present IDs in the query slot until retirement. GPU execution
is the sum of timestamped command-part spans, including stalls inside them; it
is not occupancy. Slot span includes gaps. Do not use mode 1 for representative
performance: it waits on submissions and changes GPU load/clock behavior.

The private course driver provides these separate profiles:

| Profile | Records | Purpose |
| --- | --- | --- |
| plain | Individual intervals and aggregate CPU/render counters | Comparison control |
| gpu | Plain plus GPU timestamps and IDs | GPU execution under normal asynchronous load |
| hitch | GPU plus detailed work events and slow-frame attribution | Investigate identified hitches; heavier observer overhead |
| cpu / focus | Existing call/phase profilers | Separate targeted CPU diagnosis |

Keep revision, course, engine class, immutable recipe seed, driver-cache condition,
display rate and input policy matched. Repeat plain/GPU racing sections; compare
observer overhead before claiming an improvement. GPU clocks and power are not
currently collected on Windows; do not interpret the legacy Android sysfs zero
as a measured Windows clock. Full ETW scheduling/driver allocation coverage is
not provided by this tooling.

The driver records executable/recipe/catalog hashes, build receipt, host, requested
pacing settings and cache-condition label. Capture events include original window
image dimensions, process working set/commit/page faults, log byte offsets and
wall time. The logged Vulkan adapter/startup settings complement the manifest.
A cache label of `unspecified` must be resolved before a controlled comparison.
HUD captures must confirm engine class and lap progression; FPS does not prove
simulation speed. Native simulation, fences, draws, resource incarnation and
counter behavior are unchanged by these diagnostics.

The private finish_reports.py writes timing-report.json after closure. For a
racing-only report, independently inspect two HUD captures on the same course
and pass their one-based event numbers:

```
python scripts/native_timing_report.py CASE --events FIRST LAST
```

The individual section reports nearest-rank p50/p95/p99, minimum, maximum and
counts above 60 Hz budget/33.33/100 ms from actual retained frame intervals.
Aggregate sections retain the independent 60-present means. Percentiles of
bucket means are never substituted for individual frame percentiles. Frame IDs
show gaps; missing GPU data is null, not zero. Unavailable query parts invalidate
the affected submission's GPU total. `NX_MEASURE_LIMIT` defaults to 2,000,000
records shared across the streams; reaching it emits an explicit truncation
marker. Abrupt process termination can also lose buffered tail records.

GPU records may arrive after the selected capture window. The reporter joins
against the whole closed log using stable submission IDs, not adjacent lines.
Two presentation watermarks delimit a group of CPU-ordered submissions; that
association does not prove simulation-frame ownership. Host event overlaps with
a hitch are observations, not additive causal costs. GPU durations use a distinct
clock domain and cannot be placed on the host timeline without calibration.
The report gives the 50 largest captured hitches and overlapping event scopes.
It reports fatal/shader-miss lines; separately review capture images and the
driver's closed result. Whole-log output includes loading/menus, not racing only.

No display scanout/input-latency measurement is provided. Reported intervals are
calls into presentation, not timestamps from the physical display.

Next attribution: compare GPU execution against caller CPU and replay in matched
racing windows. Existing R299 replay averages 5.75–7.21 ms; fence waits average
5.50–6.75 ms. A fence wait alone does not prove a GPU bottleneck. If GPU work is
small, target main-thread call costs or replay/upload work. If GPU work dominates,
use a separate targeted pass profile. Preserve correctness in either case.


Verification: 44 CTest suites; 3,212 GPU checks with diagnostics off and on;
300 sync, 300 cutoff and 259 readback checks. Eight changed translation units
analyzed: six clean, two with the identical dead-store warning reproduced in
R299, no new diagnostics. Eleven Python reporting regressions pass. A 142.4 s
intentional-deadline Stadium smoke recorded 7,491 intervals with zero ID gaps,
zero unavailable GPU queries, no truncation, zero shader misses and no fatal
lines. It joined all 7,490 groups bounded by two submission watermarks. This
includes startup/menu/loading; it is a tooling check, not a racing benchmark or
an observer-overhead estimate. A capture was reviewed showing actual racing.
