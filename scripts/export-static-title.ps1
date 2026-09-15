# Export and build every AOT module for one title, with explicit completion checks.
[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][string]$Rom,
    [string]$Roots = '',
    [string]$SuyuExe = '',
    [int]$McpPort = 9742,
    [switch]$SkipHostBuild,
    [int]$ExportTimeoutMinutes = 45
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath($Root)
$Rom = [IO.Path]::GetFullPath($Rom)
if (-not $SuyuExe) { $SuyuExe = Join-Path $Root 'build\suyu\bin\suyu.exe' }
$SuyuExe = [IO.Path]::GetFullPath($SuyuExe)
$gen = [IO.Path]::GetFullPath((Join-Path $Root "generated\$Target"))
$build = [IO.Path]::GetFullPath((Join-Path $Root "build\recomp\$Target"))
$generatedRoot = [IO.Path]::GetFullPath((Join-Path $Root 'generated')) + [IO.Path]::DirectorySeparatorChar
$buildRoot = [IO.Path]::GetFullPath((Join-Path $Root 'build\recomp')) + [IO.Path]::DirectorySeparatorChar
if (-not $gen.StartsWith($generatedRoot, [StringComparison]::OrdinalIgnoreCase) -or
    -not $build.StartsWith($buildRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Target resolved outside the generated/build roots'
}
if (-not (Test-Path -LiteralPath $Rom -PathType Leaf)) { throw "ROM does not exist: $Rom" }
if (-not (Test-Path -LiteralPath $SuyuExe -PathType Leaf)) { throw "suyu executable does not exist: $SuyuExe" }
if ((Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded) {
    throw "suyu MCP port $McpPort is already in use; refusing to interfere with another run"
}
if (-not $SkipHostBuild) { & (Join-Path $Root 'scripts\build-suyu.ps1') }

function Invoke-Mcp([string]$Name, [hashtable]$Arguments = @{}) {
    $argv = @((Join-Path $Root 'scripts\mcp-call.py'), $Name)
    foreach ($key in $Arguments.Keys) { $argv += "--$key"; $argv += [string]$Arguments[$key] }
    $raw = & python @argv
    if ($LASTEXITCODE -ne 0) { throw "MCP $Name failed: $($raw -join [Environment]::NewLine)" }
    ($raw -join [Environment]::NewLine) | ConvertFrom-Json
}

$previousGen = "$gen.previous-$PID"
if (Test-Path -LiteralPath $previousGen) { throw "previous-export path already exists: $previousGen" }
if (Test-Path -LiteralPath $gen) { Move-Item -LiteralPath $gen -Destination $previousGen }
New-Item -ItemType Directory -Force -Path (Join-Path $gen 'out') | Out-Null

$oldTranslate = $env:SUYU_AOT_TRANSLATE_ALL
$oldRoots = $env:SUYU_AOT_EXTRA_ROOTS
$oldMcpPort = $env:SUYU_MCP_PORT
$env:SUYU_MCP_PORT = [string]$McpPort
$env:SUYU_AOT_TRANSLATE_ALL = '1'
if ($Roots -and (Test-Path -LiteralPath $Roots)) { $env:SUYU_AOT_EXTRA_ROOTS = $Roots }
else { Remove-Item Env:\SUYU_AOT_EXTRA_ROOTS -ErrorAction SilentlyContinue }
$process = $null
try {
    $process = Start-Process -FilePath $SuyuExe `
                             -ArgumentList '-hacker' -PassThru
    $deadline = (Get-Date).AddMinutes(2)
    do {
        Start-Sleep -Seconds 2
        if ($process.HasExited) { throw "suyu exited during export startup ($($process.ExitCode))" }
        $ready = (Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded
    } until ($ready -or (Get-Date) -ge $deadline)
    if (-not $ready) { throw 'suyu MCP did not come up' }

    # Opening the modal intentionally never returns to its original RPC. The
    # export driver uses a short timeout for that first call, then the async
    # test action returns immediately and status is polled below.
    & python (Join-Path $Root 'scripts\export-recomp.py') --rom $Rom `
      --out (Join-Path $gen 'out') source
    if ($LASTEXITCODE -ne 0) { throw "export driver exited $LASTEXITCODE" }
    $deadline = (Get-Date).AddMinutes($ExportTimeoutMinutes)
    do {
        Start-Sleep -Seconds 10
        $status = Invoke-Mcp get_aot_export_status
        Write-Host ("export {0,3}%  {1}" -f $status.progress, $status.status)
        if ($process.HasExited) { throw "suyu exited during export ($($process.ExitCode))" }
    } until ($status.done -or (Get-Date) -ge $deadline)
    if (-not $status.done) { throw "export timed out after $ExportTimeoutMinutes minutes" }
    if (-not $status.success) { throw "export failed: $($status.status)" }
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $portDeadline = (Get-Date).AddSeconds(30)
    do {
        $portBusy = (Test-NetConnection 127.0.0.1 -Port $McpPort -WarningAction SilentlyContinue).TcpTestSucceeded
        if ($portBusy) { Start-Sleep -Milliseconds 500 }
    } until (-not $portBusy -or (Get-Date) -ge $portDeadline)
    if ($portBusy) { throw 'suyu MCP port 9742 remained in use after exporter shutdown' }
    if ($null -eq $oldTranslate) { Remove-Item Env:\SUYU_AOT_TRANSLATE_ALL -ErrorAction SilentlyContinue }
    else { $env:SUYU_AOT_TRANSLATE_ALL = $oldTranslate }
    if ($null -eq $oldRoots) { Remove-Item Env:\SUYU_AOT_EXTRA_ROOTS -ErrorAction SilentlyContinue }
    else { $env:SUYU_AOT_EXTRA_ROOTS = $oldRoots }
    if ($null -eq $oldMcpPort) { Remove-Item Env:\SUYU_MCP_PORT -ErrorAction SilentlyContinue }
    else { $env:SUYU_MCP_PORT = $oldMcpPort }
}

$cache = Get-ChildItem -LiteralPath $gen -Recurse -Directory -Filter aot_cache |
         Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'aot_manifest.json') } |
         Select-Object -First 1
