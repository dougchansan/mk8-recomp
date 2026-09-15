# Matched CPU-backend comparison across any number of arms.
#
# Every arm runs identically: same ROM, same warmup, same sample count, same
# interval, frame limiter and vsync off. That matters more than it sounds -
# earlier single-screenshot readings gave 473 FPS hybrid against 522 baseline,
# which was meaningless, because the two captures landed on different parts of
# the attract sequence. The title screen and the demo race are very different
# workloads and the distribution is bimodal, so a single number cannot compare
# two builds.
#
# Sample long enough to cover several attract cycles in every arm, then compare
# percentiles rather than means.
#
#   .\scripts\bench-ab.ps1                                   # hybrid, baseline
#   .\scripts\bench-ab.ps1 -Arms hybrid,baseline,static
#   .\scripts\bench-ab.ps1 -Arms static,nojit -Metric vps
#
# The arms differ only in how the recompiler is selected:
#
#   baseline  no SUYU_RECOMP_DIR and SUYU_RECOMP_AUTO=0 - Dynarmic only.
#   hybrid    SUYU_RECOMP_DIR set - static code with JIT fallback allowed.
#   static    the same, plus SUYU_RECOMP_STRICT=1 - fallback refused.
#   nojit     the same again, against build\suyu-nojit\bin\suyu.exe, which has
#             no Dynarmic linked into it at all.
#
# static and nojit are not the same claim and are deliberately separate arms.
# Strict mode refuses the fallback inside a host that still contains a JIT;
# only the nojit binary proves the JIT was absent. The campaign has already
# published a withdrawn result that conflated the two.

[CmdletBinding()]
param(
    [string]$Root    = $env:MK8R_ROOT,
    [string]$Target  = $env:MK8R_TARGET,
    [string]$Rom     = $env:MK8R_ROM,
    [int]$Warmup     = 90,
    [int]$Samples    = 60,
    [double]$Interval = 2.0,
    [string]$OutDir  = $(Join-Path $env:TEMP 'mk8r-bench'),
    [string[]]$Arms  = @('hybrid', 'baseline'),
    [ValidateSet('fps', 'vps', 'frame_ms')][string]$Metric = 'fps',
    [string]$Reference = '',
    # capture-window refuses to grab the screen region unless suyu is frontmost,
    # because another window on top would be recorded instead. A benchmark runs
    # unattended, so it brings suyu forward rather than silently collecting no
    # evidence - the MK8 three-arm run produced zero screenshots that way, and
    # with no images there was no way to tell what the static arm was drawing.
    # Pass -ActivateCapture:$false when a human is using the machine.
    [bool]$ActivateCapture = $true,
    # A/B two emulator builds on the same arms and the same workload. -Tag keeps
    # their sample files apart so both rounds can be handed to bench-compare at
    # once; the arm name stays in the stem, so the liveness warnings still fire.
    [string]$SuyuExe = '',
    [string]$Tag = '',
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

if ($Tas) {
    throw '-Tas is retired: boot-synchronized replays are fixed-work EOF tests; use playtest-static-title.ps1 -TasReplay'
}

# -Arms hybrid,baseline arrives as one comma-joined element under -File.
$Arms = @($Arms | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } |
          Where-Object { $_ })
$known = @('baseline', 'hybrid', 'static', 'nojit')
foreach ($arm in $Arms) {
    if ($known -notcontains $arm) { throw "unknown arm '$arm'; expected one of $($known -join ', ')" }
}
if ($Arms.Count -lt 1) { throw 'no arms selected' }
if ($Arms.Count -ne ($Arms | Select-Object -Unique).Count) { throw 'duplicate arm requested' }

$Exe      = if ($SuyuExe) { [IO.Path]::GetFullPath($SuyuExe) }
            else { Join-Path $Root 'build\suyu\bin\suyu.exe' }
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "suyu executable does not exist: $Exe" }
$NoJitExe = Join-Path $Root 'build\suyu-nojit\bin\suyu.exe'
$RecompIn = Join-Path $Root "build\recomp\$Target"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if ($Arms -contains 'nojit' -and -not (Test-Path -LiteralPath $NoJitExe)) {
    throw "the nojit arm needs $NoJitExe; build it with scripts\build-suyu.ps1 -NoJit"
}
if (($Arms | Where-Object { $_ -ne 'baseline' }) -and
    -not (Get-ChildItem -LiteralPath $RecompIn -Recurse -Filter '*.dll' -File -ErrorAction SilentlyContinue)) {
    throw "no static module DLLs under $RecompIn"
}

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

