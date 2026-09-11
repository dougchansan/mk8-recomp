# Assumptions and Ground Rules

## Legal / hygiene

- The game dump at
  the path given by `MK8R_ROM`
  is **read-only**. It is never moved, renamed, patched, truncated, or copied into
  this repository. Its SHA-256 is recorded in `docs/bootstrap.md` and re-verified
  before any run that touches it.
- The sibling `(Certificate).bin` and `(Initial Data).bin` are likewise read-only
  and are not used unless Suyu's loader demands them. As of this writing it does
  not — `ExtractExeFsFromRom` reads the XCI's secure partition directly.
- Keys and firmware are **user-supplied only**. Nothing is downloaded, searched
  for online, or derived. If a prerequisite is missing, we stop and report exactly
  what is missing rather than sourcing it.
- Generated C is a mechanical translation of copyrighted machine code. It stays
  local and gitignored. Nothing that substantially reproduces Nintendo machine
  code is committed or published.
- What we do publish: our source, our patches, build tooling, manifests, hashes
  and identifiers, function metadata, documentation, and original compatibility
  code.

## Technical assumptions, and how confident we are

| Assumption | Confidence | Basis |
|---|---|---|
| Suyu's AOT path preserves correctness by falling back to dynarmic | high | `arm_recomp.cpp:707-752`; the fallback is a real `ArmDynarmic64` |
| The AOT path will initially be *slower* than plain dynarmic on the target | high | NEON gaps are the hot path; see findings doc |
| Export needs prod.keys but not firmware | high | `KeyManager` in NCA ctor; no firmware reference in the export path |
| The 15.0.1-era prod.keys on this machine can decrypt this XCI | **unverified** | key generation vs. cartridge master key not yet checked |
| The target's ExeFS is the usual rtld / main / subsdk* / sdk set | **unverified** | to be detected, not assumed — Phase 6 |
| `aot_test_export` RPC gives us a scriptable export without GUI clicking | medium | `main.cpp:5529-5558`; the RPC transport itself not yet exercised |
| Build-mode export will fail on this machine as shipped | medium-high | it shells out to a hardcoded VS2022 *Community* vcvars path; this machine has VS2026 Community + VS2022 *Build Tools* |

Anything marked unverified is a task, not a belief. See the issue tracker.

## Things we are deliberately not doing yet

Not rewriting Vulkan rendering, the kernel, filesystem HLE, services, audio, or
NVN. Not decompiling the target, naming functions, or reversing classes. Not making
gameplay mods. Not optimising before correctness is demonstrated. Not removing
the dynarmic fallback.

The objective is **static execution**, not decompilation.

## Correctness over coverage

A higher static-recompilation percentage that diverges from baseline is a
regression, not progress. Differential validation (Phase 10) stops at the
*earliest* divergence and reports it rather than booting further.
