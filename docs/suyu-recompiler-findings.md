# Suyu v0.0.4 Native Recompiler — Verified Findings

Everything below was read out of the pinned source at commit
`d1d09321d7ab84252291e05b3efbc8a8dfa57481`. Line numbers are that commit's.

## First: the decoy

`tools/static_recompiler/` looks like the recompiler and is not. It is a
self-contained 425-line `suyu_recomp.cpp` plus `prove.cmd` that assembles a
**four-instruction** test program (`movz x0,#5; movz x1,#7; add x2,x0,x1; svc #0`),
translates it to C, compiles it, and prints the result. Its `GuestContext` has no
vector registers at all. It has no XCI/NCA/NSO loader and no connection to Suyu.
The directory also has committed build artifacts (`suyu_recomp.exe`, `.obj`,
`out/recompiled.exe`). **Ignore it.**

The real pipeline is three files:

| Role | File | Lines |
|---|---|---|
| Emitter (AArch64 to C) | `src/core/recompiler/arm64_to_c.h` | 2760 |
| Export driver + packager | `src/suyu/game_export.cpp` | 2982 |
| Runtime CPU backend | `src/core/arm/recomp/arm_recomp.cpp` | 880 |

## Recompiler entry point

- **Emitter:** `suyu::recomp::EmitProject` — `src/core/recompiler/arm64_to_c.h:1806`
- **Driver:** `GameExportDialog::RunAotPrecompile` — `src/suyu/game_export.cpp:1454-1663`
- **Executable:** the **Qt GUI only** (`suyu.exe`). There is no CLI exporter.
  - File > `Export Game...` — `src/suyu/main.ui:571-575`, wired `main.cpp:1751` to `GMainWindow::OnExportGame` (`main.cpp:5981`)
  - Game-list context menu > `Recompile for PC...` — `src/suyu/game_list.cpp:695`
  - Dialog title: `Export Game — AOT Static Recompilation` (`game_export.cpp:274`)
  - An automation RPC exists: `action == "aot_test_export"` (`main.cpp:5529-5558`) calls
    `TriggerExportForTesting(rom_path, output_dir, format_index)` with
    `format` in {`"source"` -> 0, `"build"` -> 1}. **This is the scriptable hook.**

`suyu-cmd` is the *runtime* for an exported game, not an exporter. Its complete
option set (`src/suyu_cmd/suyu.cpp:319-335`) is `-c -f -h -g -l -m -p -u -v`.
There is no export flag and no recomp flag; native mode is selected by the
`SUYU_CMD_STATIC_RECOMP` compile define or by finding `recompiled_*.dll` on disk.

## Input format

XCI **directly**, plus NSP / NCA / NRO / deconstructed directory
(`game_export.cpp:483`). Path to bytes (`ExtractExeFsFromRom`, `game_export.cpp:1007-1097`):

```
.xci -> FileSys::XCI -> GetSecurePartitionNSP() -> GetNCA(program TID, ContentRecordType::Program)
     -> NCA::GetExeFS() -> per-file AnalyzeNsoFile (game_export.cpp:927-1004)
     -> NSO0 magic check -> LZ4-decompress segments 0..2 -> raw .text/.rodata/.data
```

- Title does **not** need to be installed — it opens the ROM through a private `RealVfsFilesystem`.
- **prod.keys IS required at export time** (NCA decryption via `KeyManager`).
- **Firmware is NOT required.**
- The generated package needs neither at runtime — romfs is extracted into it
  (`game_export.cpp:2255-2260`).

## Export pipeline

**Block discovery** (`DiscoverBlocks`, `arm64_to_c.h:155-209`) seeds from five roots:
offset 0; the module entry PC; exported dynsym addresses passed as `extra_roots`;
`CollectAdrpAddTargets` (`:117-153`, a 32-entry ADRP-shadow scan that recovers
thread-entry callbacks handed to `svcCreateThread`); and fallthrough +
direct-branch targets.

**What discovery misses — this matters:**
- **Jump tables entirely.** No `ADRP+ADD+LDRSW` table recognition, no `BR Xn`
  target inference. Every `switch` statement's targets go unmarked.
- Vtables / function-pointer tables in `.rodata` — only `.text` is scanned (`:119-120`).
- `ADRP+LDR` (GOT-style) pairs — only `ADD` is tracked.
- There is no function-boundary concept. `Block::is_entry` (`:58`) is set only for
  block 0 (`:204`) and never read.

