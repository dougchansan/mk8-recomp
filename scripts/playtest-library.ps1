# Resumable export/build/strict-render queue for the AArch64 library.
[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [string]$QueueCsv = '',
    [string]$OverridesCsv = '',
    [int]$Limit = 0,
    [string]$StartAtTitleId = '',
    [int]$RunSeconds = 180,
    [int[]]$CaptureSeconds = @(30, 90, 150),
    [switch]$SkipHostBuild,
    [switch]$Force,
    [switch]$ListOnly
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'
if (-not $QueueCsv) { $QueueCsv = Join-Path $Root 'local\library-decode-guarded-final.csv' }
if (-not $OverridesCsv) { $OverridesCsv = Join-Path $Root 'local\playtest-overrides.csv' }
$playtestRoot = Join-Path $Root 'local\playtest'
$matrixPath = Join-Path $playtestRoot 'matrix.csv'
New-Item -ItemType Directory -Force -Path $playtestRoot | Out-Null

$queue = @{}
foreach ($row in Import-Csv -LiteralPath $QueueCsv | Where-Object Status -eq 'GAPS') {
    $queue[$row.TitleId] = [pscustomobject]@{ TitleId = $row.TitleId; Name = $row.Name;
        BootPath = $row.Path; ExportPath = $row.Path }
}
if (Test-Path -LiteralPath $OverridesCsv) {
    foreach ($row in Import-Csv -LiteralPath $OverridesCsv) {
        if ($row.Include -eq 'false') { $queue.Remove($row.TitleId); continue }
        $queue[$row.TitleId] = [pscustomobject]@{ TitleId = $row.TitleId; Name = $row.Name;
            BootPath = $row.BootPath; ExportPath = $row.ExportPath }
    }
}
$items = @($queue.Values | Sort-Object TitleId)
if ($StartAtTitleId) { $items = @($items | Where-Object TitleId -ge $StartAtTitleId) }
if ($Limit -gt 0) { $items = @($items | Select-Object -First $Limit) }
if (-not $items) { throw 'playtest queue is empty' }
if ($ListOnly) {
    $items | Select-Object TitleId,Name,BootPath,ExportPath
    return
}

$existing = @{}
if (Test-Path -LiteralPath $matrixPath) {
    foreach ($row in Import-Csv -LiteralPath $matrixPath) { $existing[$row.TitleId] = $row }
}
if (-not $SkipHostBuild) { & (Join-Path $Root 'scripts\build-suyu.ps1') }

foreach ($item in $items) {
    if (-not $Force -and $existing.ContainsKey($item.TitleId) -and
        $existing[$item.TitleId].MachineStatus -eq 'REVIEW_REQUIRED') {
        Write-Host "skip $($item.TitleId): evidence already captured" -ForegroundColor DarkGray
        continue
    }
    $target = 'title-' + $item.TitleId.ToLowerInvariant()
    $titleRoot = Join-Path $playtestRoot $item.TitleId
    $roots = Join-Path $titleRoot 'roots'
    $emptyRoots = Join-Path $titleRoot 'empty-roots'
    New-Item -ItemType Directory -Force -Path $roots | Out-Null
    New-Item -ItemType Directory -Force -Path $emptyRoots | Out-Null
    $record = [ordered]@{ TitleId = $item.TitleId; Name = $item.Name; Target = $target;
        MachineStatus = 'FAILED'; HumanVerdict = 'PENDING'; StaticBlocks = -1;
        JitTransitions = -1; GuardV2 = $false; ScreenshotCount = 0; Evidence = '';
        BaselineStatus = 'NOT_RUN'; BaselineEvidence = ''; Error = '';
        TimestampUtc = (Get-Date).ToUniversalTime().ToString('o') }
    $exportBuilt = $false
    try {
        Write-Host "`n=== $($item.TitleId) $($item.Name) ===" -ForegroundColor Cyan
        $result = $null
        foreach ($round in 1..8) {
            # ABI-4 images expose every aligned instruction within a discovered
            # block, so fixed NSOs should work without title-specific roots.
            # Keep the recorded-root loop as a fallback for a required zero trap
            # or another exporter omission found by an actual boot.
            $exportRoots = if ($round -eq 1) { $emptyRoots } else { $roots }
            $rootsBefore = (Get-ChildItem -LiteralPath $roots -Filter '*.roots' -File `
                            -ErrorAction SilentlyContinue | Sort-Object Name |
                            ForEach-Object { $_.Name + ':' + (Get-Content $_.FullName -Raw) }) -join "`n"
            & (Join-Path $Root 'scripts\export-static-title.ps1') -Root $Root -Target $target `
              -Rom $item.ExportPath -Roots $exportRoots -SkipHostBuild
            if ($LASTEXITCODE -ne 0) { throw "export/build exited $LASTEXITCODE" }
            $exportBuilt = $true
            & (Join-Path $Root 'scripts\stage-static-title.ps1') -Root $Root -Target $target `
              -TitleId $item.TitleId | Out-Null
            $runOutput = @(& (Join-Path $Root 'scripts\playtest-static-title.ps1') -Root $Root `
                           -Target $target -Rom $item.BootPath -RunSeconds $RunSeconds `
                           -CaptureSeconds $CaptureSeconds -EvidenceRoot $titleRoot `
                           -CanonicalRoots $roots -AutoBundle)
            $runExit = $LASTEXITCODE
            $result = $runOutput | Where-Object {
                $_.PSObject.Properties.Name -contains 'MachineStatus'
            } | Select-Object -Last 1
            if ($runExit -eq 0) { break }
            $rootsAfter = (Get-ChildItem -LiteralPath $roots -Filter '*.roots' -File `
                           -ErrorAction SilentlyContinue | Sort-Object Name |
                           ForEach-Object { $_.Name + ':' + (Get-Content $_.FullName -Raw) }) -join "`n"
            if ($rootsAfter -ne $rootsBefore) {
                Write-Host "new dispatch roots recorded; convergence round $round/8" `
                           -ForegroundColor Yellow
                continue
            }
            throw "strict playtest exited $runExit without recording a new root"
        }
        if (-not $result -or $result.MachineStatus -ne 'REVIEW_REQUIRED') {
            throw 'dispatch did not converge within 8 rounds'
        }
        $record.MachineStatus = $result.MachineStatus
        $record.StaticBlocks = $result.StaticBlocks
        $record.JitTransitions = $result.JitTransitions
        $record.GuardV2 = $result.GuardV2
        $record.ScreenshotCount = $result.ScreenshotCount
        $record.Evidence = $result.Evidence
        $record.Error = $result.Error
    } catch {
        $record.Error = $_.Exception.Message
        Write-Warning $record.Error
        if ($exportBuilt) {
            Write-Host 'static playtest failed; running Dynarmic control' -ForegroundColor Yellow
            $baseline = & (Join-Path $Root 'scripts\playtest-static-title.ps1') -Root $Root `
                         -Target $target -Rom $item.BootPath -RunSeconds $RunSeconds `
                         -CaptureSeconds $CaptureSeconds -EvidenceRoot (Join-Path $titleRoot 'baseline') `
                         -Baseline
            $record.BaselineStatus = $baseline.MachineStatus
            $record.BaselineEvidence = $baseline.Evidence
        }
    }
    $existing[$item.TitleId] = [pscustomobject]$record
    @($existing.Values | Sort-Object TitleId) |
        Export-Csv -LiteralPath $matrixPath -NoTypeInformation -Encoding utf8
}

@($existing.Values | Sort-Object TitleId) | Format-Table TitleId,Name,MachineStatus,
    HumanVerdict,StaticBlocks,JitTransitions,ScreenshotCount -AutoSize
Write-Host "matrix: $matrixPath"
