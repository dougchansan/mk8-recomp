# Bounded main-loop and callback profiling

The recipe adds opt-in caller, stack and callback observations to identify
CPU work before changing frame scheduling. The ordinary indirect-dispatch path
keeps its cached disabled-mode branch. Profilers preserve each original call,
CPU outputs, exception propagation, waits, draws and presentation behavior.
Measurements include observer overhead and require matching unprofiled controls.

## Caller and wait context

`NX_FRAME_CONTEXT=1` enables bounded owner-thread snapshots at presentation and
the native SVC mutex/condition-wait paths. `NX_FRAME_CONTEXT_START`, `STOP`,
`EVERY` and `LIMIT` select completed-presentation IDs and a global record cap.
Frame IDs describe presentation progress, not simulation ownership.

Snapshots read complete two-word records only within the bound active owner
stack. At most 16 frame records are followed. Preserve the reported stop reason:
a prefix ending at a limit, unreadable record or invalid chain is incomplete.
An outer native-started thread's zero frame is distinct from an interior zero LR.
Wait beginnings reserve their completion record, which survives the frame window
and global cap. Deduplication includes operation, mutex, key and caller.

Mutex records include requester/current/owner handles and whether execution
actually entered the blocking path. A zero-duration or missing record does not
prove absence of waiting; a caller selection or coverage gap may explain it.
Elapsed waits include scheduling and pacing. Overlapping worker activity does
not prove what caused a wait, and wait duration is not permission to remove it.

## Exact dispatch phases

`NX_CPU_PHASE_PROFILE=1` enables phase observation. `NX_CPU_PHASE_SITES` is a
comma-separated list of hexadecimal `caller:target` pairs, root first;
the second address is the actual indirect-call target, not a return address.
For example, the schema is `ROOT_LR:ROOT_TARGET,PHASE_LR:PHASE_TARGET`, with all
labels replaced by addresses from one's matching local compiled modules.
No title-specific selector or version map is embedded in the recipe.

R339 also accepts `ROOT_LR:*` as the first pair for a worker-dispatch inventory.
Only the root may use a wildcard; zero targets, inner wildcards and malformed
lists disable the observer. Recorded targets are the actual dispatched functions.
Exact roots retain once-per-thread/presentation selection. Wildcard roots use
per-thread/presentation target, object, vptr, readability and selected-context
keys, with the same global cap of at most 128 roots. Keys describe observations,
not resource incarnations. A rejected, deduplicated or capped outer dispatch
still owns its nested tail calls, so those calls cannot become false roots.

`NX_CPU_PHASE_START`, `STOP`, `EVERY` and `LIMIT` bound sampled roots. Inner phases
remain attached to the owning root if presentation advances during a wait.
Repeated/nested tail calls do not replace the root or double-count an open
phase. Timing ends before deferred root log emission. Each phase reports calls,
elapsed wall time, valid raw-cycle sample counts and an inclusive HLE inventory.
The inventory caps distinct entries and reports overflow calls explicitly.

Match the actual virtual target, including any derived wrapper or thunk. A base
implementation or nearest symbol may not be the function dispatched at that LR.
Zero records can indicate an incorrect selector. Confirm the observed object,
readable vptr and callback activity before assigning update/render roles.

## Selecting queued contexts

Optional root metadata selection uses all three settings together:

| Setting | Meaning |
| --- | --- |
| `NX_CPU_PHASE_ROOT_REGISTER` | Decimal CPU register-array index, 0 through 31 |
| `NX_CPU_PHASE_ROOT_OFFSET` | Hexadecimal byte offset, aligned to eight bytes and at most `fff8` |
| `NX_CPU_PHASE_ROOT_VALUES` | One through eight distinct hexadecimal 64-bit values, separated by commas |

During a sampled candidate root, the register must contain an aligned, nonzero
base address. The observer adds the configured offset with overflow checks and
reads one complete eight-byte value only
inside a readable registered VM range, then compares it with the whitelist.
The range check does not acquire a lifetime lease. Select metadata whose lifetime
is owned by the active callback.
Unreadable or unmatched metadata produces no root observation; the original dispatch
and nested calls continue. Partial, malformed, duplicate, oversized or overflowing
configuration disables the entire observer. Zero is a valid metadata value.

Separate `call phase context` records bind the selected value to its owning
root, thread and presentation ID. The existing v1 phase-row schema remains intact.
Context changes at the same target/object/frame remain distinct wildcard roots.
Keep local selector maps and logs containing guest addresses/data private.

