# Report decode coverage for every title in a library.
#
# Whether a title can run with no JIT behind it has two halves: does the emitter
# understand every instruction in its image, and is every block that executes
# actually emitted. The second needs the title to run, and is bounded by what a
# run actually does. This answers the first, which gates the second, and needs
# no boot, no input and no disk.
#
# Uses suyu-cmd --probe-decode-list, which walks XCI -> secure NSP -> Program NCA
# -> ExeFS for each path, decompresses each NSO's .text, runs block discovery,
# and puts every instruction in a discovered block through the emitter. Counting
# discovered blocks rather than sweeping .text linearly matters: a linear sweep
# also decodes literal pools and alignment padding, which are not instructions,
# and would report a gap in every binary ever built.
#
# The emitter is configured as a JIT-free export configures it, so the two
# families that are deliberately left to the JIT in a hybrid build are counted
# as translated here. They would not be a gap for the case being measured.
#
#   .\scripts\survey-decode.ps1

[CmdletBinding()]
param(
    [string]$Root    = $env:MK8R_ROOT,
    [string]$Library = $env:MK8R_LIBRARY,
    [string]$OutCsv,
    # Titles the ISA survey could not open are skipped by default: a dump that
    # does not parse says nothing about the recompiler.
    [switch]$IncludeBroken
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
if (-not $OutCsv) { $OutCsv = Join-Path $Root 'local\library-decode.csv' }

$ErrorActionPreference = 'Stop'

$Probe = Join-Path $Root 'build\suyu\bin\suyu-cmd.exe'
if (-not (Test-Path -LiteralPath $Probe))   { throw "Not built: $Probe" }
if (-not (Test-Path -LiteralPath $Library)) { throw "No library at $Library" }

$roms = Get-ChildItem -LiteralPath $Library -Recurse -File |
        Where-Object { $_.Extension -in '.xci', '.nsp' } |
        Sort-Object FullName

# Skip what the ISA survey already established cannot be opened or cannot be
# translated, so the run is spent on titles where the answer means something.
$isaCsv = Join-Path $Root 'local\library-isa.csv'
if (-not $IncludeBroken -and (Test-Path -LiteralPath $isaCsv)) {
    $skip = @{}
    foreach ($row in Import-Csv -LiteralPath $isaCsv) {
        if ($row.Isa -in 'ERROR') { $skip[$row.Path] = $true }
    }
    $before = $roms.Count
    $roms = $roms | Where-Object { -not $skip.ContainsKey($_.FullName) }
    Write-Host "skipping $($before - $roms.Count) titles the ISA survey could not open" -ForegroundColor DarkGray
}

Write-Host "$($roms.Count) titles to decode" -ForegroundColor Cyan

$listFile = Join-Path $env:TEMP "suyu_decode_list_$PID.txt"
$tsvFile  = Join-Path $env:TEMP "suyu_decode_out_$PID.tsv"
[System.IO.File]::WriteAllLines($listFile, [string[]]$roms.FullName, (New-Object System.Text.UTF8Encoding $false))

$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $Probe -ArgumentList @('--probe-decode-list', "`"$listFile`"", "`"$tsvFile`"") `
        -PassThru -NoNewWindow
while (-not $p.HasExited) {
    Start-Sleep -Seconds 10
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
    if ($f.Count -ge 6) {
        $byPath[$f[5]] = @{ Status = $f[0]; TitleId = $f[1]; Total = [int64]$f[2]
                            Unhandled = [int64]$f[3]; Gaps = $f[4] }
    }
}

$results = foreach ($rom in $roms) {
    $r = $byPath[$rom.FullName]
    $pct = if ($r -and $r.Total -gt 0) {
        [math]::Round(100.0 * ($r.Total - $r.Unhandled) / $r.Total, 4)
    } else { 0 }
    [pscustomobject]@{
        Status       = if ($r) { $r.Status } else { 'MISSING' }
        CoveragePct  = $pct
        Unhandled    = if ($r) { $r.Unhandled } else { '' }
        Instructions = if ($r) { $r.Total } else { '' }
        Name         = $rom.BaseName
        TitleId      = if ($r) { $r.TitleId } else { '' }
        Gaps         = if ($r) { $r.Gaps } else { 'no result line' }
        Path         = $rom.FullName
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutCsv) | Out-Null
$results | Sort-Object Status, @{e='Unhandled';Descending=$true}, Name |
    Export-Csv -LiteralPath $OutCsv -NoTypeInformation -Encoding utf8
Remove-Item -LiteralPath $listFile, $tsvFile -ErrorAction SilentlyContinue

Write-Host ''
Write-Host ("decoded in {0:n1} min" -f $sw.Elapsed.TotalMinutes)
Write-Host "wrote $OutCsv"
$results | Group-Object Status | Sort-Object Count -Descending |
    ForEach-Object { "{0,-8} {1}" -f $_.Name, $_.Count }
