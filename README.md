# mk8-recomp

Static recompilation of Nintendo Switch AArch64 CPU code to native x86-64, using
[suyu v0.0.4](https://github.com/suyu-emu/suyu-v0.0.4)'s AOT recompiler as the
starting point and its HLE stack for everything above the CPU.

**Status.** The pipeline runs end to end: a target boots and renders with
recompiled AArch64 code executing, alongside the fallback JIT for what the
emitter does not yet translate. Nothing here is a playable port.

## What this repository contains

Our own code, build tooling and documentation. It contains no game data, no
Nintendo code, no keys, and no generated C — that material is produced locally
and gitignored. See [`docs/assumptions.md`](docs/assumptions.md).

You need your own legally dumped game and your own keys. This project will not
help you obtain either.

## Layout

```
docs/         architecture, assumptions, emulator findings
scripts/      reproducible PowerShell/Python for every step
tests/        differential validation
third_party/  suyu submodule, pinned to a commit on our fork
```

## Start here

1. [`docs/suyu-recompiler-findings.md`](docs/suyu-recompiler-findings.md) — what
   suyu v0.0.4's recompiler actually is, verified against source rather than
   against its release notes. Read this before anything else.
2. [`docs/bootstrap.md`](docs/bootstrap.md) — machine and toolchain verification
   for this workspace.
3. [`docs/architecture.md`](docs/architecture.md) — the pipeline as it exists and
   where we intend to extend it.

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

Several of those commits fix defects in suyu itself and are not
recompiler-specific — most notably that no installed update or DLC was ever
indexed in NAND, for any title.

`externals/dynarmic` inside that fork points at
[`dougchansan/dynarmic`][dyn] for the same reason: upstream committed a
forwarding header containing an absolute path into a developer's home directory,
so it built on one machine. That repository is *not* archived, so unlike the
suyu changes it could reasonably go upstream as a PR.

Between them these replace the hand-maintained patch files that used to live
under `src/patches/`. They were a substitute for a tracked tree, reconstructed by
reverse-applying edits, and a recurring source of error.

[dyn]: https://github.com/dougchansan/dynarmic/tree/mk8-recomp

[fork]: https://github.com/dougchansan/suyu-v0.0.4/tree/mk8-recomp
[prov]: https://github.com/dougchansan/suyu-v0.0.4/blob/mk8-recomp/PROVENANCE.md

## Licence

GPL-3.0-or-later, matching upstream.