This selects measured producer/consumer groups; it is not an automatic simulation
classifier. A matching job label or a draw-free HLE inventory does not prove that
the job can be skipped or run at a different cadence.

## Callback breakdown

`NX_CPU_PHASE_CALLBACK_CALLER` optionally selects one hexadecimal callback caller
within a sampled root. The observer records each target/object/vptr combination
within that root, with a 128-entry cap and explicit dropped-call count. A full
table still aggregates an existing key. Every original callback continues to
execute, including callbacks whose observations cannot be stored.

Callback records include inclusive/exclusive elapsed time and raw thread cycles.
Exclusive cycles have their own valid-sample count; unavailable child readings
do not become fabricated exclusive work. Nested callbacks are subtracted only
on the same thread. No raw cycle counter is converted to elapsed time.

HLE activity counts describe command, draw, compute, input, tick and submission
entry points. Draw/compute counts overlap command counts, and child-inclusive
counts overlap parents. Do not add them. A callback without draw calls is not
automatically a pure simulation tick; inspect its real state and consumers.
Per-root address/vptr observations are not resource-incarnation IDs, and grouping
addresses across roots does not prove that an object retained the same lifetime.

Authored regressions cover paired/capped wait streams, actual mutex fast/blocking
paths, malformed selectors, cross-frame phase completion, invalid objects,
exceptions, nested wall/cycle exclusives, full-table duplicates and explicit
overflow while all 569 original leaf callbacks execute. The
[recording comparison](native-command-recording-performance.md) applies these
tools to reviewed normal-speed racing; it makes no true high-refresh claim.

R339 adds actual-loader stream checks for distinct worker targets/objects,
same-frame dedup, nested tails, two threads, full CPU outputs, a sampled exception
and the global cap. The filtered variant preserves all 13 original calls and
12 leaves while emitting six roots and five callback records. It tests metadata
mismatch, unaligned/unmapped/overflowing addresses, changed context at the same
object/frame and separate context ownership. A partial-configuration process
rejects profiling while preserving every original call.

## Worker ownership and render preparation

A subsequent closed Stadium Grand Prix capture found no selected job-execution callbacks
inside 29 sampled main-thread roots. Selecting the actual worker entry instead
observed 32 jobs on two threads, with 96 child-callback records, six address/vptr
keys and no dropped callbacks. Two wrapper objects shared the compiled target
and vptr: the 32-root cap covered 16 sampled presentation IDs, rather than the
entire configured window. Preserve that distinction when reporting coverage.
The worker capture used Time Trials Solo. These are separate coverage
observations, not a matched performance comparison.

Those child callbacks had no observed HLE or draw calls. Static inspection of
the matching private modules identified a timer consumer that converts an
integer frame count to milliseconds. This identifies an execution path; it does
not establish a CPU bottleneck or ownership of the complete simulation.

Two private scheduling experiments demonstrate why stronger state evidence is
required. Throttling one nested cohort left the race clock accelerated. Gating
the broader update task brought the stationary clock near real time, but some
reviewed racing images lost the course and kart while retaining HUD and effects.
Other images in the same run retained the geometry. Both experiments were
rejected; their implementations are not part of the published recipe.

The selected update boundary therefore cannot be treated as simulation-only,
even when its owner-thread draw count is zero. The exact split between simulation
state, per-presentation preparation and rendering consumers remains unresolved.
Trace those consumers and worker effects before changing scheduling. Retain
every original wait, resource identity check and complete draw path; neither
reusing presents nor advancing bookkeeping counters substitutes for that work.
See the [clock qualification](native-frame-clock-audit.md) for the measured
timing limits of these rejected experiments.

A later R339 metadata-selected capture covers all 29 configured sample frames
for each of three queued contexts: 87 roots, with no root-cap truncation, dropped
callbacks or HLE inventory overflow. Observed mean root times, including
instrumentation, are:

| Selected queued context | Samples | Mean observed root time |
| --- | ---: | ---: |
| Position | 29 | 0.0263 ms |
| Movement | 29 | 0.1490 ms |
| Apply | 29 | 0.2540 ms |

All three have zero observed HLE/draw calls. They resolve to different compiled
callees, and the apply stage has different state consumers. These measurements
guide that investigation; they do not prove which work is safe to reschedule or
how much presentation time would be saved. Raw thread cycles remain raw cycles,
and address grouping remains distinct from incarnation proof.
