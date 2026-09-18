# Build and run tests/shifted_register_width_test.cpp.
#
# Two compiles: the generator includes the emitter header directly (no build
# system, but it does need the MSVC environment), and then the C it prints is
# compiled with /we4293 so that an out-of-range shift count is an error rather
# than a warning. That second compile is the actual regression check - it is
# the exact diagnostic that blocked the Mario Kart 8 Deluxe export.

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'

$VcVars = & (Join-Path $PSScriptRoot 'find-vcvars.ps1')
if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvars64.bat not found at $VcVars" }

$Build = Join-Path $Root 'build'
New-Item -ItemType Directory -Force -Path $Build | Out-Null

$gen = Join-Path $Build 'shifted_register_width_gen.exe'
$src = Join-Path $Build 'shifted_register_width_generated.c'
$exe = Join-Path $Build 'shifted_register_width_test.exe'
foreach ($stale in @($gen, $src, $exe)) {
    Remove-Item -LiteralPath $stale -Force -ErrorAction SilentlyContinue
}

# /Fo needs a trailing backslash to mean "a directory", and a trailing
# backslash immediately before a closing quote escapes the quote - cl then sees
# a literal `"` in the path. So /Fo is passed unquoted; none of these paths
# contain spaces.
$cl = "cl /nologo /EHsc /std:c++20 /I `"$Root\third_party\suyu\src`" " +
      "`"$Root\tests\shifted_register_width_test.cpp`" /Fe:`"$gen`" /Fo:$Build\"

& cmd.exe /c "call `"$VcVars`" >nul 2>&1 && $cl"
if ($LASTEXITCODE -ne 0) { throw "generator compile failed (exit $LASTEXITCODE)" }

& $gen | Set-Content -LiteralPath $src -Encoding ASCII
if ($LASTEXITCODE -ne 0) { throw "generator reported an unallocated encoding was translated (exit $LASTEXITCODE)" }

# C4293 is "shift count negative or too big, undefined behavior". It is only a
# warning by default, which is why 4,669 of them reached a real export.
$cc = "cl /nologo /W4 /we4293 `"$src`" /Fe:`"$exe`" /Fo:$Build\"
& cmd.exe /c "call `"$VcVars`" >nul 2>&1 && $cc"
if ($LASTEXITCODE -ne 0) { throw "generated C failed to compile (exit $LASTEXITCODE)" }

& $exe
exit $LASTEXITCODE
