# Which module's static code is wrong? Run the title with a subset of its
# recompiled images staged and let the JIT cover the rest.
#
# In hybrid mode a guest module with no static image is handled by dynarmic, so
# staging three of four images isolates the fourth. That turns "static is wrong
# somewhere in 300M blocks" into at most four runs.
#
# This only works with fallback ALLOWED. Under SUYU_RECOMP_STRICT a module with
# no image does not fall back - arm_recomp.cpp returns PrefetchAbort and kills
# the thread - so every arm here passes -NoStrict.
#
#   .\scripts\bisect-modules.ps1 -Target title-01006a800016e000 -Rom ...
#
# Smash maps as: main -> cross2_Release.nss (the game), sdk -> nnSdk,
# subsdk0 -> multimedia, rtld -> nnrtld. The reported spin sits in main and
# nnSdk, so those two are the prime suspects.

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][string]$Rom,
    [int]$RunSeconds = 200,
    [int[]]$CaptureSeconds = @(60, 120, 180),
    [string]$OutDir = '',
    # Default sweep: everything static (the control), then each module in turn
    # handed to the JIT. "none" means stage nothing - a pure JIT sanity arm.
    [string[]]$Exclude = @('', 'main', 'sdk', 'subsdk0', 'rtld', 'none')
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'

$RecompIn = Join-Path $Root "build\recomp\$Target"
if (-not (Test-Path -LiteralPath $RecompIn)) { throw "no built modules at $RecompIn" }
$modules = @(Get-ChildItem -LiteralPath $RecompIn -Directory | ForEach-Object { $_.Name })
if (-not $modules) { throw "no module directories under $RecompIn" }
Write-Host "modules present: $($modules -join ', ')"

if (-not $OutDir) { $OutDir = Join-Path $Root "local\bench\modules-$Target" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stage = Join-Path $OutDir '_stage'

$results = @()
foreach ($ex in $Exclude) {
    $label = if ($ex -eq '') { 'all-static' } elseif ($ex -eq 'none') { 'all-jit' } else { "jit-$ex" }
    Write-Host ""
    Write-Host "=== $label ===" -ForegroundColor Cyan

    Get-Process suyu* -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 4

    # playtest-static-title.ps1 derives its recomp directory from -Target and has
    # no override, so each subset is staged under its own synthetic target name.
    # That also avoids editing that script while another agent is working in it.
    $stageTarget = "bisect-$Target-$label"
    $stage = Join-Path $Root "build\recomp\$stageTarget"
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    $staged = @()
    if ($ex -ne 'none') {
        foreach ($m in $modules) {
            if ($m -eq $ex) { continue }
            Copy-Item -LiteralPath (Join-Path $RecompIn $m) -Destination (Join-Path $stage $m) -Recurse
            $staged += $m
        }
    }
    Write-Host "  staged: $(if ($staged) { $staged -join ', ' } else { '(none)' })"

    $evidence = Join-Path $OutDir $label
    $runArgs = @{
        Root = $Root; Rom = $Rom
        RunSeconds = $RunSeconds; CaptureSeconds = $CaptureSeconds
        EvidenceRoot = $evidence
    }
    if ($ex -eq 'none') {
        # No images at all: the DLL presence check would throw, and a pure
        # dynarmic run is exactly what -Baseline already means.
        $runArgs['Target'] = $Target
        $runArgs['Baseline'] = $true
    } else {
        $runArgs['Target'] = $stageTarget
        $runArgs['NoStrict'] = $true
    }
    try {
        & (Join-Path $Root 'scripts\playtest-static-title.ps1') @runArgs *>&1 |
            Tee-Object -FilePath (Join-Path $OutDir "$label.log") | Out-Null
    } catch {
        Write-Host "  threw: $_" -ForegroundColor Yellow
    }

    $run = Get-ChildItem $evidence -Directory -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -match '^\d{8}T\d{6}Z$' } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $run) { Write-Host "  no evidence" -ForegroundColor Yellow; continue }
    $o = Get-Content (Join-Path $run.FullName 'observation.json') -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
    $results += [pscustomobject]@{
        Arm = $label; Staged = ($staged -join '+')
        FirstFrame = $o.first_frame; Visible = $o.rendered_visible_content
        Shots = @($o.screenshots).Count
        Blocks = $o.max_static_blocks; JitTr = $o.max_jit_transitions
        Evidence = $run.Name
    }
}

Write-Host ""
Write-Host '=== module bisection ===' -ForegroundColor Green
$results | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
Write-Host 'FirstFrame=True on an arm means the module handed to the JIT is the faulty one.'
Write-Host 'all-jit must render; if it does not, the harness is wrong, not the recompiler.'
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutDir 'summary.json')
