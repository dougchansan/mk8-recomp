# mk8-recomp

[**suyu v0.0.10 � static experimental checkpoint**](https://github.com/dougchansan/suyu-v0.0.4/releases/tag/v0.0.10)

Ahead-of-time recompilation of Nintendo Switch AArch64 CPU code to native x86-64, using suyu's HLE stack for everything above the CPU. **Use Hybrid AOT + JIT for best performance.** Static execution is experimental: it runs tested title/menu/attract paths with no JIT, but loading and gameplay can be slower.

| Mode | When to use it |
|---|---|
| suyu static (Experimental) | Test ahead-of-time execution. The separate no-JIT host binaries contain no Dynarmic. |
| Dynarmic JIT (Baseline) | Compare behavior and retain general compatibility. |
| Hybrid AOT + JIT | Recommended for normal play and performance; uncovered code may use Dynarmic. |

[Release notes](https://github.com/dougchansan/suyu-v0.0.4/blob/v0.0.10/docs/releases/v0.0.10.md) explain the regular and `no-jit` downloads. **Regenerate older static modules for ABI 4.** Updating the host alone cannot update generated code. The earlier [rendering correction](docs/static-rendering-fix.md) remains included.

## What is verified

Guarded no-JIT runs reach controller prompts, menus, attract rendering and the race starting grid with zero fallback attempts. The latest race-start fixture is visually confirmed on Dynarmic. Static reaches the grid during a bounded idle observation after EOF; exact EOF image matching remains failed because progress is delayed. This is limited path coverage, not full-race or universal-library correctness.

The no-JIT build uses `-DSUYU_NO_JIT=ON`. Dynarmic is excluded from its build graph and executable symbols; runtime telemetry also reports `jit_available=false`. Merely selecting static export mode in an ordinary host disables fallback in the generated launcher but does not remove the compiler from the ordinary host binary.

Synthetic tests cover decoder masks, integer/FP state, condition flags, crypto vectors, code guards, branch side entries and bounded module slices. The [differential harness](tests/differential_README.md) checks selected single instructions against Dynarmic; known oracle flag disagreements remain visible. Whole-block memory and concurrency equivalence remain open.

## What changed since v0.0.8

Automatic bundles validate title identity, hashes, ABI and code bytes. Library launches use the active hosted bundle. Per-CPU relocation initialization avoids the boot regression caused by sharing one process-pointer gate. ABI 4 exposes aligned instruction entries and runs a bounded nonrecursive loop inside each module to reduce host dispatch.

Recording and replay can be armed before the emulation thread starts. Exact command completion and screenshots are retained separately. See the [campaign summary and regression safeguards](docs/static-campaign.md) for why the old title-screen fixture was retired and how future changes are checked.

## Build and validate a static title

Supply your configured local dumps, keys and firmware. The boot source can differ from the update executable being exported: `MK8R_ROM` is the boot path and `MK8R_EXPORT_ROM` is the matching export source. The emitter supports AArch64; [AArch32](docs/aarch32.md) remains out of scope.

On the primary Windows workflow:

```powershell
.\scripts\build-suyu.ps1 -NoJit
.\scripts\export-static-title.ps1 -Target $env:MK8R_TARGET -Rom $env:MK8R_EXPORT_ROM
.\scripts\stage-static-title.ps1 -Target $env:MK8R_TARGET -TitleId <title-id>
```

The exporter rebuilds the host before exporting, builds every module and rejects stale output. ABI 4 supplies aligned side entries in discovered fixed modules; an empty roots directory is the initial export. Miss recording remains useful for genuinely missing code.

```powershell
.\scripts\playtest-static-title.ps1 -Target $env:MK8R_TARGET -Rom $env:MK8R_ROM `
  -SuyuExe build\suyu-nojit\bin\suyu.exe -RequireNoJit -TasReplay `
  -TasFixtureDirectory local\tas-fixtures\static-profile -TasObserveAfterSeconds 120
```

Keep baseline and static timing fixtures in separate local directories. The driver copies the chosen fixture for a run and preserves the existing recording. The idle observation is opt-in and cannot turn a failed exact-EOF test into a pass. Visual milestone review is still required. See [library playtesting](docs/library-playtesting.md) for recording, manifests and limitations.

## Performance and next work

Static remains slower in current observed loading and rendering paths. The Linux profiling work pins source and module hashes, waits for an idle machine and samples the actual current build. No current speedup ratio is claimed. Earlier ratios used an older emitter and a retired title-screen fixture; they should not be used to compare v0.0.10 gameplay.

Functional timing profiles can differ. Performance comparisons must execute identical work, interleave arms within a round, record pre-run load below 1.0, and reject stale builds. Do not infer a small speedup from noise or compare differently timed replays as equivalent workloads.

Next priorities are profiling-led optimization, reliable visual race-entry/full-race fixtures, broader per-title execution coverage, and block/memory/concurrency differential checks. Runtime-loaded NROs and arbitrary generated guest code remain outside the fixed-module builder.

## What this repository contains

Our own code, build tooling and documentation. It contains no game data, no
Nintendo code, no keys, no firmware, and no generated C — that material is
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

**Linux** — any distro with `apt`, `dnf`/`yum`, `pacman`, `zypper`, `apk`,
`xbps` or `eopkg`. Package names are per-distro; unknown ones are reported and
skipped rather than aborting the install:

```bash
curl -fsSL https://raw.githubusercontent.com/dougchansan/mk8-recomp/main/scripts/setup-linux.sh | bash
```

The script is bash, and Alpine and Void do not ship bash — install it first there
(`apk add bash`, `xbps-install -y bash`). Every other distro tested has it.

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

Building one generated module into a loadable image, once you have exported
locally:

```bash
./scripts/build-recomp.sh main       # Windows: .\scripts\build-recomp.ps1 -Module main
```

## Start here

1. [`docs/suyu-recompiler-findings.md`](docs/suyu-recompiler-findings.md) — what
   suyu v0.0.4's recompiler actually is, verified against source rather than
   against its release notes. Read this before anything else.
2. [`docs/bootstrap.md`](docs/bootstrap.md) — machine and toolchain verification
   for this workspace.
3. [`docs/architecture.md`](docs/architecture.md) — the pipeline as it exists and
   where we intend to extend it.
4. [`docs/aarch32.md`](docs/aarch32.md) — the one hard limit on what can be
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
recompiler-specific — most notably that no installed update or DLC was ever
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
