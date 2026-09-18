# Generate, compile, load, and execute a synthetic side-entry module.
[CmdletBinding()]
param([string]$Root = $env:MK8R_ROOT)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath($Root)
$vcvars = & (Join-Path $PSScriptRoot 'find-vcvars.ps1')
$testRoot = Join-Path $Root 'build\side-entry-runtime'
$project = Join-Path $testRoot 'project'
$build = Join-Path $testRoot 'build'
New-Item -ItemType Directory -Force -Path $project,$build | Out-Null

function Invoke-InVsEnv([string]$Command) {
    & cmd.exe /c "call `"$vcvars`" >nul 2>&1 && $Command"
    if ($LASTEXITCODE -ne 0) { throw "Failed (exit $LASTEXITCODE): $Command" }
}

$generator = Join-Path $testRoot 'side-entry-export.exe'
$runner = Join-Path $testRoot 'side-entry-runtime-test.exe'
Invoke-InVsEnv "cl /nologo /EHsc /std:c++20 /I `"$Root\third_party\suyu\src`" `"$Root\tests\side_entry_export.cpp`" /Fe:`"$generator`" /Fo:$testRoot\"
& $generator $project
if ($LASTEXITCODE -ne 0) { throw "synthetic export failed (exit $LASTEXITCODE)" }
Invoke-InVsEnv "cmake -S `"$project`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release"
Invoke-InVsEnv "cmake --build `"$build`" --target recompiled_side"
Invoke-InVsEnv "cl /nologo /EHsc /std:c++20 `"$Root\tests\side_entry_runtime_test.cpp`" /Fe:`"$runner`" /Fo:$testRoot\"
& $runner (Join-Path $build 'recompiled_side.dll')
if ($LASTEXITCODE -ne 0) { throw "side-entry runtime test failed (exit $LASTEXITCODE)" }
$oldNativePreference = $PSNativeCommandUseErrorActionPreference
$PSNativeCommandUseErrorActionPreference = $false
try {
    & $runner (Join-Path $build 'recompiled_side.dll') --mutate 2>$null
    $mutationExit = $LASTEXITCODE
} finally {
    $PSNativeCommandUseErrorActionPreference = $oldNativePreference
}
if ($mutationExit -eq 86) { throw 'mutated interior entry bypassed guard-v2' }
# MSVC's abort() terminates through the Windows fast-fail status. Accept only
# that exact status so an unrelated crash cannot masquerade as guard success.
if ($mutationExit -ne -1073740791) {
    throw "mutated side entry exited $mutationExit instead of guard-v2 fast-fail"
}
Write-Host 'mutated interior entry was refused by guard-v2' -ForegroundColor Green
