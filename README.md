# mk8-recomp

[**suyu v0.0.11**](https://github.com/dougchansan/suyu-v0.0.4/releases/tag/v0.0.11)

Ahead-of-time recompilation of Nintendo Switch AArch64 CPU code to native x86-64 (and AArch64 hosts), using suyu's HLE stack for everything above the CPU. The recompiler and the export pipeline live in the [suyu fork](#the-emulator); this repository holds the scripts, tests and notes used to build, validate and measure them.

| Mode | When to use it |
|---|---|
| suyu Dynarmic JIT (Baseline) | The default, and the most compatible. Runs Mario Kart 8 Deluxe at 60 fps. |
| suyu Hybrid JIT + AOT (experimental) | Pre-compiled code with JIT fallback for anything it doesn't cover. Performance varies by game. |
| suyu static AOT (Experimental) | Pre-compiled code only, no JIT fallback. Now close to JIT speed on MK8D, but tested on that one game. |

[Release notes](https://github.com/dougchansan/suyu-v0.0.4/blob/v0.0.11/docs/releases/v0.0.11.md). **Re-export every static or Hybrid build made with v0.0.10 or earlier.** suyu v0.0.11 refuses ABI 4 images on purpose, and updating the host alone cannot update generated code.

## What changed in v0.0.11

- **ABI 6 (new default)** adds three negotiated features on top of ABI 5: fast guest memory (FM1), a check-once-per-code-change guard (GG1) and exact native floating point (FPX1). An image reports its feature bits and a host refuses any it doesn't implement. `SUYU_AOT_FASTMEM=0` exports ABI 5, byte-identical to before; `SUYU_AOT_GUARD_GEN=0`, `SUYU_AOT_FPX=0` and the `SUYU_RECOMP_*` runtime switches turn single features off.
- **ABI 5** validates image revision, instruction coverage and memory guards more strictly, applies relocations to rtld only (as Dynarmic does), and fixes several FP and exclusive-state exactness bugs. ABI 4 images are rejected at build time and at load.
- **Windows exports build with clang-cl** when it is installed (`winget install LLVM.LLVM`), which alone is about 45% faster than MSVC and halves export time. `SUYU_RECOMP_COMPILER=msvc` forces the old path.
- **Export Game** in suyu is now usable end to end: Build packages with settings, shader cache, controllers, keys/firmware lookup, Steam shortcuts and Discord presence. Hybrid runs record uncovered code in `recomp_gaps.json`, which a later export uses as extra discovery roots.
- Apple Silicon Macs work again, and there is a new RetroArch (libretro) core.

## Performance

Mario Kart 8 Deluxe v4.0.0, game fps during a race (capped at 60), 3 runs per figure, measured 2026-09-22 to 2026-09-24:

| | Dynarmic JIT | Strict static, v0.0.10 code | Strict static, v0.0.11 |
|---|---|---|---|
| Windows x64 (Ryzen 9 9950X3D, RX 9070 XT) | 60 | 23.7 | **56.1** |
| macOS Apple Silicon | 59–60 | 53.7 | **59.9** |
| Linux x64 (RTX 3090) | 60 | 39.4 | **59.8** |

On Windows the gain comes from clang-cl (23.7 → 34.6), fast guest memory (36.0), the generation guard (40.2) and exact native FP (56.3 with all three). Every step keeps results bit-identical and can be switched off. Comparisons must run identical work: interleave arms within a round, keep pre-run load below 1.0, reject stale builds, and use a progressed save so the TAS measures a race rather than a menu.

## What is verified

On all three desktop platforms, ABI 6 race runs finished the TAS with zero critical errors and zero JIT fallbacks, the generation guard kept every module on its fast path, and FPX1 was negotiated. The FP harness is matched against Apple M-series hardware, including all 2^32 inputs of the unary operations. Strict static is a per-title result on MK8D v4.0.0, not a compatibility guarantee; Smash Ultimate in strict-static mode still hits uncovered code.

The no-JIT build uses `-DSUYU_NO_JIT=ON`. Dynarmic is excluded from its build graph and executable symbols, and runtime telemetry reports `jit_available=false`. Release downloads include the JIT; no-JIT binaries are built from source.

Synthetic tests cover decoder masks, integer/FP state, condition flags, crypto vectors, code guards, branch side entries and bounded module slices. The [differential harness](tests/differential_README.md) checks selected single instructions against Dynarmic. Whole-block memory and concurrency equivalence remain open.

## Build and validate a static title

The simplest path is suyu's **Export Game** dialog. The scripts below are the reproducible command-line workflow used for testing.

Supply your configured local dumps, keys and firmware. The boot source can differ from the update executable being exported: `MK8R_ROM` is the boot path and `MK8R_EXPORT_ROM` is the matching export source. The emitter supports AArch64; [AArch32](docs/aarch32.md) remains out of scope.

On the primary Windows workflow:

```powershell
.\scripts\build-suyu.ps1 -NoJit
.\scripts\export-static-title.ps1 -Target $env:MK8R_TARGET -Rom $env:MK8R_EXPORT_ROM
.\scripts\stage-static-title.ps1 -Target $env:MK8R_TARGET -TitleId <title-id>
```

The exporter rebuilds the host before exporting, builds every module and rejects stale output. Staging accepts ABI 5 or ABI 6 images and refuses a bundle that mixes them, since ABI 6 extends the shared guest context. Miss recording remains useful for genuinely missing code.

```powershell
.\scripts\playtest-static-title.ps1 -Target $env:MK8R_TARGET -Rom $env:MK8R_ROM `
  -SuyuExe build\suyu-nojit\bin\suyu.exe -RequireNoJit -TasReplay `
  -TasFixtureDirectory local\tas-fixtures\static-profile -TasObserveAfterSeconds 120
```

Keep baseline and static timing fixtures in separate local directories. The driver copies the chosen fixture for a run and preserves the existing recording. See [library playtesting](docs/library-playtesting.md) for recording, manifests and limitations.

## Next work

Broader per-title coverage (late-loaded modules and unimplemented opcodes are what recorded gaps mostly show), reliable full-race visual fixtures, and block/memory/concurrency differential checks. Runtime-loaded NROs and arbitrary generated guest code remain outside the fixed-module builder.

## What this repository contains

Our own code, build tooling and documentation. It contains no game data, no
Nintendo code, no keys, no firmware, and no generated C â€” that material is
produced locally and gitignored. The screenshots above are of our own tooling;
no frame of any game appears in this repository.

You need your own legally dumped game and your own keys. This project will not
help you obtain either, and will not answer requests to.

The rules the project holds itself to, including why recompiler output is never
published, are in [`LEGAL.md`](LEGAL.md). Working assumptions and their
confidence levels are in [`docs/assumptions.md`](docs/assumptions.md).

## Layout

```
docs/         architecture, assumptions, emulator findings
scripts/      reproducible PowerShell/Python for every step
tests/        decoder tests and differential validation
third_party/  suyu submodule, pinned to a commit on our fork
```

## Building

One command per platform, from nothing to built. Each installs the toolchain,
clones with submodules and builds. Neither fetches a game, keys, or firmware.

**Linux** â€” any distro with `apt`, `dnf`/`yum`, `pacman`, `zypper`, `apk`,
`xbps` or `eopkg`. Package names are per-distro; unknown ones are reported and
skipped rather than aborting the install:

```bash
curl -fsSL https://raw.githubusercontent.com/dougchansan/mk8-recomp/main/scripts/setup-linux.sh | bash
```

The script is bash, and Alpine and Void do not ship bash â€” install it first there
(`apk add bash`, `xbps-install -y bash`). Every other distro tested has it.

**Windows** â€” an Administrator PowerShell, because the MSVC C++ toolset needs
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
packages and run the build script â€” it needs CMake 3.31 or newer, and tells you
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

Building one generated module into a loadable image, once you have exported
locally:

```bash
./scripts/build-recomp.sh main       # Windows: .\scripts\build-recomp.ps1 -Module main
```

## Start here

1. [`docs/suyu-recompiler-findings.md`](docs/suyu-recompiler-findings.md) â€” what
   suyu v0.0.4's recompiler actually is, verified against source rather than
   against its release notes. Read this before anything else.
2. [`docs/bootstrap.md`](docs/bootstrap.md) â€” machine and toolchain verification
   for this workspace.
3. [`docs/architecture.md`](docs/architecture.md) â€” the pipeline as it exists and
   where we intend to extend it.
4. [`docs/aarch32.md`](docs/aarch32.md) â€” the one hard limit on what can be
   targeted, and how to check a title before spending an export on it.

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
recompiler-specific â€” most notably that no installed update or DLC was ever
indexed in NAND, for any title.

`externals/dynarmic` inside that fork points at
[`dougchansan/dynarmic`][dyn] for the same reason: upstream committed a
forwarding header containing an absolute path into a developer's home directory,
so it built on one machine. That repository is *not* archived, so unlike the
suyu changes it could reasonably go upstream as a PR.

[dyn]: https://github.com/dougchansan/dynarmic/tree/mk8-recomp
[fork]: https://github.com/dougchansan/suyu-v0.0.4/tree/mk8-recomp
[prov]: https://github.com/dougchansan/suyu-v0.0.4/blob/mk8-recomp/PROVENANCE.md

## Licence

GPL-3.0-or-later, matching upstream.
