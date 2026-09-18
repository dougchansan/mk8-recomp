# Library static playtesting

The playtest queue contains the 82 ARM64 cartridge bases from the final decode
survey plus the configured ARM64 update build for Mario Kart. The 24 titles that
the ISA survey could not open remain excluded. Dumps are opened read-only.

Run one canary first:

```powershell
.\scripts\playtest-library.ps1 -StartAtTitleId 0100152000022000 -Limit 1
```

Then resume the whole queue:

```powershell
.\scripts\playtest-library.ps1
```

Each title gets its own target, screenshots, logs, and `observation.json`
under `local/playtest/<TitleId>/`. `local/playtest/matrix.csv` is rewritten after
every title, so rerunning skips completed machine evidence. `-Force` reruns it.

The first export for each title uses an empty roots directory. ABI-4 generated
images make every aligned instruction inside a discovered block available as a
dispatch entry, which removes the per-title computed-branch root list for fixed
NSO modules. If a strict boot finds a required zero trap or another exporter
omission, the queue records that address and retries with the recorded roots.

After every build, the queue stages only the module DLLs under suyu's title-ID
cache and launches without `SUYU_RECOMP_DIR`:

```text
%APPDATA%\suyu\cache\aot\<16-digit title ID>\
```

`BootGame` reads the program ID; `LoadROM` stops any previous guest, selects the
bundle, and only then creates the next guest process. Automatic
loading requires generated-image ABI 4 and guard-v2, so older or mixed bundles
are refused. `bundle.json` is checked as an exact image set, including every
DLL's size and SHA-256, before anything loads. Stage an
individual build with:

```powershell
.\scripts\stage-static-title.ps1 -Target rootless-odyssey `
  -TitleId 0100000000010000
```

Set `SUYU_RECOMP_AUTO=0` for a Dynarmic control. `SUYU_RECOMP_DIR` remains an
explicit debugging override and takes precedence over automatic selection.

Entries under **suyu static AOT Builds** use `game_source.txt` only to identify
the title. They launch that title inside the current suyu process and require
its central manifest-validated cache; they do not start an older standalone
package executable. When an export was generated from an update NSP, the row
resolves the matching base application in the game list and preserves its
program index before launching. A missing cache or base application produces a
clear error instead of silently starting the JIT or opening a black legacy
window. Portable standalone exports still run directly when their source path
is no longer available.

Slow boots do not fail merely because an early capture time arrives before the
renderer. The runner records that capture as missed and continues to later
capture times. Use `-CaptureSeconds` to defer evidence for known slow titles.

The machine gate requires:

- renderer screenshots with visible, nonuniform content;
- a clean stop after the requested interval;
- guard-v2 negotiated;
- static blocks executed;
- zero static-to-JIT transitions under `SUYU_RECOMP_STRICT=1`.

This produces `REVIEW_REQUIRED`, not a visual pass. Review the latest capture
from each title and label whether it reached its title screen/menu:

```powershell
python scripts/build-playtest-contact-sheets.py local/playtest/matrix.csv local/playtest/contact-sheets
python scripts/review-playtest.py local/playtest/matrix.csv <TitleId> PASS --note "title menu visible"
```

If the static run fails after a successful export, the queue automatically runs
a Dynarmic control and records its evidence separately. That distinguishes a
static-recompiler failure from a title, firmware, applet, or general boot issue.

Once a title is visually approved, select its `Target` and paths from the matrix
and record the intended test path at the normal 60 FPS. Label each fixture's
scope: title prompt, menu navigation, attract sequence, race entry, or active
gameplay. A replay ending at title art cannot establish gameplay coverage. Keep
older fixtures as diagnostics; do not silently replace their acceptance scope.

Recording and playback use the `launch_game_path` boot mode. Both are armed
before the emulation thread starts and consume frame zero in the first renderer
TAS update. Keep physical controls neutral until the first game image during
recording. This aligns the input timeline; it does not guarantee identical
loading progress across CPU backends.

Use separate fixture directories when a slower backend needs later inputs:

```powershell
.\scripts\tas-record.ps1 -Target <target> -Rom "<cartridge path>" -Baseline `
  -TasFixtureDirectory local\tas-fixtures\<title-id>\dynarmic-boot
.\scripts\tas-record.ps1 -Target <target> -Rom "<cartridge path>" -RequireNoJit `
  -TasFixtureDirectory local\tas-fixtures\<title-id>\static-boot
```

Recording requires a fresh fixture directory and refuses existing script files.
Save with Ctrl+F7, then close suyu so the helper restores the previous TAS
directory. Without `-TasFixtureDirectory`, it backs up existing scripts to
`local/tas-archive/` before recording into the configured TAS directory.
`-RequireNoJit` selects the separate no-JIT host and checks its reported backend.

Select a fixture explicitly for replay:

```powershell
.\scripts\playtest-static-title.ps1 -Target <target> -Rom "<cartridge path>" `
  -SuyuExe .\build\suyu-nojit\bin\suyu.exe -RequireNoJit -TasReplay `
  -TasFixtureDirectory local\tas-fixtures\<title-id>\static-boot `
  -RunSeconds 600 -TasObserveAfterSeconds 120 -TasObserveCaptureIntervalSeconds 30
```

The runner copies the selected scripts into its evidence directory, records
their SHA-256 hashes, selects that snapshot through the driver's existing
`tas_directory` setting before startup, and restores the previous selection
after shutdown. It never overwrites the source fixture or the usual recording.
If no fixture is specified, it snapshots the currently configured TAS directory.
Pin a known command count with `-TasExpectedCommands` and an exact-EOF screenshot
reference with `-TasExpectedFinalImage` when those checks match the fixture's
scope. Visual approval remains required even when an image hash matches.

`-TasObserveAfterSeconds` is opt-in, bounded to 180 seconds, and starts only
after natural, non-looping EOF with exactly the loaded command count consumed.
It leaves playback stopped and observes the game with no further scripted input.
Keep physical controllers neutral during replay and observation. Delayed images
are stored separately in `observation.json` under `post_eof`; guard and backend
telemetry continue throughout. A late matching scene never changes the exact-EOF
image result, hides an early stop, or excuses a JIT transition. Review post-EOF
images separately to record the latest visible milestone and its delay.

Different timing profiles are functional tests. Performance comparisons require
the same script hash and command count across arms, with post-EOF observation
excluded from the timed workload. Run comparable arms interleaved on the idle
benchmark host; do not interpret longer fixture duration as CPU performance.

This removes manual dispatch-root configuration for code in the fixed NSO
images. Runtime-loaded NRO code still needs an export/load design of its own;
the boot/menu matrix cannot establish gameplay coverage for those modules.

For the final no-JIT check, build and select the separate frontend explicitly:

```powershell
.\scripts\build-suyu.ps1 -Configure -NoJit
.\scripts\playtest-static-title.ps1 ... `
  -SuyuExe .\build\suyu-nojit\bin\suyu.exe -RequireNoJit
```
