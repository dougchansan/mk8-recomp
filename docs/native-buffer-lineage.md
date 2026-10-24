# Trace mapped buffers before separating rendering from simulation

R351 added the bounded memory-pool observer. R366 extends it to actual BufferMap
ranges and optional raw caller registers. Original API results, mappings, waits,
hardware counters, draws and simulation remain unchanged. These observations do
not establish a safe independent rendering boundary or improve FPS.

## Configuration

| Variable | Meaning |
| --- | --- |
| `NX_POOL_LINEAGE=1` | Enable pool creation, retirement and virtual-map observations |
| `NX_POOL_LINEAGE_FIRST_FRAME` | First included observed presentation counter for Map, BufferMap and Flush |
| `NX_POOL_LINEAGE_LAST_FRAME` | Last included observed presentation counter for those calls |
| `NX_POOL_LINEAGE_REGISTERS` | Optional 1–4 distinct comma-separated decimal register indices, 0–31, for BufferMap |

The observer is off by default. The enable flag must be exactly `1`. Both frame
bounds omitted enables state-transition records only. Supply both bounds for I/O
records; decimal values are inclusive and their difference must be at most
100,000. A partial, malformed, overflowing or reversed range disables this
observer while the original APIs continue.

Registers are copied before x0 becomes the original API return; no caller pointer
is followed. Invalid, duplicate, partial, overflowing or out-of-range selections
disable the observer. An omitted list emits `regs=-`; selected values use
comma-separated decimal-index:hex-value pairs.

State transitions remain eligible from startup, so a pool created before a course
loads can be joined to later observations. The process-wide cap is 8,192 records.
Exactly one `pool-lineage: v=1 truncated limit=8192` notice follows the cap; later
records are dropped and the admission counter saturates. Truncation is incomplete
observation. With the observer off, no additional frame reads, budget atomics,
map scans or I/O occur. BufferMap adds a cached admission check; no default-off
overhead or gameplay gain is inferred from the CPU controls.

## Records and authority

Every retained `pool-lineage: v=1` record includes its reserved sequence, action,
observed frame, host monotonic nanoseconds, host thread and caller LR. Addresses,
offsets, extents, flags, LR and raw register values are hexadecimal. Sequence,
frame, time, thread and `found` are decimal.

| Action | Additional fields |
| --- | --- |
| `init` | Pool key, GPU base, CPU backing, byte extent and flags |
| `retire` | Pool key, GPU base and extent, after existing waits and before unregistration |
| `map-virtual` | Virtual pool/key/base, physical pool key, offsets and extent; physical zero removes the mapping |
| `map-cpu` | Pool key, actual CPU return and existing lookup result |
| `flush` | Original pool, offset and byte-count arguments |
| `map-buffer` | Buffer/pool keys, offset, extent, actual CPU return, GPU address, found flag and raw registers |

Sequences reserve logging slots; they do not define cross-thread completion
order. Host times are not GPU timestamps. The presentation counter is not a
simulation epoch. Keys, registers and readable addresses do not lease lifetime or
identify actors. A found buffer may still have zero CPU/GPU backing. Extent is
not bytes written or uploaded; overlapping pool and buffer maps are not additive
work, and a map call does not identify every writer.

Join GPU ranges to pool base/extent and mapping history while retaining
retirement, reuse, virtual-map and capture-boundary ambiguity. A historical CPU
backing address does not prove current bytes remain authoritative after GPU
writes. The fence-retired GPU snapshot remains the data source. Missing events
are missing observations, not zero work. Raw addresses, caller PCs, program
identities, payloads and logs remain private with the title inputs.

## R366 verification and live observations

The combined runtime passes 56 component suites and 27 hardware/control cases.
The pool policy retains 59 baseline checks and adds register cases, for 85 checks.
Seven CPU stream modes execute 8,229 actual buffer maps with an authored 64-byte
synthetic module and builder getters, without title modules or GPU initialization.
Complete CPU state, original returns, same-key recreation and calls after cap
rejection are checked. Exactly one truncation notice follows the cap.

GPU controls retain 3,253 checks, 1,549 counter checks, synchronization, cutoff,
readback and six timestamp-phase modes. Valid and malformed observer settings are
covered. Five changed-source analyses report zero diagnostics. Generated CPU
inputs and archives are unchanged.

A closed moving window retains 3,829 events, including 976 BufferMap records,
without pool-trace truncation. Twenty-four GPU uniform payloads have the expected
retired headers and lengths. Twelve captured bindings have covering BufferMap
observations. Four palette captures associate with the source-traced writer and
a skeleton pointer also observed in world-matrix calculations. The separate
world inventory reaches its 128-root capacity and is incomplete.

Some covering maps follow the GPU captures in that first window. A follow-up adds
two earlier producer frames: 4,587 events include 1,464 BufferMap records and the
same 24 GPU payload captures, without pool-trace truncation. All 12 covered
bindings, including the four palette captures, then have prior logged maps.

This strengthens address/slot association. Log ordering is not a write/completion
barrier, and the other bindings still lack BufferMap observations. Complete
writer, actor, lifetime and simulation-epoch ownership remain unproved. Enabled
diagnostics remain separate from the [performance controls](native-upload-dedup-arena.md).

## Earlier R351 verification and findings

All 54 native Windows suites pass, including 59 enable/range/concurrent-cap and
counter-wrap checks. Nineteen native GPU/control cases retain 3,253 GPU checks,
300 sync, 300 cutoff, 259 readback and six 412-check phase modes. Extra healthy
controls with valid and partial lineage configuration preserve the exact GPU
assertions; partial configuration produces no lineage records. Both changed
source analyses report zero diagnostics. The primary hardware checks do not
use a Khronos validation layer.

A closed normal-60 Stadium investigation records 3,166 events without reaching
the cap: 1,701 initializations, 634 retirements, one virtual mapping, 824 maps and
six flushes. The I/O observations cover six selected frames. All 24 captured GPU
ranges fit unique recorded physical pools, and 22 have map call sites observed
in that window. No observed covering flush belongs to those captured inputs.
These are provenance and frequency observations, not measured CPU costs.

The lineage window is stationary: the movement controller was not armed before
it passed. A separate later accelerator smoke has coherent movement/wall contact
and normal HUD-clock bounds of approximately 0.9986–1.0012. Its 1,975 retained
intervals average 16.6653 ms, p99 20.4836 ms. It does not prove moving lineage or
a performance improvement. Earlier targeted moving observations remain separate.

Programs selected from a character asset also bind 33- and eight-bone buffers;
program identity alone cannot identify an entity. The exact bone-palette writer,
pose/animation ownership and independent-render scheduling remain unresolved.
Correct 125/250 FPS and sustained 8/4 ms presentation remain open. See
[render-state findings](native-render-state-lineage.md) and
[uniform capture](native-timing-measurements.md#bounded-gpu-uniform-capture).
