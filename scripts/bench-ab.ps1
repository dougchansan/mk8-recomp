# Matched hybrid-vs-baseline FPS comparison.
#
# Both arms run identically: same ROM, same warmup, same sample count, same
# interval, frame limiter and vsync off. That matters more than it sounds -
# earlier single-screenshot readings gave 473 FPS hybrid against 522 baseline,
# which was meaningless, because the two captures landed on different parts of
# The attract sequence. The title screen and the demo race are very different
# workloads and the distribution is bimodal, so a single number cannot compare
# two builds.
#
# Sample long enough to cover several attract cycles in both arms, then compare
# percentiles rather than means.
#
#   .\scripts\bench-ab.ps1

[CmdletBinding()]
param(
    [string]$Root    = $env:MK8R_ROOT,
    [string]$Target  = $env:MK8R_TARGET,
    [string]$Rom     = $env:MK8R_ROM,
    [int]$Warmup     = 90,
    [int]$Samples    = 60,
    [double]$Interval = 2.0,
    [string]$OutDir  = $(Join-Path $env:TEMP 'mk8r-bench'),
    # Replay a recorded TAS script instead of measuring whatever the attract
    # sequence happens to be showing. That sequence alternates between a static
    # title screen and a demo race and does not repeat identically between runs,
    # which is the source of most of the noise these measurements fight.
    [switch]$Tas
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'

$Exe      = Join-Path $Root 'build\suyu\bin\suyu.exe'
$RecompIn = Join-Path $Root "build\recomp\$Target"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Set-Unlimited {
    $cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
    if (-not (Test-Path -LiteralPath $cfg)) { return }
    $t = Get-Content -LiteralPath $cfg -Raw
    # Force the log filter back to Info. A debug filter left in the config from
    # some earlier investigation floods the log, slows the boot enormously, and
    # pushes every sample into the shader-compilation window - which the sampler
    # discards, so the arm silently produces no data at all. That happened once
    # and looked like the build under test had crashed.
    $t = $t -replace 'log_filter\\default=true', 'log_filter\default=false'
    $t = $t -replace 'log_filter=.*', 'log_filter="*:Info"'
    $t = $t -replace 'use_speed_limit\\default=true', 'use_speed_limit\default=false'
    $t = $t -replace 'use_speed_limit=true', 'use_speed_limit=false'
    $t = $t -replace 'use_vsync\\default=true', 'use_vsync\default=false'
    $t = $t -replace 'use_vsync=\d+', 'use_vsync=0'
    [System.IO.File]::WriteAllText($cfg, $t, (New-Object System.Text.UTF8Encoding $false))
}

function Invoke-Arm([string]$name, [bool]$hybrid) {
    Write-Host ""
    Write-Host "=== $name ===" -ForegroundColor Cyan

    Get-Process suyu -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 3

    # suyu rewrites the config on exit, so this is re-applied per arm.
    Set-Unlimited

    if ($hybrid) {
        $env:SUYU_RECOMP_DIR = $RecompIn
    } else {
        Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue
    }

    $p = Start-Process -FilePath $Exe -ArgumentList '-hacker' -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) {
        if ((Test-NetConnection 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded) { break }
        Start-Sleep -Seconds 2
    }

    $json = Join-Path $OutDir "$name.json"

    # Capture the screen periodically while the arm runs. FPS alone cannot tell
    # a fast build from a fast *and correct* one - a rendering regression shows
    # up as a higher frame rate, not a lower one - so every arm leaves behind
    # evidence of what it actually drew.
    $shots = Join-Path $OutDir "shots-$name"
    New-Item -ItemType Directory -Force -Path $shots | Out-Null
    $capture = Start-Job -ScriptBlock {
        param($root, $dir, $warmup, $count, $gap)
        Start-Sleep -Seconds ($warmup + 10)
        for ($i = 1; $i -le $count; $i++) {
            try {
                & (Join-Path $root 'scripts\capture-window.ps1') `
                    -Out (Join-Path $dir ('{0:d2}.png' -f $i)) | Out-Null
            } catch { }
            Start-Sleep -Seconds $gap
        }
    } -ArgumentList $Root, $shots, $Warmup, 8, 30

    if ($Tas) {
        # Boot, let it settle, then start playback explicitly. pause_tas_on_load
        # keeps the script from firing while the game is still loading, so the
        # start has to be triggered rather than waited for.
        # Arguments as argv, not JSON: PowerShell strips the inner quotes from a
        # JSON string before python sees it, so the call silently fails to parse.
        python (Join-Path $Root 'scripts\mcp-call.py') launch_game_path --path $Rom
        if ($LASTEXITCODE -ne 0) { throw "launch_game_path failed (exit $LASTEXITCODE)" }
        Start-Sleep -Seconds $Warmup
        python (Join-Path $Root 'scripts\mcp-call.py') trigger_ui_action --action tas_start_stop
        if ($LASTEXITCODE -ne 0) { throw "tas_start_stop failed (exit $LASTEXITCODE)" }
        Write-Host 'TAS playback started' -ForegroundColor Green
        python (Join-Path $Root 'scripts\fps-sample.py') $Rom `
            --warmup 0 --samples $Samples --interval $Interval --out $json --no-launch
    } else {
        python (Join-Path $Root 'scripts\fps-sample.py') $Rom `
            --warmup $Warmup --samples $Samples --interval $Interval --out $json
    }

    Stop-Job -Job $capture -ErrorAction SilentlyContinue
    Remove-Job -Job $capture -Force -ErrorAction SilentlyContinue

    $null = $p.CloseMainWindow()
    if (-not $p.WaitForExit(120000)) { $p.Kill() }
    Start-Sleep -Seconds 3
}

Invoke-Arm 'hybrid'   $true
Invoke-Arm 'baseline' $false

Write-Host ""
Write-Host '=== comparison ===' -ForegroundColor Green
python (Join-Path $Root 'scripts\bench-compare.py') `
    (Join-Path $OutDir 'hybrid.json') (Join-Path $OutDir 'baseline.json')