**IR:** none. `Translate` (`arm64_to_c.h:272-1772`) is a direct
instruction-to-C-string emitter — a mask/match chain writing straight into a
`std::string`. No lifting, no SSA, no optimization pass.

**Emission** (`EmitProject`): writes `recomp_runtime.{h,c}`, `main.c`,
`recomp_export.c`, `CMakeLists.txt`, `data/{text,rodata,data}.bin`, and N units
`src/recompiled_<mod>_<N>.c` at `kBlocksPerUnit = 20000` blocks each — split
because "a whole module is over a gigabyte of C" (`:1836-1839`). Streamed through
an 8 MiB flush buffer. Generated units compile at `-O1` / `/O1` (`:2153-2161`).

**Dispatch:** two-level binary search (`:2003-2013`) — outer over per-unit high
watermarks `_seg_hi[]`, inner over that unit's entry array, requiring an **exact**
PC match.

**Indirect branches:** not resolved statically at all. `BR`/`BLR`/`RET` are
emitted as *stores to `c->pc` followed by `return`* (`:479-484`) — there is no
host call stack and no direct C call between blocks. Every transfer round-trips
through the dispatch loop. A `BR` to an address discovery never marked returns
NULL from lookup and falls to the JIT.

**Relocations:** applied at runtime by `ArmRecomp` (`ApplyAllRelocations`,
`arm_recomp.cpp:444`), not baked in — the static pass emits module-relative
addresses and the loader supplies the real base via `SetRecompBaseSetter`.
Unresolved GOT/PLT symbols become a `kUnresolvedImportTrap` (`:98`). **IFUNCs are
broken** — `arm_recomp.cpp:358-361` logs `"IRELATIVE relocation ... not invoked
(resolver call unsupported); trapped instead"`.

## Runtime

`GuestContext` (`arm64_to_c.h:2309-2354`), offsets pinned on both sides by
negative-array-size typedefs (`:2366-2371`) and `static_assert` (`arm_recomp.cpp:68-73`):

| Field | Notes |
|---|---|
| `x[32]` | `x[31]` **is** SP |
| `pc` | offset 256 |
| `n,z,c,v` | NZCV as four `uint8_t` |
| `pending_svc` | offset 304, `~0ULL` = none |
| `vreg[32][2]` | offset 312 — full 128-bit Q registers |
| `tpidr_el0` / `tpidrro_el0` | offsets 824 / 840 |
| `host_mem` | offset 832 — pointer to host memory vtable |
| `save_dir`, `heap_*`, `ipc_cmd[64]`, `save_handles[16]` | standalone-only |

- **No FPCR / FPSR field exists.** No FP rounding mode, no denormal control, no
  exception flags — and they are never marshalled across an engine transition
  (`GetContext`/`SetContext`, `arm_recomp.cpp:793-836`). This is a silent
  correctness bug, not just a gap.
- **Memory:** every load/store goes through `memload`/`memstore`
  (`arm64_to_c.h:2439-2446`), which check `c->host_mem` first. Hosted in Suyu,
  that is an **indirect call plus a size switch into `Core::Memory` per access**
  (`arm_recomp.cpp:129-147`). Standalone it falls back to a flat 256 MB `calloc`.
  Both engines see the same address space — the one thing this design gets
  unambiguously right, and also the largest performance liability.
- **Atomics:** LDXR/STXR/LDAXR/STLXR **deliberately** emit `recomp_unhandled`
  (`arm64_to_c.h:754-761`); the whole ARMv8.1 LSE group (CAS/SWP/LDADD/...) has no
  handler. Every lock and refcount round-trips to the JIT.
- **SVC:** the emitted site writes `pending_svc` and returns; `recomp_svc` is a
  no-op under `SUYU_HOSTED_RECOMP` (`:2587`). `ArmRecomp::RunThread` picks it up
  and returns `HaltReason::SupervisorCall` exactly as the dynarmic backend does
  (`arm_recomp.cpp:754-760`), so the real HLE kernel handles it.
  The standalone `recomp_svc` (`:2589-2686`) is a toy mini-HLE — `CreateThread`
  returns handle 1 and creates no thread, `QueryMemory` writes nothing,
  `SendSyncRequest` is stub-success. It cannot boot a real title.
