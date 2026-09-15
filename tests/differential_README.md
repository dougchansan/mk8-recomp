# Synthetic execution differential harness

This test links an existing Dynarmic build as a **test oracle**, executes one real
AArch64 instruction with `Jit::Step()`, and compares against compiled C emitted
by the current `Translate()`. Product binaries and their no-JIT configuration
are unaffected. No cartridge content is used.

Copy `tests/differential_*`, `scripts/run-differential.sh`, and the emitter header
into an isolated directory. Preserve `tests/` and put the emitter at
`core/recompiler/arm64_to_c.h`. From that directory run:

```sh
bash run-differential.sh "$DYNARMIC_SOURCE" "$SUYU_BUILD" "$FMT_INCLUDE"
./differential-run --fp --samples 4
./differential-run --fp
./differential-run --fp-new --samples 4096
./differential-run --fp-new --word 0x4e22f420 --samples 33
./differential-run --inject-mismatch --samples 1
```

All three paths must refer to existing compatible artifacts. This fork's active
Dynarmic source is `src/dynarmic`, **not** the inactive `externals/dynarmic` copy.
The script verifies the source against `compile_commands.json`. Its tiny logging
adapter prints library diagnostics and aborts on assertions; it does not replace
instruction execution or arithmetic. GCC UBSan instruments the emitted C.

Every case compares all 31 GPRs, SP, 32 SIMD registers, NZCV, FPCR and FPSR, and
checks the oracle advanced PC by four. Memory instructions are not yet included;
any unexpected memory access or oracle exception exits 2. Current coverage is
single-instruction integer arithmetic/flags/SIMD/saturation and scalar FP. It is
not yet a block, memory, concurrency, hardware or exhaustive ISA differential.

`--seed NUMBER` and `--samples NUMBER` reproduce deterministic input states.
Integer inputs include fixed edges followed by random states. `--fp` starts with
four exact values followed by unrestricted IEEE bit patterns, at FPCR=0.
`--fp-new` adds min/max and reductions, rounding, precision conversion, FP-to-integer,
sqrt/absolute difference, vector arithmetic and indexed fused operations. It cycles
all 16 rounding/FZ/DN combinations and includes IEEE edge payloads before random
inputs. `--word` selects one encoding without changing its reproducible inputs.
The build checks 46 instruction/mnemonic pairs against GNU AArch64 assembler.
Failures stop immediately with exit 1 and print seed, case seed, encoding,
sample, field and both values. `--inject-mismatch` is an explicit detector test:
its expected result is exit 1, never a successful validation run.

Verified on the existing Linux library: 7,680 integer cases and 1,536 ordinary
scalar FP cases passed after the arithmetic implementations were corrected.
The initial harness exposed the FADD inexact flag mismatch at encoding
`1e622820`, sample 4; that original regression now passes.

Broader full-state runs still report divergences. For example, `--fp-new`
first stops at FMAX `4e22f420`, sample 32: emitted FPSR=0, Dynarmic FPSR=1.
Other targeted cases expose FRINTN inexact, FCVTNS infinity-invalid and FZ
input-denormal flag disagreements. Arithmetic outputs match up to those stops.
Dynarmic is an independent implementation, not architectural truth: the FMAX
quiet-NaN flag discrepancy has been traced to its native MAXPS behavior, and
its FPSR mapping omits host input-denormal status. Keep every mismatch visible
and resolve against architectural evidence; do not normalize FPSR to make tests
pass. The harness does not establish full FP equivalence.

Cache invalidation is requested **after** `Reset()`: resetting after requesting
invalidation clears Dynarmic's pending halt flag and can reuse stale code at the
fixed synthetic PC. The runner follows the verified ordering.
