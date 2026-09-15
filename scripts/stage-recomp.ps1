# Stage a subset of a target's recompiled modules, so a fault can be bisected
# by which images are loaded.
#
# The loader scans a directory tree for module images and loads every one it
# finds, and each module is independent - anything it does not find runs on the
# JIT instead. So loading half of them and seeing whether a fault survives costs
# nothing but a relaunch, and halves the search space each time.
#
#   .\scripts\stage-recomp.ps1 main,rtld
#   $env:SUYU_RECOMP_DIR = <the path it prints>

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string[]]$Modules,
    [string]$Root   = $env:MK8R_ROOT,
    [string]$Target = $env:MK8R_TARGET
)

if (-not $Root)   { $Root = Split-Path -Parent $PSScriptRoot }
if (-not $Target) { $Target = 'mario-kart-8-deluxe' }
$ErrorActionPreference = 'Stop'

$src = Join-Path $Root "build\recomp\$Target"
if (-not (Test-Path -LiteralPath $src)) { throw "no built modules at $src" }

$stage = Join-Path $Root 'build\recomp-stage'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

foreach ($m in $Modules) {
    $from = Join-Path $src $m
    if (-not (Test-Path -LiteralPath $from)) { throw "no such module: $m" }
    $dlls = Get-ChildItem -LiteralPath $from -Recurse -Filter '*.dll'
    if (-not $dlls) { throw "no image built for module $m" }
    $to = Join-Path $stage $m
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    # Hard links, not copies: the main image is ~100 MB and this gets run
    # repeatedly while bisecting.
    foreach ($d in $dlls) {
        New-Item -ItemType HardLink -Path (Join-Path $to $d.Name) -Target $d.FullName | Out-Null
    }
    '{0,-10} {1,12:n0} bytes' -f $m, ($dlls | Measure-Object Length -Sum).Sum
}

Write-Host ''
Write-Host "staged: $stage"
Write-Host 'everything not staged runs on the JIT.'
$stage
