# mk8-recomp

The current goal is a standalone native Mario Kart 8 Deluxe application: statically recompiled CPU code connected to native graphics, platform services, audio, input and storage. The MK8 native runtime completes the four-course Mushroom Cup race sequence, including underwater and anti-gravity driving. The latest normal-speed comparison averages 59.50-59.96 FPS across those courses. Interactive play supports a keyboard, XInput pads and wired Switch-layout USB pads (PowerA, HORI). Full-game compatibility, subjective audio/control checks and true 125/250 FPS remain in progress.

See [native runtime progress](docs/native-runtime-progress.md) for verified results and remaining work. This repository is `dougchansan/mk8-recomp`.

**Release policy:** Public game-linked binaries are prohibited. Full native compatibility does not authorize downloadable game-linked executables. Audited authored source and game-free tooling only may be published after provenance review. Existing foundations prereleases, download assets and tags were removed; private development builds stay local. See [LEGAL.md](LEGAL.md), [distribution audit](docs/distribution-audit.md) and [build notes](docs/native-builds.md).

The [MK8 native runtime integration](integrations/belmante-runtime/README.md) uses static CPU modules and native Vulkan. R201 completes all three laps, the FINISH sequence and the results table without a fatal error. The native game commits its post-race save; a fresh process reloads an exact copy and reaches mode selection. A measured 11,280-frame racing window averages about 25 FPS (26.5 ms median interval), with profiling disabled and sparse readbacks enabled. Forty component suites, 2,327 native GPU checks and separate 300-check sync/cutoff regressions pass. The offline cache held 7,365 stages at the time (see R276 below for current coverage). No native release, tag or download is available. See [native runtime progress](docs/native-runtime-progress.md) for the remaining verification gates.

R202 repeats the full race with the real native audio device processing at zero gain: first place, 15 points, over 129,000 completed audio packets and no reported mixer drops or underruns. Its observed racing window averages about 29 FPS; sustained full speed and subjective listening remain unverified.

A later repeat exposed another descriptor timing failure. R205 fixes a regression-backed stale empty-handle cache entry; the repeat race now finishes first with 15 points. Advancing results exposes a missing texture readback operation. R207 verifies the actual 480×270 native readback and reaches loading. R210/R212 address further replay-cache lifetime failures with regressions; actual second-course validation remains in progress. The successful races do not establish full gameplay stability.

R212 passes a 24,000-frame run through Stadium results and Water Park lap 2/3. A same-build control with frame-signature hashing off and sparse captures observes about 38.5 FPS over 3,900 course presentations; sustained 60 FPS, complete-cup coverage and human audio/control checks remain open. The chunked-submission experiment reaches the unchanged memory guard and is rejected.

R218 enables precise-query grouping by default after a verified R217 Stadium win and Water Park lap. Racing averages are 50.02 and 53.52 FPS, with median reporting windows near 59 FPS; sustained smooth 60 remains open. Exact sample-count and clear-exclusion regressions pass, and `NX_COALESCE_SAMPLE_QUERIES=0` retains the diagnostic fallback.

R220 fixes coarse Windows sleep timing and adds actual frame-delivery attribution
and reusable local driver-cache support. Same-executable tests reduce timer
overshoot, while racing remains variable around 47–50 FPS in the latest short
controls. Smooth 60 and correct 120 FPS gameplay remain performance goals.

R221 reduces focus-check contention; R222 shares fully revalidated positive
texture identities across descriptor copies. The latest clean R221 short course
control averages 52.36 FPS, with late windows at 60. The first R222 timing run was
affected by an unrelated build; its longer flight now verifies Stadium first and
Water Park lap 2/3. R223 reduces diagnostic readback stalls using cached coherent
memory and passes a 6,000-target game check; that course window averages 53.93 FPS.
R235 removes a 64 KiB allocation tail from exact 16-byte counter bindings and
strengthens timestamp-result visibility. The measured report uses 512 rather
than 66,048 upload bytes, with exact padded GPU-read regressions. Both allocation
modes pass 6,000-presentation game controls; course averages of 51.87 and 50.80
FPS show no demonstrated speed gain. The source recipe reconstructs 200 generic
files. R245 fixes worker-handle leaks and native thread descriptions, synchronizes
priority metadata and expands validation to 42 suites. Its default scheduling
control averages 57.28 FPS; optional native priority mapping remains off after
showing no demonstrated speed gain. R248 adds opt-in pipeline recipes that
prewarm the graphics pipelines a previous local flight used: with a covering
recipe file, draw-path pipeline creation falls from 0.7-1.1 s (warm driver cache)
or 12.9 s (cold) to 116 ms, and the worst course-load presentation from about
1.2 s to 0.5 s. No steady-FPS gain is established. The default source recipe now
reconstructs 210 files. `NX_OVERLAY=1` shows an FPS/frame-time overlay. Sustained smooth 60 and correct 120 remain unverified.

