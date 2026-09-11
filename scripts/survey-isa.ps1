# Report the CPU architecture of every title in a library.
#
# suyu's static recompiler is 64-bit-only, so a 32-bit title can never be
# targeted no matter how complete the translator becomes. This answers "which of
# my games could this project ever work on" without booting anything.
#
# Uses suyu-cmd --probe-isa-list (patch 0004), which walks
# XCI -> secure NSP -> Program NCA -> ExeFS -> main.npdm for each path and
# writes a TSV. One process handles the whole library: no Vulkan, no emulation,
# and no crash risk from titles that fail to boot.
#
#   .\scripts\survey-isa.ps1

[CmdletBinding()]
param(
    [string]$Root    = $env:MK8R_ROOT,
    [string]$Library = $env:MK8R_LIBRARY,
    [string]$OutCsv
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

if (-not $OutCsv) { $OutCsv = Join-Path $Root 'local\library-isa.csv' }

$ErrorActionPreference = 'Stop'

$Probe = Join-Path $Root 'build\suyu\bin\suyu-cmd.exe'
if (-not (Test-Path -LiteralPath $Probe))   { throw "Not built: $Probe" }
if (-not (Test-Path -LiteralPath $Library)) { throw "No library at $Library" }

$roms = Get-ChildItem -LiteralPath $Library -Recurse -File |
        Where-Object { $_.Extension -in '.xci', '.nsp' } |
        Sort-Object FullName

Write-Host "$($roms.Count) titles to probe" -ForegroundColor Cyan

$listFile = Join-Path $env:TEMP "suyu_probe_list_$PID.txt"
$tsvFile  = Join-Path $env:TEMP "suyu_probe_out_$PID.tsv"
# Set-Content -Encoding utf8 emits a BOM on PowerShell 5.1, which would prepend
# three bytes to the first path and make that one title fail to open.
[System.IO.File]::WriteAllLines($listFile, [string[]]$roms.FullName, (New-Object System.Text.UTF8Encoding $false))

$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Probe -ArgumentList @('--probe-isa-list', "`"$listFile`"", "`"$tsvFile`"") `
        -PassThru -NoNewWindow
while (-not $p.HasExited) {
    Start-Sleep -Seconds 5
    $done = 0
    if (Test-Path -LiteralPath $tsvFile) {
        $done = @(Get-Content -LiteralPath $tsvFile -ErrorAction SilentlyContinue).Count
    }
    Write-Host ("  {0}/{1} after {2:n0}s" -f $done, $roms.Count, $sw.Elapsed.TotalSeconds)
}
$sw.Stop()

$byPath = @{}
foreach ($line in Get-Content -LiteralPath $tsvFile) {
    $f = $line -split "`t"
    if ($f.Count -ge 4) { $byPath[$f[3]] = @{ Isa = $f[0]; TitleId = $f[1]; Note = $f[2] } }
}

$results = foreach ($rom in $roms) {
    $r = $byPath[$rom.FullName]
    [pscustomobject]@{
        Isa     = if ($r) { $r.Isa }     else { 'MISSING' }
        TitleId = if ($r) { $r.TitleId } else { '' }
        Name    = $rom.BaseName
        SizeGB  = [math]::Round($rom.Length / 1GB, 2)
        Note    = if ($r) { $r.Note } else { 'no result line' }
        Path    = $rom.FullName
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutCsv) | Out-Null
$results | Sort-Object Isa, Name | Export-Csv -LiteralPath $OutCsv -NoTypeInformation -Encoding utf8
Remove-Item -LiteralPath $listFile, $tsvFile -ErrorAction SilentlyContinue

Write-Host ''
$results | Group-Object Isa | Sort-Object Count -Descending |
    ForEach-Object { '{0,-8} {1}' -f $_.Name, $_.Count }
Write-Host ''
Write-Host ("probed in {0:n1} min" -f $sw.Elapsed.TotalMinutes)
Write-Host "wrote $OutCsv" -ForegroundColor Green
