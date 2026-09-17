# Super Smash Bros. Ultimate under static recompilation

Findings from 2026-09-15. Title `01006A800016E000`. Everything below was
measured unless marked otherwise.

## Result

Smash reaches **an actual match** - gameplay, CPU opponent fighting, timer
running - with three of its four modules statically recompiled and `main` on
the JIT. Full static still never presents a frame.

| configuration | outcome |
|---|---|
| all four modules static | black, never presents, 278M blocks, 0 fallbacks |
| `main` on JIT, `sdk`+`subsdk0`+`rtld` static | **match**, 0.85x realtime |
| all JIT (dynarmic) | renders, 0.85-0.97x realtime |

## How the faulty module was found

Module bisection: stage a subset of recompiled images and let the JIT cover the
rest, since in hybrid mode a module with no static image falls through to
dynarmic. Leave-one-out across four modules, plus all-static and all-JIT as
controls. `scripts/bisect-modules.ps1`.

It found the fault in **`main`** (`cross2_Release.nss`, the title's own code) in
four runs. `sdk`, `subsdk0` and `rtld` were exonerated by running **371M static
blocks and rendering a clean 3D scene** - the first time any static code in this
title produced a picture.

This worked because it asks the question empirically. Six hypothesis-driven
investigations the same day each fitted the symptom and each was falsified.

## Narrowing inside a module: the lookup hole

Module bisection stops at module granularity. To go finer, punch an
**address-range hole** in the module's lookup index: addresses inside the hole
are absent, so in hybrid mode they fall through to the JIT while everything else
stays static. Binary search on the hole boundary narrows the faulty code without
a re-export and without rebuilding the module: `SUYU_RECOMP_MAIN_HOLE=<lo>-<hi>`
is read by suyu at dispatch time and forces the lookup to miss inside the range.
Offsets are module-relative and scoped to `main`, because module bases are
picked by the loader and differ per run.

**The lever must be present in the suyu binary you are running.** An earlier
version of this document said the module DLL was rebuilt per arm, which was
wrong, and the implementation it described had only ever existed in an
uncommitted working tree - `scripts/bisect-hole.ps1` was committed against it
and no commit on any branch defined the variable. Every arm run in that window
was an ordinary all-static run: suyu ignored the variable, nothing fell through
to the JIT, and each arm reported zero transitions, which is exactly what an arm
whose range never executed reports. The 21 arms below predate that gap and are
real, but they were not reproducible until the lever was reimplemented.
`bisect-hole.ps1` now refuses to start unless the string is present in
`build\suyu\bin\suyu.exe`.

**Validate the lever at both extremes before trusting any arm.** Measured:

| control | expectation | result |
|---|---|---|
| hole covering all of `main` | behaves like `main`-on-JIT | match rendered, 5572 commands, 949k transitions, **zero `main` static blocks** |
| no hole at all | reproduces the failure | black, 319M static blocks, **0 transitions**, no frames |

The first proves the hole actually routes addresses to the JIT. The second
proves the patched DLL still fails for the original reason rather than having
been accidentally fixed or broken by the patch. Skipping either is how a
bisection ends up measuring nothing - which already happened once here, when a
handover bisect ran 24 times under `SUYU_RECOMP_STRICT` and could never hand
over at all, because strict returns `PrefetchAbort` instead of falling back.

The JIT transition count doubles as a per-arm self-check: an arm whose holed
range was never executed reports zero transitions and is uninformative,
regardless of whether it rendered. **This is not a formality** - four of the
Smash arms reported zero transitions, including the one that appeared to clear
atomics. They establish nothing about the ranges they holed.

The milestone is also not binary. Smash produced three distinct outcomes - no
frame at all, reaching the versus splash, and a real match - and an arm can
satisfy one without the others. Arm 8 holed five files and reached the versus
splash; only holing all nineteen produced a match, which is the signature of
**more than one faulty region**. Judge with `tas_peak_frame` plus the captures,
not a single pass flag.

`scripts/bisect-hole.ps1` drives this. The hole is read at runtime from
`SUYU_RECOMP_MAIN_HOLE=<lo>-<hi>`, so an arm needs neither a re-export nor a
module rebuild.

### Correction: MK8's "title-screen stall" was a mis-export

An earlier version of this document recorded MK8 static stalling at the title
screen as an older, independent bug, reproduced on a pre-session build and
unaffected by the chain budget. **That was wrong.**

The exporter resolved ExeFS only from inside the ROM container, so MK8 Deluxe's
64-bit build - which ships in the installed update - was invisible to it and the
cartridge's **32-bit ARM (NX32)** base program was recompiled instead. The
AArch64 decoder never rejects A32, since every word decodes as *something*, so
the export reported success. Measured: `main.npdm` META flags `0x04`
(Is64BitInstruction=0) against `0x07` for a good export, rodata build paths
reading `NX32` rather than `NX64`, and 87% of blocks unhandled against 0.0003%.

Every MK8 static result in this document predating the fix used modules built
either from that misread or from before it. With the exporter running ExeFS
through `PatchManager::PatchExeFS` - the same call the loader makes - MK8 runs
**fully static, 8.43 billion blocks, zero fallbacks, and renders a race**.

The 4,669 undefined shifted-register encodings were the fingerprint of A32 read
as A64, not a decoder defect. The guard that rejects them is still correct, and
it is what made the wrong input visible instead of silent.

### A re-export may or may not invalidate an address hole - check, do not assume

Module-level bisection survives a re-export - it names modules, and module
identity is stable. An address-range hole is conditional, and an earlier version
of this document overstated it as "a re-export invalidates an address hole".

Block addresses are **guest** addresses. They do not move because the emitter
changed. They move when the **set of discovered blocks** changes, because
translation units are packed at a fixed 20,000 blocks each and any change to
discovery shifts every boundary after it. Two things do that: reading a
different input image, and an emitter change that alters what gets handled
versus routed to `put_unhandled`.

Both outcomes are measured here:

- **Invalidated.** MK8's hole stopped covering its range after the exporter was
  fixed to resolve ExeFS through the update - the exporter began reading a
  different binary entirely (NX32 to NX64), so every address moved.
- **Survived.** Smash was re-exported on 2026-09-16 with an emitter carrying the
  FP scan optimization and the shifted-register guard. TU boundaries came back
  byte-identical - 132 = `0x29d9794-0x2a16830`, 133 = `0x2a1683c-0x2a6b7ac`,
  134 = `0x2a6b7e4-0x2acebdc` - so the 21-arm result below still stands without
  re-deriving. The shifted-register guard changes nothing on a clean ARM64
  title, so discovery was identical.

The check is cheap and settles it in seconds, so run it instead of assuming in
either direction: grep the generated tree for the hole's low endpoint as a
`blk_main_<16 hex>` symbol and confirm the enclosing TU's first and last block
still match. A low endpoint that is absent, or a TU whose bounds have shifted,
means re-derive. (The high endpoint is an exclusive bound and is normally *not*
itself a block start - `0x2a6b7e0` sits between TU133's last block and TU134's
first. Its absence is expected and is not evidence of invalidation.)

Re-derive the range when that check fails, or keep the old generated tree if the
configuration still matters. And **archive the module DLLs before re-exporting**
if the current build is a measurement baseline - an export overwrites them in
place, and without the old binaries a before/after comparison cannot be
interleaved and so cannot be separated from this title's ordinary 0.85x-0.97x
run-to-run spread.

### Inside TU133: three faults, not one

13 further arms on 2026-09-16, after the lever was reimplemented. Both controls
behaved: full hole reached a match (peak 5556, 855k transitions, verified by
opening the capture); no hole reproduced the black screen (279M blocks, zero
transitions).

The prior search stopped exactly at a TU boundary because **no single sub-range
ever produces a clean pass** - TU133 holds at least three independent defects,
and fixing any one leaves the others. They are distinguishable only by what the
captures show:

| fault | range | size | symptom when static |
|---|---|---:|---|
| A1 | `0x2a56ffc-0x2a582e4` | ~5 KB | sleep spin, no frame at all |
| B | `0x2a286c8-0x2a39dac` | ~73 KB | renders menus, never enters a match |
| C | `0x2a5ed88-0x2a62b36` | ~55 KB | loading screen, only visible once B is bypassed |

Narrowest hole still reaching a full match: `0x2a286c8-0x2a62b36` (238 KB),
down from the 348 KB of the earlier result.

**`tas_peak_frame` does not separate these.** Measured in the same session:
a real match scored 5548 and 5568; the stage-select screen scored 5551 and 5555.
The match runs *bracket* the menu runs. The replay is frame-indexed and consumes
every command whether or not a match started, so in the 5548-5568 band the
number carries no information and only the image does. Peak frame still
separates gross failures (0, 447) from "ran to the end of the script".

The outcome ladder is also finer than three rungs. Observed, in order: black
with nothing; black with a live loading spinner (the render path works, content
never arrives); stage select; real match.

### Density comparison against a proven-innocent span

Once a span is guilty and another span in the same TU is proven innocent, the
generated C answers "what is different about this code" without a ROM or a
disassembler: every block carries its original ARM64 words in the `_expected[]`
code-guard array, so the guest instruction stream can be recovered exactly and
counted. Q1 (`0x2a1683c-0x2a286c8`) is the innocent control - arm 5 held it
static and still reached a full match.

This was productive at **eliminating**, and eliminated every standing
hypothesis. Measured in the guilty span:

| hypothesis | result |
|---|---|
| atomics, `LDAXR`/`STLXR` | **0** in guilty; all 44 in TU133 are in the innocent span |
| `LDAR`/`STLR` barrier gap | 0 occurrences |
| `LDNP`/`STNP` unimplemented | 0 occurrences |
| FP/SIMD, and the FP scan optimization | 0 FP instructions (245 in the innocent span) |
| shifted-register guard | 0 undefined `sf=0 amt>=32` encodings; `unhandled` = 0 |
| `LSLV`/`LSRV` shift masking | correct - emits `& 31` / `& 63` |
| `CLZ` | correct, including `CLZ(0)` = width |
| `BFM` wraparound (`immr>imms`) | correct - right mask, right shift, other bits preserved |

The last three were checked by reading the emitted C, not by inference. The
`LDAR`/`STLR` gap is real but is **not** this fault, which settles the earlier
guess that the bisection corroborated it.

What the span *is*: shifts, `CLZ`, bitfield inserts, no FP, no atomics, fewer
loads than average, and `BFM` enriched ~33x with every instance in the
wraparound form. Reading it shows successive `recomp_load8` at descending
offsets shifted into place - a byte-by-byte big-endian deserializer. That agrees
with the original spin-site finding, where the polled object carries
`rom:/data.arc`. The remaining defect is a wrong **value** computed in archive
parsing.

Next probe should be differential rather than statistical: run the blocks in
`0x2a56ffc-0x2a582e4` under the static image and under dynarmic from the same
entry state and compare registers. `tests/differential_*` already does this for
synthetic input; the gap is driving it from a recorded guest state.

Unrelated performance find: `CLZ` is emitted as a bounded per-bit loop, the same
shape the FP scan optimization replaced for floating point. 12 sites in this
5 KB span alone, in hot parsing code. `recomp_bit_index64` already exists.

### Where the Smash fault sits (earlier 21-arm result)

21 arms. Both controls behaved: no hole reproduced the black screen (319M
blocks, zero frames); a full hole reached a match (peak frame 5559).

The narrowest **working** hole was `0x2a1683c-0x2a6b7e0` - about 348 KB, some
87,000 instructions, inside translation unit 133:

```
SUYU_RECOMP_MAIN_HOLE=0x2a1683c-0x2a6b7e0
```

That configuration completed the replay to frame 5568 with **7.19 billion
static blocks** and 405k JIT transitions - roughly 55x more static execution
than routing all of `main` to the JIT, which is the next best working setup at
130M blocks.

| arm | peak frame | static blocks | transitions |
|---|---:|---:|---:|
| full hole (all of `main` on JIT) | 5559 | 130 M | 939k |
| b14 | 5559 | 1.11 B | 112k |
| b6 (19 files holed) | 5560 | 1.69 B | 728k |
| b7 | 5560 | 4.55 B | 308k |
| **b12** | **5568** | **7.19 B** | 405k |

The fault was not narrowed below that range before the work stopped. Candidate
files were 132 (30 `LDAR`, 22 `STLR`, 16 `LDAXR`, 27 `STLXR`) and 133 (32
`STLXR`, no `LDAR`/`STLR` at all); the surviving range is in 133. An early
guess that this corroborated the `LDAR`/`STLR` barrier gap does **not** hold -
that gap is real, but it is in the file the search moved away from.

## Two bugs fixed

**Undefined weak symbols resolved to a callable stub.** `ApplyRelocTable`
patched every unresolved GOT/PLT slot to a return stub in the module's own text,
but an undefined `STB_WEAK` symbol resolves to 0 by ABI. That breaks
`if (&weak) weak(...)`: the null check passes and the guest calls a function that
only returns 0. Smash guards `nu::VirtualAllocHook`/`VirtualFreeHook` that way at
**5,489 sites** on its allocator path. Committed; it did **not** change this
title's behaviour and is not the Smash fix.

**Pre-applied relocations broke deferred NRO binding.** Relocations were
pre-applied to every module and `DT_RELASZ`/`DT_PLTRELSZ` zeroed so rtld's
self-relocator would skip them. That left rtld unable to record what `nn::ro`
needs later, so **909 deferred `lua2cpp` fighter-agent JUMP_SLOTs** stayed
pointing at the return stub and no match could load. Measured: `0/909` bound,
across the NRO loads and 60s of observation.

The fix is policy `rtld-only` - pre-apply and zero **rtld alone**, leave every
other module to rtld's own pass, as dynarmic does. It relies on no ordering
guarantee.

Restoring the sizes after rtld **does not work**: rtld caches them into its own
module state when it first parses `.dynamic`, and never re-reads the table. That
was built and measured, not assumed - `restore-on-main` fires cleanly at t=17.5s,
well before the first NRO at t=25s, and still binds 0/909. Zeroing rtld only
gives **12,071 svcBreaks**, so rtld's C++ pass genuinely does trip over entries
already resolved.

Not answered: *why* rtld's pass breaks on pre-applied entries. `rtld-only`
sidesteps that. It returns if a title ever needs its non-rtld modules
pre-applied.

## The remaining `main` fault

Not a deadlock - two guest threads runnable, ~87% of SVCs are `svcSleepThread`,
and the ~300M blocks are overwhelmingly yield-poll. The spin is
`ldrb w8,[x19,#0xe6]; cbz w8` at `cross2_Release.nss+0x26d6f90`; the object at
`x19` carries `rom:/data.arc`; the state byte at `[x19+0xe4]` is guarded by
`cmp state,#1; b.lt`, and `+0xe6` is never written across 105 trapped stores.

The generated code at the spin site and its producer stores was hand-verified
correct by decoding the guard arrays, so the divergence is upstream. It is a
wrong **value**, not a wrong address - instrumented unmapped-VA detection never
fired.

## Falsified - do not revisit without new evidence

- Lost wakeup on an `nn::os` arbiter. 26 mutex unlocks all wrote correct
  next-owner tags. The five threads in `WaitForAddress` are on distinct
  per-thread park words, each reading 1 - idle workers, not contention.
- Undefined weak symbols as the cause. Fixed; block counts and SVC mix
  unchanged.
- Chain-budget stack depth. Budgets 32, 256 and 4096 measured identical,
  including the value the emitter comment records as having crashed on boot.
- Fast-path `recomp_host_ptr` vs `HostLoad`/`HostStore` semantics. Both mask
  identically and funnel through the same `GetPointerImpl`.
- The compiler hoisting the poll load. `recomp_load*` are out-of-line in a
  separate translation unit at `-O1` with no LTO.
- Shader compilation as the speed bottleneck. Interleaved measurement:
  `use_asynchronous_shaders` on is **slower** (0.839x vs 0.875x median).

A diagnostic also fabricated its own evidence: the guest thread dump printed
`m_address_key`/`m_address_key_value` beside threads parked in
`svcWaitForAddress`, which never writes those fields and never clears them, so
stale keys from unrelated completed waits were reported as the addresses being
waited on. Two analyses reasoned from those values before it was caught.

## Emitter defects found by the synthetic differential

Memory coverage was added to `tests/differential_*` (111,616 comparisons, no
mismatches). It surfaced two real gaps:

- **`LDAR`/`STLR` emit plain loads and stores with no barrier.**
  `recomp_barrier()` is emitted only for explicit `DMB`/`DSB`/`ISB` encodings.
  A single-threaded differential cannot observe this, so those cases pass -
  passing is not evidence here.
- **`LDNP`/`STNP` are unimplemented** by `Translate()`.

Coverage is **MSVC only**. The GCC path is compile-verified but not executed,
because no Dynarmic oracle built from this fork's `src/dynarmic` exists in the
Linux image. Given the emitter has separate MSVC and GCC branches, that is a
real limit rather than a footnote.

## Measurement notes

Every automated pass signal here has at some point passed a run that went
nowhere: `tas_outcome: COMPLETE` (the replay is frame-indexed and consumes every
command while parked on a title screen), `rendered_visible_content` (only asks
whether pixels vary within one frame), `frozen_tail` (byte equality, defeated by
animated fireworks and a moving lens flare), and `FirstFrame`. Three benchmark
ratios were each comparing different scenes. **Open the captures.**

`rendered_visible_content` also false-negatives: it requires every capture to be
non-uniform, and the first one lands during boot and is black.

Single runs are not trustworthy. Dynarmic measured 0.852x and 0.965x realtime on
the same fixture in the same session. Interleave arms and repeat.
