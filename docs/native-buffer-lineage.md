# Trace mapped buffers before separating rendering from simulation

R351 adds a bounded memory-pool observer for connecting a captured GPU input to
the game's mapped CPU backing and observed API call sites. It changes no pool
operations, return values, mappings, original waits, hardware counters or draws.
It does not establish a safe independent rendering boundary or improve FPS.

## Configuration

| Variable | Meaning |
| --- | --- |
| `NX_POOL_LINEAGE=1` | Enable pool creation, retirement and virtual-map observations |
| `NX_POOL_LINEAGE_FIRST_FRAME` | First included observed presentation counter for Map/Flush calls |
| `NX_POOL_LINEAGE_LAST_FRAME` | Last included observed presentation counter for Map/Flush calls |

The observer is off by default. The enable flag must be exactly `1`. Both frame
bounds omitted enables state-transition records only, with no hot Map/Flush
records. Supply both bounds to observe I/O; decimal values are inclusive and
their difference must be at most 100,000. A partial, malformed, overflowing or
reversed range disables this observer while the original APIs continue.

State transitions are retained from startup even when the I/O window starts
later, so a pool created before a course loads can be joined to a later buffer.
The entire stream has a process-wide cap of 8,192 records. Exactly one explicit
`pool-lineage: v=1 truncated limit=8192` notice follows the cap; later records
are dropped. The admission counter saturates. A truncated stream is incomplete.
With the observer off, no frame reads, budget atomics, map scans or I/O occur.

## Records and authority

Each retained `pool-lineage: v=1` record includes a unique reserved sequence,
action, observed frame, host monotonic nanoseconds, host thread and caller LR.
Addresses, offsets, byte counts, flags and LR are hexadecimal. Sequence, frame,
host time, thread and `found` are decimal.

| Action | Additional fields |
| --- | --- |
| `init` | Pool key, GPU base, CPU backing, byte extent and flags |
| `retire` | Pool key, GPU base and byte extent, after existing waits and before unregistration |
| `map-virtual` | Virtual pool/key/base, physical pool key, physical/virtual offsets and extent; physical zero removes the mapping |
| `map-cpu` | Pool key, actual returned CPU address and whether the existing lookup found a pool |
| `flush` | Existing pool, offset and byte-count arguments |

Sequences reserve logging slots; they do not define a cross-thread resource
completion order. Host event times are not GPU execution timestamps. The frame
is the existing observed presentation counter, not a simulation ownership token.
Pool keys and readable addresses alone do not lease lifetime or identify actors.

Join a bound GPU range to its recorded pool base/extent and mapping history.
Retain retirement, reuse, virtual-map and capture-boundary ambiguity. A historical
CPU backing address does not prove its current bytes are authoritative after GPU
writes. The existing fence-retired GPU snapshot remains the data source. Missing
map/flush records are missing observations, not evidence of zero work or a fault.
Raw addresses, caller PCs, payloads and logs remain private with the title inputs.

## Verification and findings

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
