# Native command-recording headroom

R330 reduces repeated capacity-check locking in native command recording. The
common path checks both command and control extents, including the eventual end
marker, and appends while holding the same buffer lock. Replenishment retains
the original callback, owner-thread, retry and identity checks outside that lock.
Command order, copied inline data, usage counters and submitted work stay intact.
`NX_RECORD_CAPACITY_FAST=0` selects the original comparison path; the default is 1.

Callback profiling identified draw recording as the largest measured main-thread
callback. The surrounding task tree also contains input and other work; it cannot
be treated as one simulation-only update. See the
[bounded main-loop profiler](native-main-loop-profiler.md).

## Recording-only control

Twenty-four same-executable processes alternate old/fast/fast/old six times.
Each records 37,888 authored commands in 16/64/512-command lists and passes 49,286
checks for order, contents, copied payload ownership and exact command/control
usage, including the end marker. Every process has the same checksum 557526016.
The timer covers recording, after native GPU initialization; no guest modules
are loaded and the benchmark does not replay a game frame.

| Recording path | Mean elapsed | Median elapsed | Mean raw thread cycles |
| --- | ---: | ---: | ---: |
| Original | 3.082 ms | 3.037 ms | 13.346 million |
| Single-lock capacity fit | 2.652 ms | 2.644 ms | 11.515 million |

The mean elapsed reduction is 13.96%. Raw thread cycles are not elapsed-time units
and are never converted to milliseconds. This ratio describes recording work,
not whole-game FPS or presentation latency.

## Normal-speed game comparison

Four closed, stationary Mario Kart Stadium TimeTrials Solo flights use the same
R330 executable and original save, strict shader coverage, the same private
recipe inputs and separate clones of the same warm driver-cache snapshot. Only
the recording control changes, in old/fast/fast/old order. Windows stay inactive
on the secondary monitor; scripted input uses shared memory. Each reviewed
racing window contains 3,601 individual presentation intervals and 29 sampled
renderer callbacks. Menu/loading intervals are excluded.

| Flight | Recording | Draw-recording callback mean | Presentation mean | Presentation p99 |
| --- | --- | ---: | ---: | ---: |
| A0 | Original | 7.499 ms | 16.667 ms | 19.278 ms |
| B1 | Fast | 6.021 ms | 16.666 ms | 19.112 ms |
| B2 | Fast | 5.928 ms | 16.667 ms | 19.716 ms |
| A3 | Original | 6.549 ms | 16.667 ms | 19.696 ms |

The two-flight callback means are 7.024 ms original and 5.974 ms candidate, a 14.94%
observed reduction with diagnostics enabled. Mean raw callback cycles fall from
29.687 million to 25.002 million. Recorded command volume averages 4,715.5 versus
4,708.1 per presentation, about 0.16% different. These volume estimates use
differences between cumulative end-recording counters; log-order presentation
anchors retain boundary ambiguity and are not exact simulation-frame ownership.

All four HUD boundary pairs show the same stationary position, lap 1/3 and a
normally advancing race timer. Capture brackets put timer/host rate near 1.0
(approximately 0.9990..1.0011 across flights). No fatal error, shader miss,
missing presentation ID, measurement truncation or dropped callback observation
occurs in the reviewed windows. Each flight captures all 77 observed callback
keys across 29 roots. Source, executable, 406 generated CPU inputs and three CPU
archives remain pinned.

These are two independent flights per setting with different scene state and
desktop scheduling, not deterministic replay. Baseline callback means vary
substantially; do not generalize the ratio to other courses or unprofiled play.
The 60 Hz presentation cadence remains approximately 16.67 ms. The p99 results are
mixed and establish no isolated-hitch improvement. No frame in these windows is
under 8 ms. GPU execution time and display scanout are not measured in this flight
set. True 125/250 FPS still requires normal-speed render/simulation separation;
the [clock audit](native-frame-clock-audit.md) rules out the uncapped override as
proof of that goal.

## Verification

The final source passes 48 CTest suites, 3,253 native GPU checks with stock/valid/
corrupt seed controls, the existing legacy/per-draw/phase GPU modes, 1,549 precise
counter checks and 412 phase checks in six modes. Sync/cutoff/readback controls
remain 300/300/259. The original recording path also passes the full GPU fixture;
both paths reject oversized, empty-tail and exhausted command arenas identically.
All seven changed final C++ translation units have zero Clang analysis diagnostics.
The 220-file generic recipe applies to the pinned upstream and reconstructs the
verified source. Private title offsets, captures, caches and binaries are excluded.
