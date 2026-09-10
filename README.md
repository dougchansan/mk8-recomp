# mk8-recomp

Static recompilation of the target title's AArch64 CPU code to native Windows
x86-64, using [suyu v0.0.4](https://github.com/suyu-emu/suyu-v0.0.4)'s AOT
recompiler as the starting point and its HLE stack for everything above the CPU.

**Status: bootstrap.** Nothing here runs a game yet.

## What this repository contains

Our own code, patches, build tooling, manifests, and documentation. It contains
no game data, no Nintendo code, no keys, and no generated C — that material is
produced locally and gitignored. See [`docs/assumptions.md`](docs/assumptions.md).

You need your own legally dumped game and your own keys. This project will not
help you obtain either.

## Layout

```
docs/         findings, architecture, assumptions, progress
scripts/      reproducible PowerShell for every step
src/          our additions — runtime, bridge, patches, instrumentation
manifests/    machine-readable module catalogs (hashes and offsets, no content)
tests/        differential validation
third_party/  pinned suyu checkout (gitignored, see scripts/bootstrap.ps1)
```

## Start here

1. [`docs/suyu-recompiler-findings.md`](docs/suyu-recompiler-findings.md) — what
   suyu v0.0.4's recompiler actually is, verified against source rather than
   against its release notes. Read this before anything else.
2. [`docs/bootstrap.md`](docs/bootstrap.md) — machine, toolchain, and input
   verification for this workspace.
3. [`docs/architecture.md`](docs/architecture.md) — the pipeline as it exists and
   where we intend to extend it.

Progress is tracked in [issues](https://github.com/dougchansan/mk8-recomp/issues),
organised by milestone M0–M10.

## Upstream

Pinned at `suyu-emu/suyu-v0.0.4` commit `d1d09321d7ab84252291e05b3efbc8a8dfa57481`
(GPL-3.0-or-later). Upstream is kept unmodified under `third_party/suyu`; our
changes live in `src/` or as reviewable patches under `src/patches/`.

## Licence

GPL-3.0-or-later, matching upstream.
