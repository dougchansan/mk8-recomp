# Synthetic execution differential harness

This test links an existing Dynarmic build as a **test oracle**, executes one to
three real AArch64 instructions with `Jit::Step()`, and compares against
compiled C emitted by the current `Translate()`. Product binaries and their
no-JIT configuration are unaffected. No cartridge content is used.

Copy `tests/differential_*` and the emitter header into an isolated directory.
Preserve `tests/` and put the emitter at `core/recompiler/arm64_to_c.h`.

On Linux, from that directory:

```sh
bash run-differential.sh "$DYNARMIC_SOURCE" "$SUYU_BUILD" "$FMT_INCLUDE"
./differential-run --memory --samples 512
./differential-run --memory-seq --samples 512
./differential-run --fp --samples 4
./differential-run --fp-new --samples 4096
./differential-run --fp-new --word 0x4e22f420 --samples 33
./differential-run --inject-mismatch --samples 1
./differential-run --memory --inject-memory-mismatch --samples 1
```

On Windows, `scripts/run-differential.ps1` does the staging, build and run with
MSVC against the Dynarmic library already present in `build\suyu`. It only reads
that build; it never rebuilds it.

```powershell
scripts\run-differential.ps1 -Clean -RunArgs @('--memory','--samples','512')
```

All three Linux paths must refer to existing compatible artifacts. This fork's
active Dynarmic source is `src/dynarmic`, **not** the inactive
`externals/dynarmic` copy. Both runners verify the source against
`compile_commands.json`. The tiny logging adapter prints library diagnostics and
aborts on assertions; it does not replace instruction execution or arithmetic.
GCC UBSan instruments the emitted C.

## What is compared

Every case compares all 31 GPRs, SP, 32 SIMD registers, NZCV, FPCR and FPSR, and
checks the oracle advanced PC by four per instruction. Memory cases additionally
compare the whole shared memory region byte for byte: registers agreeing is not
enough, because a store that went to the wrong address, or did not happen at
all, is only visible in memory.

The harness runs the **real recompiler runtime**, emitted by
`differential-generate --runtime-h` and `--runtime-c` and compiled alongside the
translated code, so the state both engines share is the production
`GuestContext` and memory cases call the production load, store, pair and
exclusive helpers. A private reimplementation of those would have tested the
harness rather than the recompiler. The only symbol stubbed out is
`recomp_lookup`, which a generated module would otherwise supply.

## Coverage

`--seed NUMBER` and `--samples NUMBER` reproduce deterministic input states.
Integer inputs include fixed edges followed by random states. `--fp` starts with
four exact values followed by unrestricted IEEE bit patterns, at FPCR=0.
`--fp-new` adds min/max and reductions, rounding, precision conversion,
FP-to-integer, sqrt/absolute difference, vector arithmetic and indexed fused
operations. It cycles all 16 rounding/FZ/DN combinations and includes IEEE edge
payloads before random inputs. `--word` selects one encoding without changing
its reproducible inputs.

`--memory` covers LDR/STR in all widths (8/16/32/64/128), sign-extending and
zero-extending loads, every addressing mode the emitter supports (unsigned
scaled offset, unscaled signed offset, pre-index, post-index, register offset
with UXTW/SXTW/LSL/SXTX and with and without scaling), SP as the base register,
LDP/STP/LDPSW including the 128-bit and pre/post-index forms, LDAR/STLR in all
four widths, and store-exclusives issued with no reservation held.

`--memory-seq` runs two- and three-instruction sequences, which is the only way
to reach the store-exclusive **success** path: LDXR/LDAXR/LDXP followed by
STXR/STLXR/STXP in every width, a second store-exclusive that must fail because
the first consumed the reservation, and CLREX between the pair.

## How the memory cases are kept honest

A memory differential that quietly stops addressing the shared region passes
every case while testing nothing. Four things guard against that, and all four
are assertions that fail the run, not comments:

- **Encodings are checked against a disassembler before anything runs.**
  `differential_verify_assembly.py` compares all 182 declared instructions
  against capstone. A mis-encoded word would decode to some other legal
  instruction, both engines would agree about it, and the case would report a
  pass under a name it never tested. Capstone rather than the GNU assembler
  because this also has to run on Windows, and it compares operands, not just a
  byte round trip.
- **Access accounting is two-sided.** Every memory case must touch the region;
  the cases declared `expect_no_access` - a store-exclusive with no reservation,
  which must fail without storing - must touch nothing. Either expectation being
  violated exits 2.
