# Control run: launch the target through UNMODIFIED suyu, no recompilation involved.
#
# This is the M0 baseline every later hybrid run is compared against. If this
# does not work, nothing downstream means anything.

[CmdletBinding()]
param(
    [string]$Root = 'G:\mk8-recomp',
    [string]$Game = $env:MK8R_ROM,
    [string]$KeysSource = '<your prod.keys>',
    [switch]$InstallKeys,
    [switch]$Gui,
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'

$Bin      = Join-Path $Root 'build\suyu\bin'
$ExeName  = if ($Gui) { 'suyu.exe' } else { 'suyu-cmd.exe' }
$Exe      = Join-Path $Bin $ExeName
$KeysDir  = Join-Path $env:APPDATA 'suyu\keys'
$TraceDir = Join-Path $Root 'traces\baseline'

if (-not (Test-Path -LiteralPath $Exe))  { throw "Not built: $Exe. Run scripts\build-suyu.ps1 first." }
if (-not (Test-Path -LiteralPath $Game)) { throw "Game dump not found: $Game" }

# Keys are the user's own. We copy an existing local file into suyu's key store;
# we never fetch, derive, or search for keys.
if ($InstallKeys) {
    if (-not (Test-Path -LiteralPath $KeysSource)) { throw "No prod.keys at $KeysSource" }
    New-Item -ItemType Directory -Force -Path $KeysDir | Out-Null
    Copy-Item -LiteralPath $KeysSource -Destination (Join-Path $KeysDir 'prod.keys') -Force
    Write-Host "Installed prod.keys -> $KeysDir" -ForegroundColor Green
}

if (-not (Test-Path -LiteralPath (Join-Path $KeysDir 'prod.keys'))) {
    Write-Warning "No prod.keys in $KeysDir. Re-run with -InstallKeys, or place your own there."
}

New-Item -ItemType Directory -Force -Path $TraceDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log   = Join-Path $TraceDir "baseline-$stamp.log"

# Record what we ran against, so a log is self-describing.
@(
    "exe        : $Exe"
    "rom        : $Game"
    "rom size   : $((Get-Item -LiteralPath $Game).Length)"
    "keys       : $(Test-Path -LiteralPath (Join-Path $KeysDir 'prod.keys'))"
    "started    : $(Get-Date -Format o)"
    "suyu commit: $(git -C (Join-Path $Root 'third_party\suyu') rev-parse HEAD)"
    '---'
) | Set-Content -Encoding utf8 $log

Write-Host "Launching baseline (timeout ${TimeoutSeconds}s), log: $log" -ForegroundColor Cyan

$sw = [Diagnostics.Stopwatch]::StartNew()
$p  = Start-Process -FilePath $Exe -ArgumentList @('-g', $Game) -PassThru -NoNewWindow `
        -RedirectStandardOutput "$log.out" -RedirectStandardError "$log.err"

if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
    Write-Host "Still running after ${TimeoutSeconds}s - that is the good outcome for a boot test." -ForegroundColor Green
    # Ask nicely first: suyu's file logger flushes on shutdown, so a hard Kill
    # leaves suyu_log.txt empty and the run unusable as evidence.
    $null = $p.CloseMainWindow()
    if (-not $p.WaitForExit(15000)) { $p.Kill() }
    $reachedTimeout = $true
} else {
    Write-Host "Exited after $([int]$sw.Elapsed.TotalSeconds)s with code $($p.ExitCode)" -ForegroundColor Yellow
    $reachedTimeout = $false
}
$sw.Stop()

Get-Content "$log.out", "$log.err" -ErrorAction SilentlyContinue | Add-Content -Encoding utf8 $log
Remove-Item "$log.out", "$log.err" -ErrorAction SilentlyContinue

# suyu writes its own log to the user directory; fold it in so one file has everything.
$suyuLog = Join-Path $env:APPDATA 'suyu\log\suyu_log.txt'
if (Test-Path -LiteralPath $suyuLog) {
    Copy-Item -LiteralPath $suyuLog -Destination (Join-Path $TraceDir "suyu_log-$stamp.txt") -Force
}

[pscustomobject]@{
    Log            = $log
    ElapsedSeconds = [int]$sw.Elapsed.TotalSeconds
    ReachedTimeout = $reachedTimeout
    ExitCode       = if ($reachedTimeout) { $null } else { $p.ExitCode }
}
