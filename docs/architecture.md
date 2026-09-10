# Architecture

## The pipeline as it actually exists in suyu v0.0.4

```
the target XCI
  |
  | FileSys::XCI -> secure partition NSP -> Program NCA -> ExeFS
  | (needs prod.keys; does NOT need firmware)
  v
per-NSO: NSO0 header -> LZ4-decompress segments -> raw .text / .rodata / .data
  |
  | suyu::recomp::EmitProject   (src/core/recompiler/arm64_to_c.h:1806)
  |   DiscoverBlocks -> Translate (direct emit, no IR) -> N x .c units
  v
generated C + CMakeLists + data blobs
  |
  | host C compiler (MSVC / clang) at -O1
  v
recompiled_<module>.dll   (or static lib linked into suyu-cmd-static)
  |
  | LoadLibrary + GetProcAddress("recomp_image_lookup", "recomp_image_set_base")
  | Core::SetRecompLookup / SetRecompBaseSetter
  v
Core::ArmRecomp : ArmInterface   (src/core/arm/recomp/arm_recomp.cpp)
  |
  +-- block(&ctx) executes -> pending_svc -> HaltReason::SupervisorCall
  |                                             |
  +-- lookup miss OR halted==UNHANDLED          |
  |         -> marshal ThreadContext            |
  |         -> ArmDynarmic64 (JIT fallback)     |
  |                                             v
  +-- every load/store -> RecompHostMem -> Core::Memory
                                                |
                                    HLE kernel / services / GPU / audio / input
                                                |
                                            suyu core
```

This is the architecture the original brief proposed, and upstream already
implements it. We keep it. The `ArmInterface` seam is the right one: everything
above the CPU reaches the guest only through it, so a recompiled game gets the
real HLE stack rather than a stub.

## Phase 7 questions, answered against source

| # | Question | Answer |
|---|---|---|
| 1 | Is every AArch64 instruction translated? | **No.** Integer core is broad; NEON is a thin slice; exclusives and LSE atomics are deliberately unhandled. See findings doc for the full absent list. |
| 2 | Are basic blocks emitted independently? | Yes. One `void blk_<va>(GuestContext*)` per block, batched 20000 per `.c` unit. |
| 3 | Are complete functions recovered? | **No.** `Block::is_entry` exists but is set only for block 0 and never read. There is no function-boundary concept at all. |
| 4 | How are BL calls represented? | `c->x[30] = base+next; c->pc = base+target; return;` — a store and a return to the dispatch loop, not a host call. |
| 5 | BR / BLR / RET? | Same shape: store `c->pc` from the register (or x30) and return. Resolved only at runtime by exact-match lookup. |
| 6 | Jump tables? | **Not handled at all.** No table recognition anywhere. Every `switch` target is a lookup miss into the JIT. |
| 7 | How are guest pointers represented? | As plain `uint64_t` guest virtual addresses. No host-pointer rewriting. |
| 8 | Are loads/stores direct mapped? | No. Every access is `memload`/`memstore`, an indirect call through `RecompHostMem` into `Core::Memory`. Standalone only, it degrades to a flat array. |
| 9 | Does memory stay in a guest VA space? | Yes — both engines share `Core::Memory`, so the AOT and JIT paths see one identical address space. This is the design's strongest property. |
| 10 | MMIO? | Not special-cased in the recompiler. It goes through `Core::Memory` like everything else and is handled by the existing HLE. |
| 11 | SVCs? | The emitted site writes `pending_svc` and returns; `ArmRecomp` converts that to `HaltReason::SupervisorCall`, identical to the dynarmic backend. The real kernel handles it. |
| 12 | NEON translation? | Partial — see findings. Full 128-bit `vreg[32][2]` state exists, so the *state* is right; the *coverage* is not. |
| 13 | Unsupported instructions? | Hosted: park PC, set `halted = 2`, hand to JIT. Standalone: print and **step over**, corrupting state. |
| 14 | Does it fall back to Dynarmic? | Yes, a real lazily-constructed `ArmDynarmic64`. |
| 15 | Can static blocks call JIT blocks? | Not directly — control returns to `RunThread`, which marshals into the JIT. |
| 16 | Can JIT return to static code? | Yes, on the JIT's next halt: context is marshalled back and the loop resumes static dispatch. |
| 17 | Are code pages assumed immutable? | **Yes.** `ClearInstructionCache` and `InvalidateCacheRange` are empty. |
| 18 | Self-modifying / JIT-generated guest code? | Unsupported by construction. Guest-generated code will only ever run on the dynarmic fallback. |

## Known correctness defects in the shipped design

These are bugs, not gaps, and they are the first things to fix:

1. **FPCR/FPSR do not exist.** `GuestContext` has no field for them and
   `GetContext`/`SetContext` never marshal them. Guest code that sets a rounding
   mode and then crosses an engine boundary silently loses it. Fix is
   append-only — the pinned offsets survive adding fields after `tpidrro_el0`.
2. **Standalone `recomp_load_segments`** loads `.text`, `.rodata` and `.data` all
   at offset 0, overwriting each other. Standalone-only, but it means the
   standalone path has never been exercised on anything real.
3. **IFUNC / IRELATIVE relocations are trapped, not resolved.**
4. **`StepThread` has no fallback** — debugger single-step over uncovered code
   returns `PrefetchAbort`.
5. **Coverage is unmeasurable.** `RecompileStats` counts blocks, instructions and
   terminators, but not instructions that fell through to `recomp_unhandled`.

## Where we intend to extend

In rough dependency order — not a schedule, a dependency graph:

- **Measurement first.** An `unhandled` counter in `RecompileStats`, a per-opcode
  histogram of what fell through, and runtime counters for fallback entries and
  their originating PCs. Without these, every coverage claim is a guess. The
  per-miss diagnostic machinery at `arm_recomp.cpp:682-706` already exists but is
  capped at 8 reports; that cap becomes a knob.
- **FPCR/FPSR** in the context and in both marshalling directions.
- **NEON coverage** in `Translate`, prioritised by the histogram rather than by
  what looks important. Expect `LD1..LD4`/`ST1..ST4` and `TBL` to dominate.
- **Exclusives** — LDXR/STXR need a real exclusive monitor shared with dynarmic,
  which `DynarmicExclusiveMonitor` already provides to the fallback.
- **Jump-table discovery**, so `switch` statements stop being lookup misses.
- **Memory fast path** — the per-access indirect call is the structural
  performance ceiling. Not touched until correctness is demonstrated.

## Milestones

| | |
|---|---|
| M0 | Unmodified Suyu builds and launches the target |
| M1 | Export recognises the target and catalogs its modules |
| M2 | Recompiled C generated for at least one module |
| M3 | Generated C compiles to a Windows x86-64 binary |
| M4 | One known function/block executes through the AOT path and returns correctly |
| M5 | Boot runs partly through AOT with deterministic fallback |
| M6 | Title screen, with measured static coverage |
| M7 | Enter a race |
| M8 | Complete a race correctly |
| M9 | Profile and reduce JIT/emulation overhead |
| M10 | Repeatable semi-native Windows launcher |
