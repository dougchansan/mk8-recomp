# Record a TAS from boot for a declared functional test path.
#
# Savestates are not implemented in this emulator - CreateSaveState and
# LoadSaveState are stubs that return false - and implementing them means
# capturing kernel objects mid-IPC and GPU state in flight, which is why yuzu
# and Ryujinx never shipped them either. Replaying inputs from boot gets the
# same thing for benchmarking purposes and the machinery already exists.
#
#   .\scripts\tas-record.ps1            # record a new script
#   .\scripts\tas-record.ps1 -Baseline  # record on dynarmic instead
#   .\scripts\tas-record.ps1 -RequireNoJit -TasFixtureDirectory local\tas-fixtures\static-new
#
# Recording is armed before the emulation thread starts. Frame zero is the
# first displayed game frame, which is also where boot-synchronized playback
# will consume frame zero. Keep the controller neutral until the game appears,
# play through L+R and into a race, then press Ctrl+F7 once to stop and save.
#
# Use a fresh -TasFixtureDirectory for a backend-specific functional replay.
# Otherwise the script lands in the configured TAS directory, after a backup.
# Different timing profiles must not be compared as fixed-work benchmarks.

[CmdletBinding()]
param(
    [string]$Root   = $env:MK8R_ROOT,
    [string]$Target = $env:MK8R_TARGET,
    [string]$Rom    = $env:MK8R_ROM,
    [switch]$Baseline,
    [switch]$RequireNoJit,
    [string]$SuyuExe = '',
    [string]$TasFixtureDirectory = '',
    [int]$McpPort = 9742
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'
if ($Baseline -and $RequireNoJit) { throw '-Baseline and -RequireNoJit are mutually exclusive' }

$Exe = if ($SuyuExe) { [IO.Path]::GetFullPath($SuyuExe) } elseif ($RequireNoJit) {
    Join-Path $Root 'build\suyu-nojit\bin\suyu.exe'
} else { Join-Path $Root 'build\suyu\bin\suyu.exe' }
$RecompIn = Join-Path $Root "build\recomp\$Target"
$TasDriver = Join-Path $Root 'scripts\playtest-tas.py'
$cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
$TasDir = if ($TasFixtureDirectory) { [IO.Path]::GetFullPath($TasFixtureDirectory) } else {
    # Read the same tas_directory setting used by the input driver. A fresh
    # profile can still record its first script before describe-fixture works.
    $configText = Get-Content -LiteralPath $cfg -Raw
    if ($configText -match '(?m)^tas_directory=(.+)') {
        $Matches[1].Trim().Trim('"')
    } else { Join-Path $env:APPDATA 'suyu\tas' }
}
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) { throw "suyu executable does not exist: $Exe" }
if (-not (Test-Path -LiteralPath $Rom -PathType Leaf)) { throw "ROM does not exist: $Rom" }
if (-not $Baseline -and -not (Get-ChildItem -LiteralPath $RecompIn -Recurse -File -Filter '*.dll' -ErrorAction SilentlyContinue)) {
    throw "no static modules under $RecompIn"
}
if ($TasFixtureDirectory -and (Get-ChildItem -LiteralPath $TasDir -File -Filter 'script*.txt' -ErrorAction SilentlyContinue)) {
    throw '-TasFixtureDirectory must be a fresh output directory; existing fixtures are preserved'
}
New-Item -ItemType Directory -Force -Path $TasDir | Out-Null

$otherSuyu = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -like 'suyu*'
})
if ($otherSuyu.Count -gt 0) {
    $owners = ($otherSuyu | ForEach-Object { "$($_.ProcessName) (PID $($_.Id))" }) -join ', '
    throw "another suyu process is running: $owners; close it before recording"
}