R284 reduces single-thread focus-gate overhead while preserving every ownership
and visibility check, and records minimum presentation intervals for future
course reports. See [main-thread measurements and per-course FPS](docs/native-main-thread-focus.md).

R299 imports a prior local pipeline catalog without sharing mutable caches.
With a covering private capture, Sweet Sweet Canyon and Thwomp Ruins entry
peaks fall from 3.0–3.4 s and 2.7–3.3 s to 417 ms and 319 ms. This moves known
compilation ahead of course entry; it is not a steady-FPS gain. See
[catalog measurements and current racing readings](docs/native-pipeline-catalogs.md).

R300 adds opt-in individual frame intervals, submission IDs, asynchronous GPU
execution records and timed hitch events. A short Stadium run verifies the
measurement streams; no performance improvement is claimed. See
[native timing measurements](docs/native-timing-measurements.md) for the
plain/GPU comparison and separate diagnostic profiles.

R304 moves recipe-history filtering and merging to the existing saver thread,
while capturing owned inputs under the engine lock. In the measured save events,
presentation-thread preparation falls from 22.16 ms to 3.36 ms. See
[recipe-save preparation measurements](docs/native-recipe-save-performance.md)
for correctness checks and limits; this is a periodic-pause reduction.

R308 removes stable-policy read-lock contention while retaining live display
ownership, generation and visibility queries on every enabled gate call.
Unprofiled gate benchmarks improve; the two gameplay runs have mixed
per-course results. See [focus revision measurements](docs/native-focus-revision-performance.md).

R313 reduces RTTI work in copied-texture registry lookups while preserving the
first matching owner and complete identity validation. The isolated fixture
improves by 47.2%; a separate racing diagnostic also measures lower lookup cost.
Four-course presentation results remain mixed. See
[texture registry measurements](docs/native-texture-registry-performance.md).

R324 removes a measured GPU query-resolution stall by collecting actual completed
queries after existing full retirement. Normal-speed four-course averages improve
from 53.25-57.80 to 59.50-59.96 FPS in the observed controls; paths vary. See
[counter measurements and correctness](docs/native-counter-retirement-performance.md),
[asynchronous GPU phases](docs/native-gpu-phase-profile.md) and the
[simulation-clock audit](docs/native-frame-clock-audit.md). Raising the display
override accelerates the race clock, so older uncapped readings do not establish
normal-speed high refresh. The source recipe contains 214 generic files.

R282 moves optional diagnostics out of ordinary CPU dispatch. Authored alternating
benchmarks show lower dispatch overhead, with all 44 suites and 3,212 GPU checks
passing. See [dispatch measurements and sweep limits](docs/native-dispatch-optimization.md);
the microbenchmark improvement is not a whole-game FPS claim.

R249-R280 (October 2026) turn to what the game looks like and how it is played.
R249/R250 attribute slow presentations: late frames wait on the title's main
thread, not on GPU work. R252 adds a metrics overlay (FPS, frames shown, frame
time, render size) and whole-frame draw traces. R264 renders each target into
the storage it was recorded with, so the environment cube that character
materials reflect is no longer black. R274 fixes the dark characters and the
white bloom (glider glow, glowing kart lamps, lights shining through walls): the
game's CPU-written lighting tables are linear in memory and had been untiled as
block-linear, which zeroed most of the specular table and produced NaN in the
light pass. Compared side by side with suyu on the same save and course, kart
select and the race now match. R275 reads wired Switch-layout USB pads directly
(PowerA, HORI) alongside XInput and the keyboard. Offline shader coverage now
includes every course effect file and every compressed archive (karts, drivers,
courses, UI): 14,046 translated stages in all. R276 adds an opt-in
`NX_SKIP_MISSING_SHADERS=1` for collection sessions, which skips the draws of a
shader still missing and dumps it for translation instead of stopping. An
unattended 200cc sweep of every cup found two stops: R278 reads a uniform block
that runs past its memory pool (Sweet Sweet Canyon), and R280 samples an R32F
texture through a shadow sampler via a D32F copy with the same bits (Shy Guy
Falls). R279 can place the window on a second monitor and take scripted input
without keyboard focus. All 44 suites and 3,212 GPU checks pass. The default
source recipe reconstructs 212 files. Sustained smooth 60, first-visit pipeline hitches on courses outside the
recorded flights, and full gameplay verification remain open.