if (-not $cache) { throw "export reported success but no AOT manifest exists under $gen" }

# Export rewrites every source file. Preserve the old timestamp for byte-identical
# files so Ninja recompiles only units changed by newly discovered roots.
if (Test-Path -LiteralPath $previousGen) {
    $previousCache = Get-ChildItem -LiteralPath $previousGen -Recurse -Directory -Filter aot_cache |
                     Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'aot_manifest.json') } |
                     Select-Object -First 1
    if ($previousCache) {
        foreach ($newFile in Get-ChildItem -LiteralPath $cache.FullName -Recurse -File) {
            $relative = $newFile.FullName.Substring($cache.FullName.Length + 1)
            $oldFile = Join-Path $previousCache.FullName $relative
            if ((Test-Path -LiteralPath $oldFile -PathType Leaf) -and
                (Get-Item -LiteralPath $oldFile).Length -eq $newFile.Length -and
                (Get-FileHash -LiteralPath $oldFile -Algorithm SHA256).Hash -eq
                (Get-FileHash -LiteralPath $newFile.FullName -Algorithm SHA256).Hash) {
                $newFile.LastWriteTimeUtc = (Get-Item -LiteralPath $oldFile).LastWriteTimeUtc
            }
        }
    }
}
$exefs = Join-Path $cache.FullName 'exefs'
$modules = Get-ChildItem -LiteralPath $exefs -Directory |
           Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'CMakeLists.txt') } |
           Sort-Object Name
if (-not $modules) { throw "no generated modules under $exefs" }
$package = $cache.Parent.FullName.Substring($gen.Length + 1)
function Get-ModuleDigest([string]$Path) {
    $lines = Get-ChildItem -LiteralPath $Path -Recurse -File |
             Sort-Object FullName |
             ForEach-Object {
                 $relative = $_.FullName.Substring($Path.Length).Replace([IO.Path]::DirectorySeparatorChar, '/')
                 "$relative $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
             }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try { [Convert]::ToHexString($sha.ComputeHash($bytes)) } finally { $sha.Dispose() }
}
$emitterTime = (Get-Item -LiteralPath (Join-Path $Root 'third_party\suyu\src\core\recompiler\arm64_to_c.h')).LastWriteTime
foreach ($module in $modules) {
    $moduleBuild = Join-Path $build $module.Name
    $digestPath = Join-Path $moduleBuild '.source-sha256'
    $digest = Get-ModuleDigest $module.FullName
    $builtDll = Get-ChildItem -LiteralPath $moduleBuild -Recurse -Filter '*.dll' -File `
                -ErrorAction SilentlyContinue | Select-Object -First 1
    $previousDigest = if (Test-Path -LiteralPath $digestPath) {
        (Get-Content -LiteralPath $digestPath -Raw).Trim()
    } else { '' }
    if ($builtDll -and $previousDigest -eq $digest -and
        $builtDll.LastWriteTime -ge $emitterTime) {
        Write-Host "module $($module.Name): generated source unchanged, keeping existing DLL" `
                   -ForegroundColor DarkGray
        continue
    }
    $forceRebuild = $builtDll -and $previousDigest -eq $digest -and
                    $builtDll.LastWriteTime -lt $emitterTime
    & (Join-Path $Root 'scripts\build-recomp.ps1') -Root $Root -Target $Target `
      -Package $package -Module $module.Name -ForceRebuild:$forceRebuild
    New-Item -ItemType Directory -Force -Path $moduleBuild | Out-Null
    Set-Content -LiteralPath $digestPath -Value $digest -NoNewline -Encoding ascii
}
$dlls = Get-ChildItem -LiteralPath $build -Recurse -Filter '*.dll' -File
if ($dlls.Count -ne $modules.Count) {
    throw "built $($dlls.Count) DLLs for $($modules.Count) generated modules"
}
if ($dlls | Where-Object LastWriteTime -lt $emitterTime) { throw 'stale module DLL detected' }
if (Test-Path -LiteralPath $previousGen) {
    $resolvedPrevious = [IO.Path]::GetFullPath($previousGen)
    if (-not $resolvedPrevious.StartsWith($generatedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'previous export resolved outside generated root'
    }
    Remove-Item -LiteralPath $resolvedPrevious -Recurse -Force
}
[pscustomobject]@{ Target = $Target; Package = $package; Modules = $modules.Name;
                  ModuleCount = $modules.Count; BuildDirectory = $build }
