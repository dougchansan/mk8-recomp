# Run one AOT module subset and report, objectively, whether it renders.
#
# The interesting signal is not "did it boot" but "is there a picture and how
# fast". FPS comes from the MCP server via boot-and-stop.py; the colour count
# comes from a window capture, because suyu reports a first frame as soon as the
# presentation path runs once, whether or not the frame has anything in it.
#
#   .\scripts\bisect-modules.ps1 -Only main,rtld,sdk

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)]
    [string[]]$Only,
    [int]$RunSeconds = 100,
    [int]$ShotAt = 70,
    [string]$ShotDir = $(Join-Path $env:TEMP 'mk8r-bisect')
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'

New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
$tag = ($Only -join '+')
$shot = Join-Path $ShotDir "$tag.png"

# The periodic dump persists across runs; a stale one would be read as this
# run's result.
$covPath = Join-Path $env:APPDATA ('suyu' + [char]92 + 'log' + [char]92 + 'recomp_coverage.txt')
Remove-Item -LiteralPath $covPath -Force -ErrorAction SilentlyContinue

# -ArgumentList splats an array into separate parameters, so a $Only of several
# modules arrives as one string containing commas and run-hybrid rejects it as a
# module name. Pass it joined and split it inside the job.
$run = Start-Job -ScriptBlock {
    param($root, $onlyCsv, $secs)
    & (Join-Path $root 'scripts\run-hybrid.ps1') -Only ($onlyCsv -split ',') -RunSeconds $secs 2>&1
} -ArgumentList $Root, ($Only -join ','), $RunSeconds

Start-Sleep -Seconds $ShotAt
$cap = $null
try {
    $cap = & (Join-Path $Root 'scripts\capture-window.ps1') -Out $shot
} catch {
    Write-Warning "capture failed: $_"
}

$out = Receive-Job -Job $run -Wait -AutoRemoveJob

# Blocks executed is the check that the subset was actually exercised. A run
# that stages a module and then never enters it proves nothing, and the first
# bisect step did exactly that: 126 blocks, so the frames it produced were the
# JIT's work, not the recompiler's.
$cov = Join-Path $env:APPDATA ('suyu' + [char]92 + 'log' + [char]92 + 'recomp_coverage.txt')
$blocks = -1
$transitions = -1
if (Test-Path -LiteralPath $cov) {
    $text = Get-Content -LiteralPath $cov -Raw
    if ($text -match 'static blocks executed\s*:\s*(\d+)') { $blocks = [int64]$Matches[1] }
    if ($text -match 'static -> JIT\s*:\s*(\d+)') { $transitions = [int64]$Matches[1] }
}

[pscustomobject]@{
    Modules     = $tag
    Blocks      = $blocks
    Transitions = $transitions
    Distinct    = if ($cap) { $cap.Distinct } else { -1 }
    Shot        = $shot
}