The following figures describe the earlier guarded-runtime research track; the current native race result is the Belmante integration above.

Private CPU bring-up passes 12,017 checks across 16 fresh processes for genuine SDK bootstrap, the title's 24 MiB allocator, installed allocation/free hooks, aligned-pointer handling and finalization. One captured game background now draws through native Vulkan and reaches 45 successful, independently retired native presentations without copying pixels through the CPU. All 2,073,600 packed source pixels remain exact. Live native Device integration, complete game frames, audio, controls and save/restart remain pending.

A separate private Device-coordinator CPU model passes 22,824 checks across 76 fresh cases for callback returns, cancellation, stale identities and teardown. Native Device creation for an actual MK8D request remains an integration gate.

The complete guarded static CPU build now passes independent review: 74 objects form the four title-module libraries and one shared runtime. The source covers 1,076,832 blocks and 4,832,474 instruction entries, retaining 42 explicit unsupported sites that halt. The standalone guarded Host now passes all 24 strict C++ compilations and fresh stopped controls. Its latest original-entry run executes 12,614,619 fenced blocks across 3,478 slices and 90 SVC calls before the missing `pctl:s` service. All four execution and memory bindings clear before storage teardown.

Native `aoc:u`, `pctl` and all three restricted `pctl:s`, `pctl:a` and `pctl:r` ports now participate in actual startup with distinct owned interfaces. The primary child completes Initialize; the restricted children are created and retired without method calls. Fifteen CPU suites pass, including 112 filesystem process-binding checks and the existing core, priority, process-policy and whole-heap tests. A fresh game boot completes its heap and thread setup, connects to native `fsp-srv`, converts to a domain, clones the session and successfully initializes FSP. It next stops at `GetGlobalAccessLogMode` (1005) after 12,783,762 blocks, 9,078 slices and 188 SVC calls. The cloned handle shares the original domain and endpoint state; the binding commits once after its reply write. Root directly reviewed the startup, and its exact binary is retained in the 31-entry local index. The explicit native disabled logging policy is being integrated. Native content reads, Device creation and a live game frame remain pending.

The new SDK and shared-runtime archives also pass compiled compatibility review with the 57 retained title objects. Their canonical Device callback descriptor is present in compiled data. Linking and exercising that path in the native game consumer remains pending.

The native Vulkan Device bridge passes 42 CPU rejection checks and 43 actual Vulkan component checks: factory creation, identity and buffer validation, an exact readback, same-device presentation, revocation after submission, and one final drain/destruction. A fresh build of all 35 static-runtime C++ units, including 19 matching Sirit units, passes independent compiled-artifact review. All eight runtime fixtures now pass, covering the Device component, GPU copies, drawing, persistent images, state, presentation conversion and retirement. The resource fixture now checks that access is denied after parent retirement and explicit lease release. These component results do not yet establish an MK8D Device request or a native game frame. Audio, controls and save/restart remain pending.

## Historical static-recompiler release (September 2026)

The following v0.0.11 release and performance information describes the earlier suyu-hosted static CPU work. It does not establish completion of the native graphics/platform migration.

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

## Retired suyu-hosted export workflow

The older keyed baseline, static export, re-export and one-click setup workflows were retired in October 2026. Suyu-hosted performance figures remain historical research results, **not** current build or distribution instructions. Current local development generates private modules from previously extracted user-supplied game inputs using the [native integration](integrations/belmante-runtime/README.md).

## Historical next work (September 2026)

Broader per-title coverage (late-loaded modules and unimplemented opcodes are what recorded gaps mostly show), reliable full-race visual fixtures, and block/memory/concurrency differential checks. Runtime-loaded NROs and arbitrary generated guest code remain outside the fixed-module builder.

## What this repository contains

