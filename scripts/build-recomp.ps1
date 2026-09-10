# Build one generated recompiled module into a Windows DLL.
#
# The exporter's own Build mode is not usable here: it shells out to vcvars64.bat
# at a hardcoded VS2022 *Community* path (game_export.cpp:1929-1954) and then
# relinks suyu itself. This machine has VS2026 Community and VS2022 Build Tools.
# So we drive the generated CMake directly.
#
# Target `recompiled_<module>` is the SHARED build, which exports
# recomp_image_lookup / recomp_image_set_base - the two symbols suyu resolves
# when loading an AOT image (suyu.cpp:552-553, main.cpp:6285/6305). That is the
# shape we want: it loads into a normal suyu build with no relinking.

[CmdletBinding()]
param(
    [string]$Root    = 'G:\mk8-recomp',
    [string]$Package = 'the target title',
    [Parameter(Mandatory)]
    [string]$Module,
    [string]$BuildType = 'Release'
)

$ErrorActionPreference = 'Stop'

$Src    = Join-Path $Root "generated\$Package\aot_cache\exefs\$Module"
$Build  = Join-Path $Root "build\recomp\$Module"
$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'

if (-not (Test-Path -LiteralPath $Src))    { throw "No generated project at $Src" }
if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvars64.bat not found at $VcVars" }

New-Item -ItemType Directory -Force -Path $Build | Out-Null

function Invoke-InVsEnv([string]$Command) {
    & cmd.exe /c "call `"$VcVars`" >nul 2>&1 && $Command"
    if ($LASTEXITCODE -ne 0) { throw "Failed (exit $LASTEXITCODE): $Command" }
}

$srcSize = (Get-ChildItem -LiteralPath (Join-Path $Src 'src') -File |
            Measure-Object -Property Length -Sum).Sum
Write-Host ("module {0}: {1:n1} MB of generated C" -f $Module, ($srcSize / 1MB)) -ForegroundColor Cyan

Invoke-InVsEnv "cmake -S `"$Src`" -B `"$Build`" -G Ninja -DCMAKE_BUILD_TYPE=$BuildType"

$sw = [Diagnostics.Stopwatch]::StartNew()
Invoke-InVsEnv "cmake --build `"$Build`" --target recompiled_$Module"
$sw.Stop()

Write-Host ("built in {0:n1} min" -f $sw.Elapsed.TotalMinutes) -ForegroundColor Green
Get-ChildItem -LiteralPath $Build -Recurse -Include *.dll |
    Select-Object Name, Length, FullName | Format-Table -AutoSize
