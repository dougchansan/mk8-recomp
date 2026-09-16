# Guard against shipping an emitter that cannot build the titles.
#
# The emitter is compiled into the exporter, so changing arm64_to_c.h changes
# the code every title generates - but nothing re-exports automatically, and a
# module set built before the change keeps working until someone regenerates it.
#
# That is exactly how this bit us. The emitter changed on 2026-09-15; MK8's
# modules were last built 09-14. Everything kept passing for two days against
# stale modules. The first re-export produced 4,669 compile errors from
# undefined shifted-register encodings, and after fixing those the modules
# crashed the emulator on load. Both defects existed the whole time and nothing
# looked at them.
#
#   .\scripts\check-emitter.ps1              # report staleness, run synthetics
#   .\scripts\check-emitter.ps1 -TestsOnly   # synthetics only, no ROMs needed
#   .\scripts\check-emitter.ps1 -Strict      # non-zero exit if anything is stale
#
# What this CANNOT do: verify a title still runs. That needs the ROM, keys and
# firmware, which never go near CI. The synthetic suite is the part that is
# portable; re-exporting and playtesting stays local and manual. Run it after
# any emitter change, on at least one ARM64 title and one whose base image is
# ARM32 - the shifted-register defect only appeared on the ARM32 one.

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [switch]$TestsOnly,
    [switch]$Strict
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'
$emitter = Join-Path $Root 'third_party\suyu\src\core\recompiler\arm64_to_c.h'
if (-not (Test-Path -LiteralPath $emitter)) { throw "emitter not found: $emitter" }

$emitterTime = (Get-Item -LiteralPath $emitter).LastWriteTime
$emitterHash = (Get-FileHash -LiteralPath $emitter -Algorithm SHA256).Hash
Write-Host "emitter  $($emitterHash.Substring(0,16))  $emitterTime"

$stale = @()
if (-not $TestsOnly) {
    Write-Host ""
    Write-Host '=== exported targets ===' -ForegroundColor Cyan
    $recomp = Join-Path $Root 'build\recomp'
    if (Test-Path -LiteralPath $recomp) {
        foreach ($t in Get-ChildItem -LiteralPath $recomp -Directory) {
            $dlls = @(Get-ChildItem -LiteralPath $t.FullName -Recurse -Filter '*.dll' -File -ErrorAction SilentlyContinue)
            if (-not $dlls) { continue }
            $oldest = ($dlls | Sort-Object LastWriteTime | Select-Object -First 1).LastWriteTime
            $isStale = $oldest -lt $emitterTime
            if ($isStale) { $stale += $t.Name }
            $mark = if ($isStale) { 'STALE ' } else { 'ok    ' }
            $colour = if ($isStale) { 'Yellow' } else { 'Gray' }
            Write-Host ("  {0} {1,-44} modules {2}" -f $mark, $t.Name, $oldest) -ForegroundColor $colour
        }
    }
    if ($stale) {
        Write-Host ""
        Write-Host "$($stale.Count) target(s) built before the current emitter." -ForegroundColor Yellow
        Write-Host 'Re-export at least one ARM64 title and one with an ARM32 base image:' -ForegroundColor Yellow
        Write-Host '  .\scripts\export-static-title.ps1 -Target <t> -Rom <rom> -SkipHostBuild'
        Write-Host 'then run the three arms and OPEN THE CAPTURES:'
        Write-Host '  .\scripts\bench-tas.ps1 -Target <t> -Rom <rom> -TasFixtureDirectory <fixture> -Arms baseline,hybrid,static'
    }
}

Write-Host ""
Write-Host '=== synthetic emitter tests ===' -ForegroundColor Cyan
# These need no game content, so they are the portable half and belong in CI.
# Each has its own runner because each generates C, compiles it, and executes it
# against a reference rather than being a plain unit test.
# Scored on a success marker in the output, NOT on the exit code. Several of
# these deliberately provoke a guard rejection or an expected trap to prove the
# refusal path works, and the process exits non-zero as a side effect - the
# side-entry test prints its pass line and still exits 1 because the code-guard
# mutation it induces aborts. Gating on exit code reports those as failures, and
# a check that cries wolf gets ignored.
$runners = @(
    @{ Name = 'shifted-register width'; Script = 'run-shifted-register-test.ps1'; Ok = 'checks passed' },
    @{ Name = 'translate';              Script = 'run-translate-test.ps1';        Ok = '/240 ok' },
    @{ Name = 'side entry';             Script = 'run-side-entry-test.ps1';       Ok = 'passed' }
)
$failed = @()
foreach ($r in $runners) {
    $path = Join-Path $Root "scripts\$($r.Script)"
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Host ("  skip   {0,-24} (no {1})" -f $r.Name, $r.Script) -ForegroundColor DarkGray
        continue
    }
    Write-Host ("  run    {0}" -f $r.Name)
    $log = Join-Path $env:TEMP "emitter-check-$($r.Script).log"
    try { & $path *> $log } catch { }
    $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
    if ($text -match [regex]::Escape($r.Ok)) {
        Write-Host ("  pass   {0}" -f $r.Name) -ForegroundColor Green
    } else {
        $failed += $r.Name
        Write-Host ("  FAIL   {0}  (no '{1}' in output)" -f $r.Name, $r.Ok) -ForegroundColor Red
        Write-Host ("         log: $log")
    }
}

Write-Host ""
if ($failed) { Write-Host "synthetic failures: $($failed -join ', ')" -ForegroundColor Red }
else { Write-Host 'synthetic tests passed' -ForegroundColor Green }
if ($stale) { Write-Host "stale targets: $($stale -join ', ')" -ForegroundColor Yellow }

# A green synthetic run does not mean the titles still work. The crash that
# followed the shifted-register fix passed every synthetic test and still took
# the emulator down on load.
if ($failed) { exit 1 }
if ($Strict -and $stale) { exit 2 }
exit 0