Our own code, build tooling and documentation. It contains no game data, no
Nintendo code, no keys, no firmware, and no generated C — that material is
produced locally and gitignored. The screenshots above are of our own tooling;
no frame of any game appears in this repository.

Native title-specific tests use lawfully obtained, locally supplied game inputs. No encrypted-game/key installer is provided. **Public game-linked binaries are prohibited.** See [LEGAL.md](LEGAL.md), [distribution audit](docs/distribution-audit.md) and [archived assumptions](docs/assumptions.md).

## Layout

```
docs/         architecture, assumptions, emulator findings
scripts/      reproducible PowerShell/Python for every step
tests/        decoder tests and differential validation
third_party/  suyu submodule, pinned to a commit on our fork
```

## Current native development

Follow the [Belmante integration README](integrations/belmante-runtime/README.md) for local source preparation, private title-module generation and tests. Legacy suyu-hosted build/export instructions are retired; do not upload local outputs. For candidate authored-source archives, run `python3 scripts/dist_audit.py --artifact dist/source.zip` and obtain provenance review.

## Start here

1. [Current native integration](integrations/belmante-runtime/README.md)
2. [Native runtime progress](docs/native-runtime-progress.md)
3. [Private development vs. public-source policy](docs/native-builds.md)
4. [Distribution audit](docs/distribution-audit.md)
5. [Archived suyu findings](docs/suyu-recompiler-findings.md)

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

## References

Software this work builds on, studies or compares against. Each keeps its own licence; versions are the ones
recorded in this repository or its build inputs.

1. **SuperMarioWonderRecomp**, BelmanteGu (Projeto Belmante).
   <https://github.com/BelmanteGu/SuperMarioWonderRecomp>, MIT. The native runtime and AArch64-to-C++
   recompiler the [Belmante integration](integrations/belmante-runtime/README.md) adapts, pinned at
   `010ed7a596a621c15d3a8dfa5795ca8c3668b066` (`UPSTREAM.json`, `LICENSE.upstream`).
2. **suyu v0.0.4**, suyu-emu, base commit `d1d09321d7ab84252291e05b3efbc8a8dfa57481`, continued as
   [`dougchansan/suyu-v0.0.4`][fork], GPL-3.0-or-later (with GPL-2.0-or-later files inherited from yuzu; see its
   [`PROVENANCE.md`][prov]). The emulator host of the historical static-recompiler track, the reference
   implementation for Switch GPU behaviour, and the side-by-side reference for native visual checks.
3. **Eden** <https://git.eden-emu.dev/eden-emu/eden> and **yuzu**, the emulators the current suyu line descends
   from; its `shader_recompiler` (Maxwell to SPIR-V) is used offline to translate MK8D's shaders.
4. **sirit**, the SPIR-V emitter used by that translator: Eden's fork <https://github.com/eden-emulator/sirit>
   (from ReinUsesLisp's original sirit); its v1.0.5 source carries GPL-3.0 and AGPL-3.0 licence texts.
5. **Ryujinx shader translator** (`Ryujinx.Graphics.Shader`, MIT), as maintained by Ryubing
   <https://codeberg.org/Software_arch/Ryubing> (upstream build input `a82350bb77`), used by upstream's offline
   SPIR-V harness with upstream's patches; MK8D's shader cache comes from item 3.
6. **dynarmic**, the ARM recompiler inside the suyu fork, via [`dougchansan/dynarmic`][dyn].
7. **Vulkan** headers and loader, The Khronos Group, Apache-2.0 / MIT; **SDL 3** <https://libsdl.org>, zlib, and
   **MoltenVK** <https://github.com/KhronosGroup/MoltenVK>, Apache-2.0, on the upstream runtime's non-Windows hosts.
8. **envytools** <https://github.com/envytools/envytools> and **Switchbrew** <https://switchbrew.org>: Maxwell GPU
   and Switch system documentation used by upstream and by this work.
9. *Sony Computer Entertainment v. Connectix* (9th Cir. 2000) and *Sony Computer Entertainment v. Bleem*
   (9th Cir. 2000), cited in [LEGAL.md](LEGAL.md).

Nintendo, Nintendo Switch and Mario Kart are trademarks of Nintendo. This project is not affiliated with or
endorsed by Nintendo. As upstream states, the NVN API is reverse engineered from binaries; no leaked NVN headers
are used.
