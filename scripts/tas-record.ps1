# Set up a TAS recording session, so a real race can be replayed for benchmarking.
#
# Savestates are not implemented in this emulator - CreateSaveState and
# LoadSaveState are stubs that return false - and implementing them means
# capturing kernel objects mid-IPC and GPU state in flight, which is why yuzu
# and Ryujinx never shipped them either. Replaying inputs from boot gets the
# same thing for benchmarking purposes and the machinery already exists.
#
#   .\scripts\tas-record.ps1            # record a new script
#   .\scripts\tas-record.ps1 -Baseline  # record on dynarmic instead
#
# Then, in the suyu window:
#   Ctrl+F7   start recording        (press once - the game must be running)
#   ...       play into a time trial and drive a lap
#   Ctrl+F7   stop, and answer Yes to overwrite player 1's script
#
# The script lands in %APPDATA%\suyu\tas\ and bench-ab.ps1 -Tas replays it.

[CmdletBinding()]
param(
    [string]$Root   = 'G:\mk8-recomp',
    [string]$Target = $env:MK8R_TARGET,
    [string]$Rom    = $env:MK8R_ROM,
    [switch]$Baseline
)

$ErrorActionPreference = 'Stop'

$Exe      = Join-Path $Root 'build\suyu\bin\suyu.exe'
$RecompIn = Join-Path $Root "build\recomp\$Target"
$TasDir   = Join-Path $env:APPDATA 'suyu\tas'
New-Item -ItemType Directory -Force -Path $TasDir | Out-Null

Get-Process suyu -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

# tas_enable defaults to false, so without this the hotkeys do nothing at all
# and it looks like the feature is missing. pause_tas_on_load is left at its
# default of true: playback then waits for an explicit start rather than firing
# the moment a game loads, which is what lets a benchmark control the timing.
$cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
if (Test-Path -LiteralPath $cfg) {
    $t = Get-Content -LiteralPath $cfg -Raw
    $t = $t -replace 'tas_enable\\default=true', 'tas_enable\default=false'
    $t = $t -replace 'tas_enable=false', 'tas_enable=true'
    # Recording at an uncapped frame rate produces a script whose timing cannot
    # be reproduced, so record at the normal 60 FPS.
    $t = $t -replace 'use_speed_limit\\default=true', 'use_speed_limit\default=false'
    $t = $t -replace 'use_speed_limit=false', 'use_speed_limit=true'
    [System.IO.File]::WriteAllText($cfg, $t, (New-Object System.Text.UTF8Encoding $false))
    Write-Host 'tas_enable=true, frame limiter on for recording' -ForegroundColor Green
}

if ($Baseline) {
    Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue
    Write-Host 'recording on dynarmic (no AOT modules)' -ForegroundColor Yellow
} else {
    $env:SUYU_RECOMP_DIR = $RecompIn
    Write-Host "recording with AOT modules from $RecompIn" -ForegroundColor Green
}

$p = Start-Process -FilePath $Exe -ArgumentList '-hacker' -PassThru
Write-Host "suyu pid $($p.Id)"

$deadline = (Get-Date).AddSeconds(120)
while ((Get-Date) -lt $deadline) {
    if ((Test-NetConnection 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded) { break }
    Start-Sleep -Seconds 2
}

# Errors are deliberately not swallowed here. The first version piped this to
# Out-Null and the launch failed silently: the script printed its instructions,
# the game never booted, and there was nothing to say so.
python (Join-Path $Root 'scripts\mcp-call.py') launch_game_path --path $Rom
if ($LASTEXITCODE -ne 0) { throw "launch_game_path failed (exit $LASTEXITCODE)" }
Write-Host ''
Write-Host 'Game booting. Once it is at the title screen:' -ForegroundColor Cyan
Write-Host '  1. Ctrl+F7        start recording'
Write-Host '  2. play into Time Trials and drive a lap'
Write-Host '  3. Ctrl+F7        stop, answer Yes to overwrite'
Write-Host ''
Write-Host "Script will be written to $TasDir" -ForegroundColor Cyan