function Invoke-Arm([string]$name) {
    Write-Host ""
    Write-Host "=== $name ===" -ForegroundColor Cyan

    Get-Process suyu* -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 3

    # suyu rewrites the config on exit, so this is re-applied per arm.
    Set-Unlimited

    # Clear every knob first. Leaving one set from the previous arm is the
    # failure that produces a "baseline" which quietly ran static code.
    Remove-Item Env:\SUYU_RECOMP_DIR    -ErrorAction SilentlyContinue
    Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue
    Remove-Item Env:\SUYU_RECOMP_AUTO   -ErrorAction SilentlyContinue

    $exe = $Exe
    switch ($name) {
        'baseline' { $env:SUYU_RECOMP_AUTO = '0' }
        'hybrid'   { $env:SUYU_RECOMP_DIR = $RecompIn }
        'static'   { $env:SUYU_RECOMP_DIR = $RecompIn; $env:SUYU_RECOMP_STRICT = '1' }
        'nojit'    { $env:SUYU_RECOMP_DIR = $RecompIn; $env:SUYU_RECOMP_STRICT = '1'
                     $exe = $NoJitExe }
    }

    $p = Start-Process -FilePath $exe -ArgumentList '-hacker' -PassThru
    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) {
        if ((Test-NetConnection 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded) { break }
        Start-Sleep -Seconds 2
    }

    $tagged = if ($Tag) { "$name-$Tag" } else { $name }
    $json = Join-Path $OutDir "$tagged.json"

    # Capture the screen periodically while the arm runs. FPS alone cannot tell
    # a fast build from a fast *and correct* one - a rendering regression shows
    # up as a higher frame rate, not a lower one - so every arm leaves behind
    # evidence of what it actually drew.
    $shots = Join-Path $OutDir "shots-$tagged"
    New-Item -ItemType Directory -Force -Path $shots | Out-Null
    # First capture lands inside the warmup rather than after it. A title that
    # never presents a frame ends the arm with no samples, and the old timing
    # meant that case also produced no screenshots - no evidence at exactly the
    # moment the evidence decides what went wrong.
    $capture = Start-Job -ScriptBlock {
        param($root, $dir, $warmup, $count, $gap, $activate)
        Start-Sleep -Seconds ([Math]::Min(30, $warmup))
        for ($i = 1; $i -le $count; $i++) {
            try {
                & (Join-Path $root 'scripts\capture-window.ps1') `
                    -Out (Join-Path $dir ('{0:d2}.png' -f $i)) -Activate:$activate | Out-Null
            } catch {
                "capture $i failed: $_" | Out-File -Append -Encoding utf8 `
                    -FilePath (Join-Path $dir 'capture-errors.txt')
            }
            Start-Sleep -Seconds $gap
        }
    } -ArgumentList $Root, $shots, $Warmup, 8, 30, $ActivateCapture

    python (Join-Path $Root 'scripts\fps-sample.py') $Rom `
        --warmup $Warmup --samples $Samples --interval $Interval --label $tagged --out $json
    $sampled = ($LASTEXITCODE -eq 0)

    Stop-Job -Job $capture -ErrorAction SilentlyContinue
    Remove-Job -Job $capture -Force -ErrorAction SilentlyContinue

    $null = $p.CloseMainWindow()
    if (-not $p.WaitForExit(120000)) { $p.Kill() }
    Start-Sleep -Seconds 3
    return $sampled
}

$measured = @()
foreach ($arm in $Arms) {
    if (Invoke-Arm $arm) { $measured += $arm }
    else { Write-Host "arm '$arm' produced no usable samples; excluded from the comparison" -ForegroundColor Yellow }
}

if ($measured.Count -eq 0) {
    throw 'no arm produced usable samples; see the per-arm json and screenshots for why'
}
if ($measured.Count -lt $Arms.Count) {
    # Comparing the arms that happened to work is how a partial run gets quoted
    # as a complete one. Say plainly which arms are missing.
    Write-Host "incomplete: $(($Arms | Where-Object { $measured -notcontains $_ }) -join ', ') missing" -ForegroundColor Yellow
}

Write-Host ""
Write-Host '=== comparison ===' -ForegroundColor Green
$compareArgs = @((Join-Path $Root 'scripts\bench-compare.py'), '--metric', $Metric)
if (-not $Reference) { $Reference = if ($measured -contains 'baseline') { 'baseline' } else { $measured[-1] } }
# Membership is checked against the bare arm names; the tag is a filename
# suffix and is applied only afterwards.
if ($measured -notcontains $Reference) { throw "reference arm '$Reference' produced no usable samples" }
if ($Tag) { $Reference = "$Reference-$Tag" }
$compareArgs += @('--reference', $Reference)
$compareArgs += ($measured | ForEach-Object { Join-Path $OutDir $(if ($Tag) { "$_-$Tag.json" } else { "$_.json" }) })
python @compareArgs
