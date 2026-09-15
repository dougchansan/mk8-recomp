# Sweep SUYU_RECOMP_CHAIN_BUDGET across one arm and compare the distributions.
#
# The chain budget bounds how many blocks a chain of direct calls may run inside
# a generated module before returning to the host dispatcher. The built-in
# default is 32; SetRecompLongSlices raises it to 4096, which is what the ABI 4
# profiling run used. The environment override wins over both and accepts
# 1..8192 (arm_recomp.cpp:156-169), so the whole range is reachable without a
# rebuild.
#
# It is read through a const initialised once per process, so every budget needs
# its own suyu launch. That is why this restarts the emulator per point rather
# than retuning a running one.
#
#   .\scripts\bench-chain-budget.ps1
#   .\scripts\bench-chain-budget.ps1 -Budgets 32,512,4096 -Arm nojit -Metric vps
#
# Module lookup and slice dispatch measured 2.3-2.9% of exclusive cycles in the
# ABI 4 profile, so that is roughly the ceiling on what this knob can return.
# Treat a result larger than that as a reason to go looking for the real cause,
# not as a win - a bigger budget also means longer between guard checks and
# fewer chances to notice a fallback, and an arm that quietly stopped running
# statically will look very fast indeed.

[CmdletBinding()]
param(
    [string]$Root    = $env:MK8R_ROOT,
    [string]$Target  = $env:MK8R_TARGET,
    [string]$Rom     = $env:MK8R_ROM,
    [int[]]$Budgets  = @(32, 128, 512, 1024, 2048, 4096, 8192),
    [ValidateSet('hybrid', 'static', 'nojit')][string]$Arm = 'static',
    [int]$Warmup     = 90,
    [int]$Samples    = 60,
    [double]$Interval = 2.0,
    [ValidateSet('fps', 'vps', 'frame_ms')][string]$Metric = 'fps',
    [string]$OutDir  = $(Join-Path $env:TEMP 'mk8r-chain-budget')
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'

foreach ($b in $Budgets) {
    if ($b -lt 1 -or $b -gt 8192) { throw "budget $b is outside the accepted 1..8192 range" }
}
if ($Budgets.Count -ne ($Budgets | Select-Object -Unique).Count) { throw 'duplicate budget requested' }

$Exe      = Join-Path $Root 'build\suyu\bin\suyu.exe'
$NoJitExe = Join-Path $Root 'build\suyu-nojit\bin\suyu.exe'
$RecompIn = Join-Path $Root "build\recomp\$Target"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if ($Arm -eq 'nojit' -and -not (Test-Path -LiteralPath $NoJitExe)) {
    throw "the nojit arm needs $NoJitExe; build it with scripts\build-suyu.ps1 -NoJit"
}
if (-not (Get-ChildItem -LiteralPath $RecompIn -Recurse -Filter '*.dll' -File -ErrorAction SilentlyContinue)) {
    throw "no static module DLLs under $RecompIn"
}

function Set-Unlimited {
    $cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
    if (-not (Test-Path -LiteralPath $cfg)) { return }
    $t = Get-Content -LiteralPath $cfg -Raw
    $t = $t -replace 'log_filter\\default=true', 'log_filter\default=false'
    $t = $t -replace 'log_filter=.*', 'log_filter="*:Info"'
    $t = $t -replace 'use_speed_limit\\default=true', 'use_speed_limit\default=false'
    $t = $t -replace 'use_speed_limit=true', 'use_speed_limit=false'
    $t = $t -replace 'use_vsync\\default=true', 'use_vsync\default=false'
    $t = $t -replace 'use_vsync=\d+', 'use_vsync=0'
    [System.IO.File]::WriteAllText($cfg, $t, (New-Object System.Text.UTF8Encoding $false))
}

function Invoke-Point([int]$budget) {
    # bench-compare keys its arm warnings off the file stem, so keep the arm
    # name in it - a sweep whose points are called "512" would lose the check
    # that they were static at all.
    $name = "$Arm-chain$budget"
    Write-Host ""
    Write-Host "=== $name ===" -ForegroundColor Cyan

    Get-Process suyu* -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 3
    Set-Unlimited

    Remove-Item Env:\SUYU_RECOMP_AUTO -ErrorAction SilentlyContinue
    $env:SUYU_RECOMP_DIR = $RecompIn
    $env:SUYU_RECOMP_CHAIN_BUDGET = [string]$budget
    if ($Arm -eq 'hybrid') {
        Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue
    } else {
        $env:SUYU_RECOMP_STRICT = '1'
    }
    $exe = if ($Arm -eq 'nojit') { $NoJitExe } else { $Exe }

    $p = Start-Process -FilePath $exe -ArgumentList '-hacker' -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) {
        if ((Test-NetConnection 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded) { break }
        Start-Sleep -Seconds 2
    }

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

    python (Join-Path $Root 'scripts\fps-sample.py') $Rom `
        --warmup $Warmup --samples $Samples --interval $Interval `
        --label $name --out (Join-Path $OutDir "$name.json")

    Stop-Job -Job $capture -ErrorAction SilentlyContinue
    Remove-Job -Job $capture -Force -ErrorAction SilentlyContinue

    $null = $p.CloseMainWindow()
    if (-not $p.WaitForExit(120000)) { $p.Kill() }
    Start-Sleep -Seconds 3
}

try {
    foreach ($budget in $Budgets) { Invoke-Point $budget }
} finally {
    Remove-Item Env:\SUYU_RECOMP_CHAIN_BUDGET -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host '=== sweep ===' -ForegroundColor Green
# 32 is the built-in default, so it is the honest denominator when it is in the
# sweep. Otherwise fall back to the first point rather than picking a winner.
$reference = if ($Budgets -contains 32) { "$Arm-chain32" } else { "$Arm-chain$($Budgets[0])" }
$compareArgs = @((Join-Path $Root 'scripts\bench-compare.py'),
                 '--metric', $Metric, '--reference', $reference)
$compareArgs += ($Budgets | ForEach-Object { Join-Path $OutDir "$Arm-chain$_.json" })
python @compareArgs
