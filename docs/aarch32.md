# The AArch32 constraint

The recompiler translates **AArch64 only**. This is the single most important
limit on what the project can target, and it is the reason the repository's
name no longer describes its working target.

## What the limit is

`src/core/recompiler/arm64_to_c.h` decodes 32-bit-wide AArch64 instruction
words. AArch32 is a different instruction set — different encodings, different
register model, Thumb's variable instruction width — and none of it is
implemented. There is no partial support to extend: an AArch32 title is not
"mostly translated with gaps", it is not translated at all.

## Why it was not obvious

An unhandled instruction is stepped over and the block stays open, exactly as a
translated one does. That is deliberate — it is what lets the fallback JIT pick
up the remainder — but it means a decoder fed the wrong instruction set does not
fail. It emits fallbacks, and the export reports success.

The comment at `arm64_to_c.h:334-341` records the consequence: *"a whole module
of misdecoded ARM32 once passed for a successful export."* Every block count,
instruction count and coverage percentage produced that way was the result of
decoding ARM32 as AArch64, and was meaningless. The `unhandled` out-parameter on
`Translate` exists specifically so the emitter can distinguish a real
translation from a stream of fallbacks.

## How to tell before exporting

The NPDM header carries the flag. `ProgramMetadata::Is64BitProgram()`
(`src/core/file_sys/program_metadata.cpp:173`) reads it, and the DIAG line at
`program_metadata.cpp:52` logs `flags` and `is64` for the loaded title.
`scripts/probe-title.py` asks a running suyu the same question over MCP.

**Check this first for any new target.** An AArch32 title should be rejected at
the probe, not discovered after an export.

Module names, build IDs, and segment virtual addresses and sizes come from the
NSO headers and are ISA-independent — those remain valid even for a title the
emitter cannot translate.

## What it means for Mario Kart 8 Deluxe

MK8D is an AArch32 title (NPDM `flags=0x04`). It is therefore outside what this
recompiler can translate, and no amount of emitter coverage work changes that.
The project began aimed at it, which is where the name comes from; the working
targets since have been AArch64 titles, and the tooling takes the target from
`MK8R_TARGET` rather than assuming one.

Supporting MK8D would mean writing an AArch32 front end — a separate decoder and
a separate translation strategy, not an extension of the existing one. That is a
project in its own right and is not currently planned.

## Consequences for reading old measurements

Any measurement taken against an AArch32 target before this was understood is
invalid and should not be compared against current numbers. Coverage figures,
block counts and instruction totals are all affected. Timing measurements taken
through the fallback path are measuring the JIT, not the recompiler.
