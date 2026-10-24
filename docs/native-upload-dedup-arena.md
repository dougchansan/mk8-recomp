# Reuse upload bookkeeping storage

This change reduces CPU allocation work in upload deduplication. It keeps the
existing ordered map, address/size/padding/alignment checks, invalidation walk,
front-cache generation, upload bytes and GPU synchronization.

Map nodes use a 256 KiB initial arena instead of returning to the general heap
after every submit. Larger workloads spill to the existing heap. Reset destroys
the map and its sentinel before reconstructing the arena and empty map, releasing
all spill allocations. Peak spill storage is not retained across submits.

The explicit upstream resource uses the existing C++ allocation functions. The
Windows imports were checked against the previous executable: no additional CRT
satellite or other DLL is required. There are no new external dependencies.

`NX_UPLOAD_DEDUP_ARENA=0` selects the direct-heap control for diagnosis; a new
process is required. The default enables the arena. Existing copied/reused byte
counters and performance scopes remain unchanged.

The shared policy test passes 59 checks covering ordered lookup, overwrite,
erase, repeated reset, empty reset, spill release and destruction. Repeated
1,536-node cases require zero upstream allocations. Four upload controls retain
51 byte/padding/alignment/ownership checks and checksum 40124; two ring-wrap
controls retain nine checks each, including two actual allocation-triggered
resets. Normal dedup and the existing no-dedup diagnostic path are covered.

The controlled comparison uses an identical timed upload fixture linked against
the unchanged published runtime and the candidate. The candidate adds only an
observer header and an unrelated CLI branch outside that timed fixture. Sixteen alternating processes
each perform 6,291,456 queries over 1,024 logical submits, retaining checksum
802160640. Median wall time falls from 1,187.580 to 1,140.984 ms (3.92%); OS thread
CPU time falls from 1,187.500 to 1,132.813 ms (4.61%); raw cycles fall 4.06%. Each of
the four paired blocks improves in all three measures. Cycles are not converted
to time. These are authored upload-copy/bookkeeping measurements, not gameplay
FPS or an isolated allocator-only timing.

An earlier variant included additional per-upload diagnostic branches and did
not establish a net benefit over the published runtime. The release candidate
keeps that upload path's existing diagnostics. The separate frozen diagnostic
builds remain available for private attribution work.

All 56 native component suites and 27 native GPU/control cases pass, including
3,253 GPU checks, 1,549 counter checks, synchronization, cutoff, readback and
timestamp-phase controls. Five changed-source analyses report no diagnostics.
Generated CPU inputs and archives remain unchanged.

Four same-executable controls use arena settings 0/1/1/0 with exact copies of
six driver-cache files, frozen pipeline seeds and the same catalog. Each has
about 45 seconds at the Stadium start line and 45 seconds holding the accelerator
into a wall. All eight windows retain normal HUD progression, coherent reviewed
geometry, complete GPU timings and no fatal or shader miss. Screenshots occur
only at the prescribed boundaries, with no interior captures.

The means below average the two process-window means per setting. Replay is an
elapsed render scope; OS thread CPU and raw cycles are separate observations.
They overlap with other work and do not form an additive critical path.

| Measurement | Start line, direct heap / arena | Held accelerator, direct heap / arena |
| --- | ---: | ---: |
| Replay wall scope ms/presentation | 4.412 / 4.229 | 2.789 / 2.429 |
| Render-worker CPU ms/presentation | 5.201 / 4.904 | 3.832 / 3.224 |
| Render-worker raw cycles/presentation | 22.024M / 21.384M | 15.961M / 14.322M |
| Mean presentation spacing ms | 16.6666 / 16.6668 | 16.6663 / 16.6669 |

Both adjacent control pairs reduce render-worker CPU and raw cycles in both
scenes. Upload volumes are similar at the reporting precision. The direct-heap
control uses the same PMR map and is not the literal previous public allocator;
the authored comparison above separately tests the actual published production.

The main thread does not show the same improvement: its mean CPU reading rises
19.5% at the start line and 9.0% while driving. Aggregate process CPU rises 4.54%
in the first scene and falls 4.89% in the second. No cause is attributed to that
variation, and no whole-process CPU, energy or FPS gain is claimed. An earlier
separate-executable comparison was mixed too. These limits remain part of the
result rather than being removed by the control order.

This is a scoped allocation/replay improvement. Presentation remains about
16.67 ms. Full-lap, all-course, physics/audio equivalence, independent rendering,
normal-speed 125/250 FPS and sustained 8/4 ms presentation remain unverified.