- **Self-modifying code:** unsupported by construction —
  `ClearInstructionCache` / `InvalidateCacheRange` are empty
  (`arm_recomp.cpp:783-791`).

## Instruction coverage

Handled: the integer core (MOV*, ADD/SUB all forms, logical, ADR/ADRP, LDR/STR
all addressing modes, STP/LDP, bitfield, CCMP/CSEL family, MADD/MSUB/mulh,
UDIV/SDIV, shifts, RBIT/REV/CLZ), LDAR/STLR, MRS/MSR for **TPIDR_EL0 and
TPIDRRO_EL0 only**, and a thin FP/NEON slice.

**NEON/FP present:** scalar FP single+double only (half-precision falls through),
FCMxx vs zero, vector SCVTF/UCVTF, FMUL by indexed element, DUP/INS/SMOV/UMOV,
MOVI/MVNI **only for `cmode == 0xE`**, 3-same logical + integer ADD/SUB/MUL,
vector FMLA/FMLS/FADD/FSUB/FMUL/FDIV, and SIMD LDP/STP/LDR/STR/LDUR/STUR.

**NEON/FP absent — falls through to `recomp_unhandled`:**
`TBL/TBX`, `ZIP/UZP/TRN`, `EXT`, vector `REV16/32/64`,
**`LD1/LD2/LD3/LD4` and `ST1..ST4` (all structure load/stores)**,
`ADDV/UADDLV/SADDLV`, `SMAX/UMIN/...`, all shifts (`SSHL/USHL/SHL/SSHR/USHR/SLI/SRI`),
all saturating arithmetic, `SABD/UABD`, `SMULL/UMULL/SQDMULL`,
`FRECPE/FRSQRTE`, `FRINTx`, `FCVTL/FCVTN`, vector `FCVTZS`, `FMLA` by element,
integer vector compares, `CNT`, vector `NOT/MVN`, `ADDP/FADDP`.

Also absent: PAC (`RETAA` becomes unhandled), `BRK/HLT/HVC`, `CRC32*`,
`PRFM` unsigned-offset, and every system register except the two TPIDRs.

**Unsupported-instruction behaviour** (`arm64_to_c.h:1758-1771`, `:2723-2732`):
under `SUYU_HOSTED_RECOMP` it parks `pc` on the failing instruction and sets
`halted = RECOMP_HALT_UNHANDLED (2)`, and `ArmRecomp` re-runs it on dynarmic.
Standalone it prints to stderr and **steps over the instruction**, leaving state wrong.

`RecompileStats` (`:1790-1794`) has only `blocks`, `instructions`,
`translated_terminators`. **There is no unhandled counter** — the exporter cannot
report its own coverage.

## Fallback

`ArmRecomp::RunThread` (`arm_recomp.cpp:565-764`) runs blocks in a loop and drops
to a **full, lazily-constructed `ArmDynarmic64`** on either a lookup miss
(`:707-729`) or `halted == kHaltUnhandled` (`:739-752`). Architecturally the
fallback is complete. The hand-off is not free and not lossless:

- Every crossing marshals 32 GPRs + 32 x 128-bit vectors through
  `Kernel::Svc::ThreadContext` and reloads the JIT context (`RunFallback`, `:535-563`).
- **FPCR/FPSR are dropped in both directions.**
- `StepThread` (`:766-781`) has **no fallback at all** — it returns
  `PrefetchAbort` on a miss.

## Suyu bridge

Everything above the CPU is untouched and still HLE: kernel, filesystem, all
services, GPU/Vulkan, shader translation, audio, input, networking, timers. The
whole point of `ArmRecomp` implementing `ArmInterface` is that a recompiled game
gets the real stack instead of the generated runtime's stub SVC handler.

## Standalone application

`PackageNativeExport` (`game_export.cpp:2218-2457`). Windows output:

```
<output>/<GameName>/
  <GameName>.exe          # static_launcher.exe, else generic suyu-cmd.exe
  launch.bat, README_NATIVE_EXPORT.txt
  exefs/  exefs/romfs.bin  mods/  user/
  *.dll                    # ffmpeg, dxcompiler, openssl
  aot_cache/               # kept only when no static launcher was produced
```

- **Qt is required to produce an export.** It is not required to run one —
  the launcher is `suyu-cmd`-derived.