if ((Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded) {
    throw "suyu MCP port $McpPort is already in use"
}
$stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssffffZ')
$backupDir = Join-Path $Root "local\tas-archive\$stamp"
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$oldScripts = @(Get-ChildItem -LiteralPath $TasDir -File -Filter 'script*.txt')
$backupManifest = foreach ($script in $oldScripts) {
    Copy-Item -LiteralPath $script.FullName -Destination (Join-Path $backupDir $script.Name)
    [ordered]@{
        source = $script.FullName
        sha256 = (Get-FileHash -LiteralPath $script.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
ConvertTo-Json -InputObject @($backupManifest) | Set-Content -LiteralPath (Join-Path $backupDir 'manifest.json')
if ($oldScripts.Count) {
    Write-Host "Backed up the previous scripts to $backupDir" -ForegroundColor DarkGray
}
$tasRestore = Join-Path $backupDir 'tas-directory-restore.json'

# tas_enable defaults to false, so without this the hotkeys do nothing at all
# and it looks like the feature is missing. The explicit boot recording mode
# controls the start boundary independently of pause_tas_on_load.
if (Test-Path -LiteralPath $cfg) {
    $t = Get-Content -LiteralPath $cfg -Raw
    $t = $t -replace 'tas_enable\\default=true', 'tas_enable\default=false'
    $t = $t -replace 'tas_enable=false', 'tas_enable=true'
    # Recording at an uncapped frame rate produces a script whose timing cannot
    # be reproduced, so record at the normal 60 FPS.
    $t = $t -replace 'use_speed_limit\\default=true', 'use_speed_limit\default=false'
    $t = $t -replace 'use_speed_limit=false', 'use_speed_limit=true'
    [System.IO.File]::WriteAllText($cfg, $t, (New-Object System.Text.UTF8Encoding $false))
    Write-Host 'tas_enable=true, frame limiter on for recording' -ForegroundColor Green
}

$oldDir = $env:SUYU_RECOMP_DIR
$oldStrict = $env:SUYU_RECOMP_STRICT
$oldAuto = $env:SUYU_RECOMP_AUTO
$oldMcpPort = $env:SUYU_MCP_PORT
$p = $null
try {
    $env:SUYU_MCP_PORT = [string]$McpPort
    if ($TasFixtureDirectory) {
        & python $TasDriver select-fixture --config $cfg --directory $TasDir --restore-file $tasRestore
        if ($LASTEXITCODE -ne 0) { throw 'could not select the recording fixture directory' }
    }
    if ($Baseline) {
        Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue
        Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue
        $env:SUYU_RECOMP_AUTO = '0'
        Write-Host 'recording on dynarmic (no AOT modules)' -ForegroundColor Yellow
    } else {
        $env:SUYU_RECOMP_DIR = $RecompIn
        if ($RequireNoJit) { $env:SUYU_RECOMP_STRICT = '1' }
        else { Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue }
        Write-Host "recording with AOT modules from $RecompIn" -ForegroundColor Green
    }

    $p = Start-Process -FilePath $Exe -ArgumentList '-hacker' -PassThru
    Write-Host "suyu pid $($p.Id)"

    $deadline = (Get-Date).AddSeconds(120)
    while ((Get-Date) -lt $deadline) {
        if ($p.HasExited) { throw "suyu exited during recording startup ($($p.ExitCode))" }
        if ((Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded) { break }
        Start-Sleep -Seconds 2
    }

    # Errors are deliberately not swallowed here. The first version piped this to
    # Out-Null and the launch failed silently: the script printed its instructions,
    # the game never booted, and there was nothing to say so.
    python (Join-Path $Root 'scripts\mcp-call.py') launch_game_path --path $Rom --tas_mode record
    if ($LASTEXITCODE -ne 0) { throw "launch_game_path failed (exit $LASTEXITCODE)" }
    $statusJson = (& python (Join-Path $Root 'scripts\mcp-call.py') get_emulator_state | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "get_emulator_state failed (exit $LASTEXITCODE)" }
    $status = $statusJson | ConvertFrom-Json
    if (-not $status.tas_recording) { throw 'TAS recording was not armed at the boot boundary' }
    if ($RequireNoJit -and $status.jit_available -ne $false) {
        throw '-RequireNoJit needs a host reporting jit_available=false'
    }
    Write-Host ''
    Write-Host 'Boot-synchronized recording is active.' -ForegroundColor Cyan
    Write-Host '  1. keep controls neutral until the first game image appears'
    Write-Host '  2. follow the intended test path, leaving time for this backend to load'
    Write-Host '  3. Ctrl+F7 once to stop, then answer Yes to overwrite player 1'
    Write-Host ''
    Write-Host "Script will be written to $TasDir" -ForegroundColor Cyan
    if ($TasFixtureDirectory) {
        Write-Host 'Close suyu after saving; this helper then restores the previous TAS directory.'
        $p.WaitForExit()
        if (Test-Path -LiteralPath (Join-Path $TasDir 'script0-1.txt')) {
            $fixtureJson = (& python $TasDriver describe-fixture --directory $TasDir | Out-String)
            if ($LASTEXITCODE -ne 0) { throw 'could not record fixture provenance' }
            [ordered]@{
                mode = if ($Baseline) { 'DYNARMIC' } elseif ($RequireNoJit) { 'STATIC_NO_JIT' } else { 'HYBRID' }
                recorded_from_boot = $true
                functional_scope = 'UNREVIEWED; label the milestones reached before accepting'
                scripts = ($fixtureJson | ConvertFrom-Json).scripts
            } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $TasDir 'recording.json') -Encoding utf8
        }
    }
} finally {
    $canRestore = $true
    if ($TasFixtureDirectory -and $p -and -not $p.HasExited) {
        $null = $p.CloseMainWindow()
        $canRestore = $p.WaitForExit(60000)
    }
    if ($null -eq $oldDir) { Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_DIR = $oldDir }
    if ($null -eq $oldStrict) { Remove-Item Env:\SUYU_RECOMP_STRICT -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_STRICT = $oldStrict }
    if ($null -eq $oldAuto) { Remove-Item Env:\SUYU_RECOMP_AUTO -ErrorAction SilentlyContinue }
    else { $env:SUYU_RECOMP_AUTO = $oldAuto }
    if ($null -eq $oldMcpPort) { Remove-Item Env:\SUYU_MCP_PORT -ErrorAction SilentlyContinue }
    else { $env:SUYU_MCP_PORT = $oldMcpPort }
    if (-not $canRestore) { throw "close suyu before restoring TAS directory from $tasRestore" }
    if ($TasFixtureDirectory -and (Test-Path -LiteralPath $tasRestore)) {
        & python $TasDriver restore-fixture --restore-file $tasRestore
        if ($LASTEXITCODE -ne 0) { throw "could not restore TAS directory; use record $tasRestore" }
    }
}
