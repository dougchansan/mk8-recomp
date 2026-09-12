# mk8-recomp

Static recompilation of Nintendo Switch AArch64 CPU code to native x86-64, using
[suyu v0.0.4](https://github.com/suyu-emu/suyu-v0.0.4)'s AOT recompiler as the
starting point and its HLE stack for everything above the CPU.

The recompiler is **AArch64-only**, and the pipeline is target-agnostic: which
title is surveyed comes from `MK8R_TARGET`, and nothing is hardcoded to one.
The name is historical — the project began aimed at Mario Kart 8 Deluxe, which
turned out to be an AArch32 title and therefore outside what the emitter can
translate. See [`docs/aarch32.md`](docs/aarch32.md).

**Status.** The pipeline runs end to end: a target boots and renders with
recompiled AArch64 code executing, alongside the fallback JIT for what the
emitter does not yet translate. It is currently slower than the JIT it augments.
Nothing here is a playable port, and it is not a way to play anything you do not
already own.

## What this repository contains

Our own code, build tooling and documentation. It contains no game data, no
Nintendo code, no keys, no firmware, and no generated C — that material is
produced locally and gitignored.

You need your own legally dumped game and your own keys. This project will not
help you obtain either, and will not answer requests to.

The rules the project holds itself to, including why recompiler output is never
published, are in [`LEGAL.md`](LEGAL.md). Working assumptions and their
confidence levels are in [`docs/assumptions.md`](docs/assumptions.md).

## Layout

```
docs/         architecture, assumptions, emulator findings
scripts/      reproducible PowerShell/Python for every step
tests/        differential validation
third_party/  suyu submodule, pinned to a commit on our fork
```

## Building

One command per platform, from nothing to built. Each installs the toolchain,
clones with submodules and builds. Neither fetches a game, keys, or firmware.

**Linux** — any distro with `apt`, `dnf`/`yum`, `pacman`, `zypper`, `apk`,
`xbps` or `eopkg`. Package names are per-distro; unknown ones are reported and
skipped rather than aborting the install:

```bash
curl -fsSL https://raw.githubusercontent.com/dougchansan/mk8-recomp/main/scripts/setup-linux.sh | bash
```

**Windows** — an Administrator PowerShell, because the MSVC C++ toolset needs
one. Everything comes from `winget`; an existing Visual Studio with the C++
workload is reused rather than duplicated:

```powershell
irm https://raw.githubusercontent.com/dougchansan/mk8-recomp/main/scripts/setup-windows.ps1 | iex
```

Both clone into `./mk8-recomp` (`MK8R_ROOT` overrides), or build in place if
you run them from inside a clone you already have. To stop partway:
`--deps-only` / `--no-build` on Linux, `$env:MK8R_SETUP='deps'` or `'nobuild'`
on Windows. `./scripts/setup-linux.sh --dry-run` lists the packages it would
install without touching the system.

<details>
<summary>Doing it by hand</summary>

The setup scripts are a wrapper around three steps. Clone with submodules, then
on Linux install Qt 6, GCC, nasm, glslang and the codec/Boost development
packages and run the build script — it needs CMake 3.31 or newer, and tells you
where to put one if yours is older:

```bash
git clone --recursive https://github.com/dougchansan/mk8-recomp && cd mk8-recomp && ./scripts/build-suyu.sh
```

On Windows, `bootstrap.ps1` downloads the pinned glslang and Qt into
`local/tools`. It verifies a dump when `MK8R_ROM` points at one and skips that
step with `-SkipGame`:

```powershell
git clone --recursive https://github.com/dougchansan/mk8-recomp; cd mk8-recomp; .\scripts\bootstrap.ps1 -SkipGame; .\scripts\build-suyu.ps1
```

Both build scripts take `--clean` / `-Clean` to start from scratch and
`--configure` / `-Configure` to force a reconfigure. If you cloned without
`--recursive`, run `git submodule update --init --recursive` first; the scripts
stop with that instruction rather than failing inside cmake.

</details>

Building a generated module into a loadable image is a separate step, once you
have exported one locally:

```bash
./scripts/build-recomp.sh main       # Windows: .\scripts\build-recomp.ps1 -Module main
```

Paths come from the environment — `MK8R_ROOT`, `MK8R_ROM`, `MK8R_TARGET`,
`MK8R_PACKAGE` — and default to the repository root where they can. Nothing is
hardcoded to a particular machine.

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
