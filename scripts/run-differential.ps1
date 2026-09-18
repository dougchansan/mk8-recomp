# Build and run the synthetic execution differential with MSVC.
#
# The companion run-differential.sh covers GCC/UBSan on Linux. This path exists
# because the Dynarmic oracle is already built here as part of build\suyu, so
# the differential can be run without a Linux box and without rebuilding
# anything: it only ever reads the existing dynarmic.lib and fmt.lib.
#
# Everything is staged into an isolated work directory, as the harness requires.
[CmdletBinding()]
param(
    # Repository root holding third_party\suyu and build\suyu.
    [string] $Root = (Split-Path -Parent $PSScriptRoot),
    # Isolated directory to build in. Rebuilt from scratch when -Clean is given.
    [string] $WorkDir = (Join-Path $env:TEMP 'mk8-differential'),
    # Arguments forwarded to differential-run.exe, e.g. --memory --samples 512.
    # Named RunArgs rather than Args because $args is an automatic variable.
    [string[]] $RunArgs = @(),
    [switch] $Clean,
    # Build only; do not run.
    [switch] $NoRun
)

$ErrorActionPreference = 'Stop'

function Invoke-Vc {
    param([string] $Command, [string] $VcVars)
    # vcvars cannot be imported into this session, so the whole command is
    # handed to cmd.exe after it. Not '&&' - PowerShell 5.1 has no such operator
    # and this string is parsed by cmd, not by PowerShell.
    & cmd.exe /c "call `"$VcVars`" >nul 2>&1 && $Command"
    if ($LASTEXITCODE -ne 0) { throw "failed (exit $LASTEXITCODE): $Command" }
}

$suyuSrc = Join-Path $Root 'third_party\suyu\src'
$dynSrc  = Join-Path $suyuSrc 'dynarmic\src'
$build   = Join-Path $Root 'build\suyu'
$dynLib  = Join-Path $build 'src\dynarmic\src\dynarmic\dynarmic.lib'
$fmtLib  = Join-Path $build '_deps\fmt-build\fmt.lib'
$emitter = Join-Path $suyuSrc 'core\recompiler\arm64_to_c.h'
$ccJson  = Join-Path $build 'compile_commands.json'

foreach ($required in @($dynLib, $fmtLib, $emitter, $ccJson)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "missing required artifact: $required" }
}

$VcVars = & (Join-Path $PSScriptRoot 'find-vcvars.ps1')
if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvars64.bat not found at $VcVars" }

if ($Clean -and (Test-Path -LiteralPath $WorkDir)) {
    Remove-Item -LiteralPath $WorkDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $WorkDir, (Join-Path $WorkDir 'tests'),
    (Join-Path $WorkDir 'core\recompiler'), (Join-Path $WorkDir 'runner') | Out-Null

Copy-Item (Join-Path $Root 'tests\differential_*') (Join-Path $WorkDir 'tests') -Force
Copy-Item $emitter (Join-Path $WorkDir 'core\recompiler') -Force

# Reuse the exact include set the existing dynarmic objects were compiled with,
# so the oracle headers agree with the library rather than being guessed at.
$entry = (Get-Content -LiteralPath $ccJson -Raw | ConvertFrom-Json) |
    Where-Object { $_.file -match 'a64_interface\.cpp' } | Select-Object -First 1
if (-not $entry) { throw "no a64_interface.cpp entry in $ccJson" }
# This fork also contains an inactive externals\dynarmic with an incompatible
# ABI. Require the library to have been built from the active src\dynarmic, the
# same check the Linux script makes. Compared on the parsed entry rather than
# the raw file, whose backslashes are JSON-escaped.
$expected = Join-Path $dynSrc 'dynarmic\backend\x64\a64_interface.cpp'
if ((Resolve-Path -LiteralPath $entry.file).Path -ne (Resolve-Path -LiteralPath $expected).Path) {
    throw "build\suyu was built from $($entry.file), not $expected; the oracle ABI would differ"
}
$cmdline = $entry.command
if (-not $cmdline) { $cmdline = ($entry.arguments -join ' ') }
$includes = [regex]::Matches($cmdline, '[-/]I\s*"?([^"\s]+)') |
    ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique
$rsp = Join-Path $WorkDir 'includes.rsp'
Set-Content -LiteralPath $rsp -Encoding ascii -Value (
    ($includes | ForEach-Object { '/I"' + $_ + '"' }) -join [Environment]::NewLine)

Push-Location $WorkDir
try {
    Write-Host '=== generator ==='
    Invoke-Vc -VcVars $VcVars -Command ('cl /nologo /std:c++20 /EHsc /O2 /MD /I. ' +
        'tests\differential_generate.cpp /Fe:differential-generate.exe /Fo:gen.obj')

    Write-Host '=== runtime and generated code ==='
    # The production runtime is emitted and compiled rather than reimplemented,
    # so memory cases call the real load, store, pair and exclusive helpers.
    Invoke-Vc -VcVars $VcVars -Command '.\differential-generate.exe --runtime-h > recomp_runtime.h'
    Invoke-Vc -VcVars $VcVars -Command '.\differential-generate.exe --runtime-c > recomp_runtime.c'
    Invoke-Vc -VcVars $VcVars -Command '.\differential-generate.exe --encodings > differential-encodings.txt'
    Invoke-Vc -VcVars $VcVars -Command '.\differential-generate.exe > differential-generated.c'

    Write-Host '=== encoding check ==='
    # A mis-encoded word would decode to some other legal instruction, both
    # engines would agree about it, and the case would report a pass while
    # testing nothing of what it names. Checked before anything runs.
    & python (Join-Path $WorkDir 'tests\differential_verify_assembly.py') `
        (Join-Path $WorkDir 'differential-encodings.txt')
    if ($LASTEXITCODE -ne 0) { throw 'encoding verification failed' }

    Write-Host '=== compile C ==='
    Invoke-Vc -VcVars $VcVars -Command ('cl /nologo /TC /O2 /MD /I. /Itests /c ' +
        'recomp_runtime.c /Fo:recomp_runtime.obj')
    Invoke-Vc -VcVars $VcVars -Command ('cl /nologo /TC /O2 /MD /I. /Itests /c ' +
        'differential-generated.c /Fo:differential-generated.obj')

    Write-Host '=== runner ==='
    Invoke-Vc -VcVars $VcVars -Command ('cl /nologo /std:c++20 /EHsc /O2 /MD /utf-8 @includes.rsp ' +
        "/I. /Itests /I`"$dynSrc`" /I`"$suyuSrc`" " +
        'tests\differential_dynarmic.cpp tests\differential_logging.cpp ' +
        "differential-generated.obj recomp_runtime.obj `"$dynLib`" `"$fmtLib`" " +
        '/Fe:differential-run.exe /Fo:runner\')

    if ($NoRun) { Write-Host "built: $(Join-Path $WorkDir 'differential-run.exe')"; return }

    & (Join-Path $WorkDir 'differential-run.exe') @RunArgs
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
