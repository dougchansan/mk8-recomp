# Builds the pinned Suyu v0.0.4 checkout out-of-tree.
#
# The Qt frontend target (suyu) is required: the AOT export pipeline is a Qt
# dialog and has no CLI. suyu-cmd is the runtime an export is packaged around.
#
# Qt is not installed on this machine; YUZU_USE_BUNDLED_QT defaults ON under
# MSVC and downloads a Qt binary drop. Vulkan headers come from
# externals/Vulkan-Headers, so no Vulkan SDK install is needed.

[CmdletBinding()]
param(
    [string]$Root      = $env:MK8R_ROOT,
    [string]$BuildType = 'Release',
    [switch]$Configure,
    [switch]$Clean
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'

$SuyuSrc   = Join-Path $Root 'third_party\suyu'
$BuildDir  = Join-Path $Root 'build\suyu'
$VcVars    = & (Join-Path $PSScriptRoot 'find-vcvars.ps1')
# glslang 16.x renamed glslangValidator to glslang; suyu's find_program still
# looks for the old name, so point the cache variable at the new binary rather
# than installing the whole Vulkan SDK for one shader compiler.
$Glslang   = Join-Path $Root 'local\tools\glslang\bin\glslang.exe'
$QtDir     = Join-Path $Root 'local\tools\Qt\6.9.3\msvc2022_64'

if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvars64.bat not found at $VcVars" }
if (-not (Test-Path -LiteralPath $SuyuSrc)) { throw "Suyu checkout not found at $SuyuSrc" }
if (-not (Test-Path -LiteralPath $Glslang)) { throw "glslang not found at $Glslang - see scripts/bootstrap.ps1" }

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

# CMake and Ninja must run inside the MSVC environment. Rather than importing
# vcvars into this session, hand the whole command to cmd.exe after it.
function Invoke-InVsEnv([string]$Command) {
    $full = "call `"$VcVars`" >nul 2>&1 && $Command"
    & cmd.exe /c $full
    if ($LASTEXITCODE -ne 0) { throw "Command failed (exit $LASTEXITCODE): $Command" }
}

$cmakeArgs = @(
    "-S `"$SuyuSrc`""
    "-B `"$BuildDir`""
    '-G Ninja'
    "-DCMAKE_BUILD_TYPE=$BuildType"
    '-DENABLE_QT=ON'
    # The bundled Eden-CI Qt 6.11.1 drop is built with a newer MSVC STL than
    # either toolset on this machine provides, so linking suyu.exe against it
    # fails on __std_replace_copy_1/2 and __std_minmax_element_2u. Use an
    # official Qt built for MSVC 2022 instead - it also actually ships Svg.
    '-DYUZU_USE_BUNDLED_QT=OFF'
    "-DQt6_DIR=`"$QtDir`""
    "-DCMAKE_PREFIX_PATH=`"$QtDir`"" 
    '-DYUZU_CMD=ON'
    '-DYUZU_TESTS=OFF'
    '-DENABLE_WEB_SERVICE=OFF'
    '-DYUZU_ROOM=OFF'
    '-DYUZU_ROOM_STANDALONE=OFF'
    '-DENABLE_QT_TRANSLATION=OFF'
    '-DUSE_DISCORD_PRESENCE=OFF'
    "-DGLSLANGVALIDATOR=`"$Glslang`""
    # A linker map turns a Windows Error Reporting fault offset into a function
    # name. Without a debugger on this machine it is the only way to identify
    # where suyu is crashing.
    '-DCMAKE_EXE_LINKER_FLAGS=/MAP'
    '-DCMAKE_SHARED_LINKER_FLAGS=/MAP' 
) -join ' '

if ($Configure -or -not (Test-Path -LiteralPath (Join-Path $BuildDir 'build.ninja'))) {
    Write-Host '=== configure ===' -ForegroundColor Cyan
    Invoke-InVsEnv "cmake $cmakeArgs"
}

Write-Host '=== build: suyu suyu-cmd ===' -ForegroundColor Cyan
$sw = [Diagnostics.Stopwatch]::StartNew()
Invoke-InVsEnv "cmake --build `"$BuildDir`" --target suyu suyu-cmd"
$sw.Stop()

Write-Host ("=== built in {0:n1} min ===" -f $sw.Elapsed.TotalMinutes) -ForegroundColor Green
Get-ChildItem -LiteralPath (Join-Path $BuildDir 'bin') -Filter '*.exe' -ErrorAction SilentlyContinue |
    Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
