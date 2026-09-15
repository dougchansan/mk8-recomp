# Run one strict static title/menu playtest and retain local screenshots/logs.
[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][string]$Rom,
    [int]$RunSeconds = 180,
    [int[]]$CaptureSeconds = @(30, 90, 150),
    [string]$EvidenceRoot = '',
    [string]$CanonicalRoots = '',
    [string]$SuyuExe = '',
    [int]$McpPort = 9742,
    [switch]$AutoBundle,
    [switch]$RequireNoJit,
    [switch]$Baseline,
    [switch]$TasReplay,
    [string]$TasFixtureDirectory = '',
    [string]$TasExpectedFinalImage = '',
    [int]$TasMaxFinalHashDistance = 12,
    [int]$TasExpectedCommands = 0,
    [int]$TasStartDelaySeconds = 0,
    [ValidateRange(0, 180)][int]$TasObserveAfterSeconds = 0,
    [ValidateRange(1, 180)][int]$TasObserveCaptureIntervalSeconds = 30
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'
if ($Baseline -and $RequireNoJit) { throw '-Baseline and -RequireNoJit are mutually exclusive' }
if (-not $TasReplay -and ($TasFixtureDirectory -or $TasObserveAfterSeconds)) {
    throw '-TasFixtureDirectory and -TasObserveAfterSeconds require -TasReplay'
}
$Root = [IO.Path]::GetFullPath($Root)
$Rom = [IO.Path]::GetFullPath($Rom)
$recomp = [IO.Path]::GetFullPath((Join-Path $Root "build\recomp\$Target"))
if (-not $SuyuExe) { $SuyuExe = Join-Path $Root 'build\suyu\bin\suyu.exe' }
$SuyuExe = [IO.Path]::GetFullPath($SuyuExe)
if (-not (Test-Path -LiteralPath $Rom -PathType Leaf)) { throw "ROM does not exist: $Rom" }
if (-not (Test-Path -LiteralPath $SuyuExe -PathType Leaf)) { throw "suyu executable does not exist: $SuyuExe" }
if (-not $Baseline -and -not $AutoBundle -and -not (Get-ChildItem -LiteralPath $recomp -Recurse -Filter '*.dll' -File -ErrorAction SilentlyContinue)) {
    throw "no static module DLLs under $recomp"
}
$otherSuyu = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -like 'suyu*'
})
if ($otherSuyu.Count -gt 0) {
    $owners = ($otherSuyu | ForEach-Object { "$($_.ProcessName) (PID $($_.Id))" }) -join ', '
    throw "another suyu process is running: $owners; refusing a shared-profile playtest"
}
if ((Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded) {
    throw "suyu MCP port $McpPort is already in use; refusing to interfere with another run"
}
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $Root "local\playtest\$Target" }
$stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
$evidence = Join-Path $EvidenceRoot $stamp
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$tasConfig = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
$tasRestore = Join-Path $evidence 'tas-directory-restore.json'
$tasDriver = Join-Path $Root 'scripts\playtest-tas.py'
if ($TasReplay) {
    $describeArgs = @($tasDriver, 'describe-fixture', '--config', $tasConfig)
    if ($TasFixtureDirectory) { $describeArgs += @('--directory', $TasFixtureDirectory) }
    $fixtureJson = (& python @describeArgs | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'could not inspect the selected TAS fixture' }
    $fixture = $fixtureJson | ConvertFrom-Json
    $tasSource = $fixture.directory
    $tasEvidence = Join-Path $evidence 'tas-fixture'
    New-Item -ItemType Directory -Force -Path $tasEvidence | Out-Null
    $tasManifest = @()
    foreach ($tasFile in Get-ChildItem -LiteralPath $tasSource -File -Filter 'script*.txt' `
                                -ErrorAction SilentlyContinue) {
        Copy-Item -LiteralPath $tasFile.FullName -Destination (Join-Path $tasEvidence $tasFile.Name)
        $tasManifest += [ordered]@{
            file = $tasFile.Name
            bytes = $tasFile.Length
            sha256 = (Get-FileHash -LiteralPath $tasFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    $tasManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $tasEvidence 'manifest.json')
    $fixtureJson | Set-Content -LiteralPath (Join-Path $tasEvidence 'source.json') -Encoding utf8
}
$coverage = Join-Path $env:APPDATA 'suyu\log\recomp_coverage.txt'
$suyuLog = Join-Path $env:APPDATA 'suyu\log\suyu_log.txt'
Remove-Item -LiteralPath $coverage -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $suyuLog -Force -ErrorAction SilentlyContinue

$oldDir = $env:SUYU_RECOMP_DIR
$oldStrict = $env:SUYU_RECOMP_STRICT
$oldRecord = $env:SUYU_RECOMP_RECORD_MISSES
$oldMcpPort = $env:SUYU_MCP_PORT
$oldAuto = $env:SUYU_RECOMP_AUTO
$env:SUYU_MCP_PORT = [string]$McpPort
if ($Baseline -or $AutoBundle) {
    Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue
}
if ($Baseline) {
    Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue
    $env:SUYU_RECOMP_AUTO = '0'
} else {
    if (-not $AutoBundle) { $env:SUYU_RECOMP_DIR = $recomp }
    if ($AutoBundle) { $env:SUYU_RECOMP_AUTO = '1' }
    $env:SUYU_RECOMP_STRICT = '1'
}
if (-not $CanonicalRoots) { $CanonicalRoots = Join-Path $EvidenceRoot 'roots' }
$canonicalRoots = $CanonicalRoots
$runRoots = Join-Path $evidence 'recorded-roots'
New-Item -ItemType Directory -Force -Path $canonicalRoots | Out-Null
New-Item -ItemType Directory -Force -Path $runRoots | Out-Null
$env:SUYU_RECOMP_RECORD_MISSES = $runRoots
$process = $null
$runnerExit = -1
try {
    if ($TasReplay) {
        # The driver already supports tas_directory. Point it at this run's
        # immutable input snapshot before the frontend constructs the driver.
        & python $tasDriver select-fixture --config $tasConfig --directory $tasEvidence `
            --restore-file $tasRestore
        if ($LASTEXITCODE -ne 0) { throw 'could not select the TAS fixture directory' }
    }
    $process = Start-Process -FilePath $SuyuExe `
                             -ArgumentList '-hacker' -PassThru
    $deadline = (Get-Date).AddMinutes(2)
    do {
        Start-Sleep -Seconds 2
        if ($process.HasExited) { throw "suyu exited during playtest startup ($($process.ExitCode))" }
        $ready = (Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded
    } until ($ready -or (Get-Date) -ge $deadline)
    if (-not $ready) { throw 'suyu MCP did not come up' }
    if ($TasReplay) {
        $runnerArgs = @((Join-Path $Root 'scripts\playtest-tas.py'), $Rom, $evidence,
                        '--timeout', $RunSeconds, '--fixture-directory', $tasEvidence,
                        '--observe-after-seconds', $TasObserveAfterSeconds,
                        '--observe-capture-interval', $TasObserveCaptureIntervalSeconds)
        if ($Baseline) { $runnerArgs += '--baseline' }
        if ($TasExpectedFinalImage) {
            $runnerArgs += @('--expected-final-image', $TasExpectedFinalImage,
                             '--max-final-hash-distance', $TasMaxFinalHashDistance)
        }
        if ($TasExpectedCommands -gt 0) {
            $runnerArgs += @('--expected-commands', $TasExpectedCommands)
        }
        if ($TasStartDelaySeconds -gt 0) {
            $runnerArgs += @('--start-delay', $TasStartDelaySeconds)
        }
    } else {
        $runnerArgs = @((Join-Path $Root 'scripts\playtest-title.py'), $Rom, $evidence,
                        '--seconds', $RunSeconds, '--capture') + $CaptureSeconds
        if ($Baseline) { $runnerArgs += '--baseline' }
    }
    & python @runnerArgs
    $runnerExit = $LASTEXITCODE
} finally {
    if ($process -and -not $process.HasExited) {
        $null = $process.CloseMainWindow()
        if (-not $process.WaitForExit(60000)) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }
    # Waiting for the process above is the ownership boundary. On the shared
    # workstation another suyu can legitimately bind the same port immediately
    # afterward; treating that new listener as our shutdown failure discards an
    # otherwise complete evidence run. The next run still refuses a busy port
    # before launching.
    if ($null -eq $oldDir) { Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_DIR = $oldDir }
    if ($null -eq $oldStrict) { Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_STRICT = $oldStrict }
    if ($null -eq $oldRecord) { Remove-Item Env:\SUYU_RECOMP_RECORD_MISSES -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_RECORD_MISSES = $oldRecord }
    if ($null -eq $oldMcpPort) { Remove-Item Env:\SUYU_MCP_PORT -ErrorAction SilentlyContinue }
    else { $env:SUYU_MCP_PORT = $oldMcpPort }
    if ($null -eq $oldAuto) { Remove-Item Env:\SUYU_RECOMP_AUTO -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_AUTO = $oldAuto }
    if ($TasReplay -and (Test-Path -LiteralPath $tasRestore)) {
        & python $tasDriver restore-fixture --restore-file $tasRestore
        if ($LASTEXITCODE -ne 0) {
            throw "could not restore the TAS directory; restoration record: $tasRestore"
        }
    }
}

if (-not $Baseline) {
    foreach ($recorded in Get-ChildItem -LiteralPath $runRoots -Filter '*.roots' -File `
                              -ErrorAction SilentlyContinue) {
        $canonical = Join-Path $canonicalRoots $recorded.Name
        $allRoots = @()
        if (Test-Path -LiteralPath $canonical) { $allRoots += Get-Content -LiteralPath $canonical }
        $allRoots += Get-Content -LiteralPath $recorded.FullName
        $allRoots = @($allRoots | ForEach-Object { $_.Trim().ToLowerInvariant() } |
                      Where-Object { $_ -match '^[0-9a-f]+$' } | Sort-Object -Unique)
        Set-Content -LiteralPath $canonical -Value $allRoots -Encoding ascii
    }
}