- **Build mode relinks Suyu itself**: it reconfigures the Suyu build tree with
  `-DSUYU_CMD_RECOMP_DIR=<recomp_root>` and builds target `suyu-cmd-static`
  (`game_export.cpp:1964-1990`). So a Build export needs a Suyu build tree
  next to the running `suyu.exe`, or it degrades to the DLL layout (`:1884-1887`).
- It shells out to `vcvars64.bat` at a **hardcoded VS2022 Community path**
  (`game_export.cpp:1929-1954`) — a portability trap on this machine, which has
  VS2026 Community and VS2022 Build Tools but no VS2022 Community.
- Linux/macOS packaging bundles **no runtime binary** and produces nothing runnable
  (`game_export.cpp:2460-2515`).

## Limitations / defects found

| Where | What |
|---|---|
| `game_export.cpp:2817`, `:1424` | `"Ballistic export is not wired yet"` — the Ballistic backend combo item is inert; `effective_backend_name` is hardcoded to `"dynarmic"` |
| `game_export.cpp:624-676` | `AnalyzeArm64BasicBlocks` accepts a `full_scan` parameter and **never reads it** — the UI checkbox at `:340-343` is a no-op |
| `game_export.cpp:1475-1478` | canonical module-name list declared, never used for filtering |
| `game_export.h:56-58` | documents a "known hang past ~15%"; `:1185-1198` claims it was diagnosed and gated behind `SUYU_AOT_DUMP_BLOCKS`, header never updated |
| `game_export.cpp` | export runs **on the GUI thread**, pumping `processEvents`; **no cancellation** — `closeEvent`/`reject()` are refused mid-export |
| `arm_recomp.cpp:358-361` | IFUNC / IRELATIVE relocations unsupported, trapped |
| `arm64_to_c.h:2563-2577` | standalone `recomp_load_segments` loads .text/.rodata/.data all at offset 0, **overwriting each other**; comment admits "the real offsets come from the blockmap" |
| `arm64_to_c.h:1790` | no unhandled-instruction counter — coverage is unmeasurable as shipped |

## Conclusion

**Prototype, partially stubbed — but a well-engineered one.**

The release's claims check out in shape and not in scope:

1. *"Game CPU-code export to recompiled C"* — **true.** Real XCI to NCA to ExeFS to
   NSO to LZ4 to AArch64 to C pipeline.
2. *"Ahead-of-time static AArch64 to x86-64 recompilation"* — **true but qualified.**
   It emits C, not x86-64; a host compiler does the codegen. Coverage is a fraction
   of AArch64.
3. *"Standalone semi-native application output"* — **true on Windows only**, and the
   single-file form requires relinking Suyu. Linux/macOS packages are non-runnable.
4. *"Continued use of Suyu's HLE backend"* — **true and well done.** `ArmRecomp`
   implementing `ArmInterface` is the correct architecture and we should keep it.

The comments in these files document real bugs someone actually hit and fixed
(LDUR sign-extension turning loads into wild stores, `DecodeBitMasks` rotation,
TST being dropped, ABS64 vtable relocations). This is not vapourware.

**But for the target title specifically, the honest expectation is that the AOT
path will be slower than plain dynarmic**, and here is why:

- The absent NEON set — `LD1..LD4`/`ST1..ST4`, `TBL`, `EXT`, `ZIP/UZP/TRN`,
  horizontal reductions, `FRECPE/FRSQRTE`, `FMLA` by element, saturating
  arithmetic — is precisely what a game engine's math, physics, skinning and
  animation code runs every frame.
- Exclusives are unconditionally punted, so every lock and refcount round-trips too.
- No jump-table discovery, so every `switch` is a lookup miss.
- Each of those costs a full 32-GPR + 32-Q marshal into dynarmic and back.
- Every guest memory access is an indirect call; dynarmic inlines these.

So the AOT path handles prologues, integer glue and control flow, while the actual
hot work executes on dynarmic **plus two context marshals per entry**. The author
knew: `arm64_to_c.h:2705` — *"every remaining gap costs speed rather than correctness."*
For the target the gaps are the hot path.

**This does not make the project unviable.** It makes the first real engineering
task obvious: measure coverage, then close the NEON and exclusives gaps in
`Translate`, and add jump-table discovery. That is additive work against a sound
architecture, which is a much better starting position than a blank page.
