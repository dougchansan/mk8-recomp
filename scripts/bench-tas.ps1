# Equal-work backend comparison: replay one TAS fixture and time it to EOF.
#
# This is the answer to the attract-sampling problem. bench-ab.ps1 samples
# whatever the attract sequence happens to be showing, and that sequence is
# bimodal - a still title screen renders roughly nine times faster than the demo
# race on the same build. An arm that caught mostly title screen looks fast for
# no reason. Measured on MK8, Dynarmic's own race-phase median moved 161 -> 203
# FPS between two runs of the same unchanged backend, which is enough drift to
# manufacture any ratio you like.
#
# A recorded script is a fixed number of commands, so every arm does the same
# work by construction and the honest comparison is seconds-to-last-command.
#
# The frame limiter is forced ON, and that is not optional. The script is
# frame-indexed and was recorded at 60 Hz, so uncapped playback consumes inputs
# far faster than load-bound sections actually advance, the replay desyncs, and
# each arm ends somewhere different. Measured: uncapped, Dynarmic ran the script
# at 6.26x realtime and finished on a loading screen while static ran it at
# 1.73x and finished on the logo splash - neither reached the race, and the
# resulting "0.28x" compared two different scenes.
#
# With the limiter on, an arm that keeps up finishes in total/60 seconds and one
# that cannot takes longer, in proportion to how far behind it falls. That is
# the number worth having, and both arms reach the same game state.
#
#   .\scripts\bench-tas.ps1 -Target title-0100152000022000 `
#       -Rom ... -TasFixtureDirectory local\tas-fixtures\0100152000022000-race-start
#
# Arms: baseline is Dynarmic, static is strict (fallback refused). There is no
# hybrid arm, because playtest-static-title.ps1 forces strict for every
# non-baseline run. For a title whose static coverage is complete that costs
# nothing - MK8 and Smash both take zero fallbacks, so hybrid and static execute
# the same code - but do not read a static number here as a hybrid number for a
# title that does fall back.

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][string]$Rom,
    [Parameter(Mandatory)][string]$TasFixtureDirectory,
    [string[]]$Arms = @('baseline', 'static'),
    [int]$RunSeconds = 900,
    [int]$ObserveAfterSeconds = 0,
    [string]$OutDir = '',
    # Run the fixture against a different emulator build - an archived exe, say -
    # to tell a regression we introduced apart from one we inherited.
    [string]$SuyuExe = ''
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'

$Arms = @($Arms | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } |
          Where-Object { $_ })
foreach ($arm in $Arms) {
    if (@('baseline', 'static') -notcontains $arm) { throw "unknown arm '$arm'" }
}
if (-not $OutDir) { $OutDir = Join-Path $Root "local\bench\tas-$Target" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Set-ReplayConfig {
    # Limiter on at 100%, matching the rate the fixture was recorded at. vsync
    # stays off so the display refresh does not become the cap instead.
    $cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
    if (-not (Test-Path -LiteralPath $cfg)) { return }
    $t = Get-Content -LiteralPath $cfg -Raw
    $t = $t -replace 'use_speed_limit\\default=true', 'use_speed_limit\default=false'
    $t = $t -replace 'use_speed_limit=false', 'use_speed_limit=true'
    $t = $t -replace 'speed_limit\\default=false', 'speed_limit\default=true'
    $t = $t -replace 'speed_limit=\d+', 'speed_limit=100'
    $t = $t -replace 'use_vsync\\default=true', 'use_vsync\default=false'
    $t = $t -replace 'use_vsync=\d+', 'use_vsync=0'
    $t = $t -replace 'log_filter=.*', 'log_filter="*:Info"'
    [System.IO.File]::WriteAllText($cfg, $t, (New-Object System.Text.UTF8Encoding $false))
}

$results = @()
foreach ($arm in $Arms) {
    Write-Host ""
    Write-Host "=== $arm ===" -ForegroundColor Cyan
    Get-Process suyu* -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 4
    Set-ReplayConfig   # suyu rewrites the config on exit, so re-apply per arm

    $evidence = Join-Path $OutDir $arm
    # A hashtable, not an array: splatting an array binds POSITIONALLY, which
    # silently handed -RunSeconds the target name. And not $args either - that
    # is an automatic variable.
    $runArgs = @{
        Root = $Root; Target = $Target; Rom = $Rom
        TasReplay = $true; TasFixtureDirectory = $TasFixtureDirectory
        RunSeconds = $RunSeconds; EvidenceRoot = $evidence
        CaptureSeconds = @(60)
    }
    if ($ObserveAfterSeconds -gt 0) { $runArgs['TasObserveAfterSeconds'] = $ObserveAfterSeconds }
    if ($arm -eq 'baseline') { $runArgs['Baseline'] = $true }
    if ($SuyuExe) { $runArgs['SuyuExe'] = $SuyuExe }
    $armLog = Join-Path $OutDir "$arm-run.log"
    try {
        & (Join-Path $Root 'scripts\playtest-static-title.ps1') @runArgs *>&1 |
            Tee-Object -FilePath $armLog | Out-Null
    } catch {
        "EXCEPTION: $_" | Out-File -Append -Encoding utf8 -FilePath $armLog
        Write-Host "  arm threw: $_" -ForegroundColor Yellow
    }

    # Match the run stamp explicitly. The evidence root also holds a "roots"
    # directory, and a plain descending name sort picks that over 2026... - the
    # collector then found no observation.json and reported a completed run as
    # having produced nothing.
    $run = Get-ChildItem $evidence -Directory -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -match '^\d{8}T\d{6}Z$' } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $run) { Write-Host "  no evidence produced" -ForegroundColor Yellow; continue }
    $obs = Join-Path $run.FullName 'observation.json'
    if (-not (Test-Path -LiteralPath $obs)) { Write-Host "  no observation.json" -ForegroundColor Yellow; continue }
    $o = Get-Content -LiteralPath $obs -Raw | ConvertFrom-Json
    $results += [pscustomobject]@{
        Arm = $arm; Outcome = $o.tas_outcome; Commands = $o.tas_completed_commands
        EofSeconds = $o.eof_seconds; RealtimeRatio = $o.eof_realtime_ratio
        FirstFrame = $o.first_frame; LiveContent = $o.rendered_live_content
        FrozenTail = $o.frozen_tail; StaticBlocks = $o.max_static_blocks
        JitTransitions = $o.max_jit_transitions; Evidence = $run.Name
    }
}

Write-Host ""
Write-Host '=== equal-work comparison ===' -ForegroundColor Green
$results | Format-Table -AutoSize | Out-String -Width 220 | Write-Host

# Only arms that actually reached the last command are comparable. One that
# stopped early did less work, so its seconds are not a speed measurement.
$done = @($results | Where-Object { $_.EofSeconds })
if ($done.Count -ge 2) {
    $ref = $done | Where-Object { $_.Arm -eq 'baseline' } | Select-Object -First 1
    if (-not $ref) { $ref = $done[0] }
    Write-Host "ratio against $($ref.Arm), higher means faster:"
    foreach ($r in $done) {
        if ($r.Arm -eq $ref.Arm) { continue }
        Write-Host ("  {0,-10} {1,6:N2}x   ({2:N1}s vs {3:N1}s)" -f `
            $r.Arm, ($ref.EofSeconds / $r.EofSeconds), $r.EofSeconds, $ref.EofSeconds)
    }
} else {
    Write-Host 'not enough arms reached EOF to compare' -ForegroundColor Yellow
}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutDir 'summary.json')