if (Test-Path -LiteralPath $coverage) { Copy-Item -LiteralPath $coverage -Destination (Join-Path $evidence 'recomp_coverage.txt') }
if (Test-Path -LiteralPath $suyuLog) { Copy-Item -LiteralPath $suyuLog -Destination (Join-Path $evidence 'suyu_log.txt') }
$observationPath = Join-Path $evidence 'observation.json'
if (-not (Test-Path -LiteralPath $observationPath)) { throw 'playtest produced no observation.json' }
$observation = Get-Content -LiteralPath $observationPath -Raw | ConvertFrom-Json
$coverageText = if (Test-Path -LiteralPath $coverage) { Get-Content -LiteralPath $coverage -Raw } else { '' }
$staticBlocks = if ($coverageText -match 'static blocks executed\s*:\s*(\d+)') { [int64]$Matches[1] } else { -1 }
$transitions = if ($coverageText -match 'static -> JIT\s*:\s*(\d+)') { [int64]$Matches[1] } else { -1 }
$guardReady = @($observation.states | Where-Object { $_.guard_v2_ready -eq $true }).Count -gt 0
if (-not $guardReady -and (Test-Path -LiteralPath $suyuLog)) {
    $guardReady = [bool](Select-String -LiteralPath $suyuLog `
        -SimpleMatch 'Recompiled instruction guard-v2: ready' -Quiet)
}
$jitAvailable = @($observation.states | Where-Object { $_.jit_available -eq $true }).Count -gt 0
$stateCount = @($observation.states).Count
$noJitObserved = $stateCount -gt 0 -and
    @($observation.states | Where-Object { $_.jit_available -eq $false }).Count -eq $stateCount
$backendPass = if ($Baseline) { -not $observation.static_backend_active } else {
    $staticBlocks -gt 0 -and $transitions -eq 0 -and $guardReady -and
        (-not $RequireNoJit -or $noJitObserved)
}
$machinePass = $runnerExit -eq 0 -and $observation.machine_status -eq 'REVIEW_REQUIRED' -and $backendPass
$summary = [ordered]@{
    Target = $Target; Mode = if ($Baseline) { 'DYNARMIC' } elseif ($AutoBundle -and $RequireNoJit) {
        'STATIC_AUTO_NO_JIT'
    } elseif ($AutoBundle) {
        'STATIC_AUTO'
    } elseif ($RequireNoJit) {
        'STATIC_NO_JIT'
    } elseif ($TasReplay) {
        'STATIC_STRICT_TAS'
    } else { 'STATIC_STRICT' }
    RomName = [IO.Path]::GetFileName($Rom); Evidence = $evidence
    MachineStatus = if ($machinePass) { 'REVIEW_REQUIRED' } else { 'FAILED' }
    HumanVerdict = 'PENDING'; FirstFrame = [bool]$observation.first_frame
    VisibleContent = [bool]$observation.rendered_visible_content
    StaticBlocks = $staticBlocks; JitTransitions = $transitions; GuardV2 = $guardReady
    JitAvailable = $jitAvailable
    TasOutcome = $observation.tas_outcome
    TasCompletedCommands = $observation.tas_completed_commands
    TasFixtureDirectory = if ($TasReplay) { $tasSource } else { $null }
    FinalImageMatch = $observation.final_image_match
    PostEofOutcome = $observation.post_eof.outcome
    PostEofObservedSeconds = $observation.post_eof.elapsed_seconds
    PostEofScreenshotCount = @($observation.post_eof.screenshots).Count
    ScreenshotCount = @($observation.screenshots).Count; Error = $observation.error
}
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'summary.json') -Encoding utf8
[pscustomobject]$summary
if (-not $machinePass) { exit 1 }
