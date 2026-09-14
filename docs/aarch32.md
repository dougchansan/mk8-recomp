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

**Corrected 2026-09-13.** This section previously said the title was AArch32 and
therefore permanently out of scope. That is true of one of its two programs and
false of the one that runs.

The cartridge carries a base program flagged AArch32, and the v4.0.0 update
carries a program flagged AArch64. Booting the cartridge normally loads both
NPDMs and runs the update:

```
DIAG NPDM name='main.npdm' ... flags=0x04 is64=0    <- cartridge base
DIAG NPDM name='main.npdm' ... flags=0x07 is64=1    <- update, and what loads
```

Both were booted to confirm it. With the update applied the AArch64 program
loads and renders; with the update disabled through `DisabledAddOns` the
AArch32 base loads and also renders, on the 32-bit JIT. Neither fails to boot —
the difference is only which instruction set is executing, and therefore whether
this recompiler has anything to say about it.

So the working target is the update, it is AArch64, and it is what every
measurement in this project has been taken against. The evidence that it is
genuinely being translated rather than misdecoded is in the numbers themselves:
a misdecoded module produces a flood of unhandled instructions, and this one
produces almost none, executes billions of blocks, and completes a recorded
replay.

Export from the update, not the cartridge. `MK8R_EXPORT_ROM` exists for exactly
this reason and is separate from `MK8R_ROM` for exactly this reason.

Supporting the *base* program would still mean writing an AArch32 front end — a
separate decoder and a separate translation strategy, not an extension of the
existing one. That remains unplanned, and there is no reason to want it: the
update supersedes the base.

## Consequences for reading old measurements

Any measurement taken against an AArch32 target is invalid and should not be
compared against current numbers. Coverage figures, block counts and instruction
totals are all affected, and timing taken through the fallback path measures the
JIT rather than the recompiler.

The probe still matters for any *new* target, and for any title where no update
is installed. Check `is64` before spending an export on it.
