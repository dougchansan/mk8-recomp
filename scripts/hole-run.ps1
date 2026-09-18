# One arm of the intra-module bisection for title-01006a800016e000's `main`.
#
# `main`'s generated lookup unit (recompiled_main.c) reads
# SUYU_RECOMP_MAIN_HOLE="lo-hi[,lo-hi]..." and zeroes those module-relative
# ranges out of its block index. Every route into a static block goes through
# that index or through recomp_lookup(), and generated blocks never call one
# another by symbol, so a hole hands exactly that address range to the JIT -
# the module bisection one level down.
#
#   .\scripts\hole-run.ps1 -Label lo-half -Hole 0x0-0x17fe11e
#   .\scripts\hole-run.ps1 -Label none -Hole ''

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Label,
    [string]$Hole = '',
    [string]$Root = 'G:\mk8-recomp',
    [int]$RunSeconds = 600
)

$ErrorActionPreference = 'Stop'

Get-Process suyu* -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 8

if ($Hole) { $env:SUYU_RECOMP_MAIN_HOLE = $Hole }
else { Remove-Item Env:\SUYU_RECOMP_MAIN_HOLE -ErrorAction SilentlyContinue }

$out = Join-Path $Root "local\bench\hole-$Label"
& (Join-Path $Root 'scripts\bench-tas.ps1') `
    -Target title-01006a800016e000 `
    -Rom "F:\Emulation\MIG Switch Game Collection\Super Smash Bros Ultimate.xci\Super Smash Bros Ultimate.xci" `
    -TasFixtureDirectory (Join-Path $Root 'local\tas-fixtures\01006A800016E000-baseline') `
    -Arms static -NoStrict -RunSeconds $RunSeconds -OutDir $out *>&1 |
    Out-File -Encoding utf8 (Join-Path $Root "local\bench\hole-$Label-console.log")

$run = Get-ChildItem (Join-Path $out 'static') -Directory -ErrorAction SilentlyContinue |
       Where-Object { $_.Name -match '^\d{8}T\d{6}Z$' } |
       Sort-Object Name -Descending | Select-Object -First 1
if (-not $run) { Write-Host "no evidence for $Label"; exit 1 }
Write-Host "evidence: $($run.FullName)"
Get-ChildItem $run.FullName -Filter 'frame-*.png' | ForEach-Object { Write-Host ("  {0}  {1} bytes" -f $_.Name, $_.Length) }
Get-Content (Join-Path $out 'summary.json') -Raw | Write-Host
