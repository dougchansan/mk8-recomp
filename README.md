# mk8-recomp

Static recompilation of Nintendo Switch AArch64 CPU code to native Windows
x86-64, using [suyu v0.0.4](https://github.com/suyu-emu/suyu-v0.0.4)'s AOT
recompiler as the starting point and its HLE stack for everything above the CPU.

**Targets: the target title** (`targets/target/`) and
**the target title** (`targets/target/`).

the target title was written off early as untargetable: the cartridge ships a **32-bit
AArch32** build and suyu's recompiler is AArch64-only. That was wrong, and the
reason was a bug in our own ISA probe — it read only the *base* NPDM, so an
update-only NSP came back "no exefs" and the base's ARM32 flag stood in for the
whole title. **Update v4.0.0 is AArch64**, and it is the build the emulator
actually runs. See `targets/target/README.md`.

**Status.** Both targets boot and render at 60 FPS with recompiled AArch64 code
executing:

| | static coverage | hybrid run |
|---|---:|---|
| the target title | 99.79% of 4,832,510 instructions | renders, 60 FPS, 2.4bn blocks native |
| the target title | — | boots; renders black under full recompilation ([#27](https://github.com/dougchansan/mk8-recomp/issues/27)) |

Neither is a playable port. Both still lean on the fallback JIT for the
instructions the emitter does not yet translate, which is correct but slow, and
the target has a genuine translation defect that the target does not.

## What this repository contains

Our own code, build tooling, manifests, and documentation. It contains
no game data, no Nintendo code, no keys, and no generated C — that material is
produced locally and gitignored. See [`docs/assumptions.md`](docs/assumptions.md).

You need your own legally dumped game and your own keys. This project will not
help you obtain either.

## Layout

```
docs/         findings, architecture, assumptions, progress, library survey
scripts/      reproducible PowerShell/Python for every step
src/          our additions — runtime, bridge, instrumentation
manifests/    machine-readable catalogs (hashes and offsets, no content)
targets/      per-title working directories
tests/        differential validation
third_party/  suyu submodule, pinned to a commit on our fork
```

## Start here

1. [`docs/suyu-recompiler-findings.md`](docs/suyu-recompiler-findings.md) — what
   suyu v0.0.4's recompiler actually is, verified against source rather than
   against its release notes. Read this before anything else.
2. [`docs/bootstrap.md`](docs/bootstrap.md) — machine, toolchain, and input
   verification for this workspace.
3. [`docs/architecture.md`](docs/architecture.md) — the pipeline as it exists and
   where we intend to extend it.
4. [`docs/library-isa.md`](docs/library-isa.md) — which titles are AArch64 and
   therefore targetable at all.

Progress is tracked in [issues](https://github.com/dougchansan/mk8-recomp/issues),
organised by milestone M0–M10.

## The emulator

`third_party/suyu` is a submodule of [`dougchansan/suyu-v0.0.4`][fork], branch
`mk8-recomp`, pinned to an exact commit.

suyu was archived upstream, so this is a continuation rather than a temporary
divergence: the name and numbering carry on, `0.0.4` becomes `0.0.5`. The base
is `suyu-emu/suyu-v0.0.4` at `d1d09321d7ab84252291e05b3efbc8a8dfa57481`,
GPL-3.0-or-later. See [`PROVENANCE.md`][prov] in the fork for the base, the
licensing, and the significant changes.

The change set is:

```
git -C third_party/suyu diff d1d09321d7ab84252291e05b3efbc8a8dfa57481..mk8-recomp
```

Five of those commits fix defects in suyu itself and are not
recompiler-specific — most notably that *no* installed update or DLC was ever
indexed in NAND, for any title.

This replaces the hand-maintained patch files that used to live under
`src/patches/`. They were a substitute for a tracked tree, reconstructed by
reverse-applying edits, and a recurring source of error.

[fork]: https://github.com/dougchansan/suyu-v0.0.4/tree/mk8-recomp
[prov]: https://github.com/dougchansan/suyu-v0.0.4/blob/mk8-recomp/PROVENANCE.md

## Licence

GPL-3.0-or-later, matching upstream.