- **Out-of-region access exits 2** rather than being serviced, so a case whose
  address calculation drifted is reported instead of silently comparing two
  faults.
- **Store-exclusive outcomes are counted.** A `--memory-seq` run must observe
  both successes and failures. If the reservation never held, every case would
  agree on status 1 and the run would look clean while never reaching the path
  the cases exist for.

`--inject-mismatch` and `--inject-memory-mismatch` are explicit detector tests:
the expected result of each is exit 1, never a successful validation run. The
second flips one byte the emitted side wrote and must be reported as
`field=mem[...]`, which is what demonstrates the region really is compared.

Memory cases run the emitted code twice per sample, against two backends. The
flat buffer is the standalone runtime's path. The page table is the path suyu
actually drives, through `recomp_host_ptr` and the `recomp_pair_same_page` fast
path, and neither had previously been compared against anything. The first 512
samples sweep the base address one byte at a time across a page boundary, so for
every encoding - whatever its own displacement - some sample straddles the
boundary and some sample is misaligned.

Exclusive cases use a real `Dynarmic::ExclusiveMonitor` as the oracle. Supplying
hand-written `MemoryWriteExclusive` semantics instead would have compared the
recompiler against this harness's idea of the architecture rather than against
an independent implementation. The callbacks perform only the compare-and-swap
against the shared image, exactly as `Core::Memory::WriteExclusive` does for the
emulator; whether the reservation is still valid is Dynarmic's decision. Note
that leaving those callbacks at their defaults makes every store-exclusive fail,
which looks precisely like a recompiler bug and is not one.

## Results

Verified on the existing Windows MSVC build of the oracle:

| run | result |
| --- | --- |
| `--samples 256` | 7,680 comparisons, 30 cases, pass |
| `--fp --samples 256` | 1,536 comparisons, 6 cases, pass |
| `--memory --samples 512` | 96,256 comparisons, 94 cases, pass; 58,368 oracle accesses, 561 crossing a page boundary |
| `--memory-seq --samples 512` | 15,360 comparisons, 15 cases, pass; 6,144 store-exclusive successes and 1,536 failures |

The integer and scalar-FP figures are unchanged from before memory coverage was
added, which is what establishes that the move to the production `GuestContext`
did not disturb the existing cases.

Broader full-state runs still report divergences. `--fp-new` first stops at FMAX
`4e22f420`, sample 32: emitted FPSR=0, Dynarmic FPSR=1. Other targeted cases
expose FRINTN inexact, FCVTNS infinity-invalid and FZ input-denormal flag
disagreements. Arithmetic outputs match up to those stops. Dynarmic is an
independent implementation, not architectural truth: the FMAX quiet-NaN flag
discrepancy has been traced to its native MAXPS behavior, and its FPSR mapping
omits host input-denormal status. Keep every mismatch visible and resolve
against architectural evidence; do not normalize FPSR to make tests pass. The
harness does not establish full FP equivalence.

## Known gaps

- **LDNP/STNP are not implemented by `Translate()` at all.** They were left out
  of the case table because listing them aborts generation. That is a real
  decode gap, not a harness limitation.
- **LDAR/STLR are emitted as plain loads and stores**, with no barrier; the
  runtime has `recomp_barrier` but the ordered forms do not call it. A
  single-threaded differential cannot observe this, so these cases pass. It is
  recorded here because passing is not evidence of correctness for this one.
- The standalone exclusive latch is a pair of file-static globals, so it is
  shared across guest threads. The hosted path routes to the emulator's monitor
  instead and is unaffected.
- Neither engine clears a reservation when the same processor performs a plain
  store to the reserved granule, which real hardware does. One case pins that
  shared divergence in place so a future change to either side is noticed.
- The emitted runtime needs `-std=gnu11`, not strict `-std=c11`: it calls
  `nanosleep`, which strict ISO mode hides behind `_POSIX_C_SOURCE`. The
  generated code under test still builds as strict C11 with UBSan.

Not yet covered: SIMD load/store structure forms (LD1-LD4, ST1-ST4), literal
pool loads, atomic read-modify-write (LDADD, SWP, CAS), prefetch, and anything
concurrent, hardware or exhaustive.

Cache invalidation is requested **after** `Reset()`: resetting after requesting
invalidation clears Dynarmic's pending halt flag and can reuse stale code at the
fixed synthetic PC. Both runners follow the verified ordering.
