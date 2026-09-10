# Build and run tests/translate_test.cpp.
#
# The test is a single translation unit that includes the emitter header
# directly, so it needs no build system - but it does need the MSVC environment,
# and vcvars has to be imported by cmd.exe rather than PowerShell.

[CmdletBinding()]
param(
    [string]$Root = 'G:\mk8-recomp'
)

$ErrorActionPreference = 'Stop'

$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $VcVars)) { throw "vcvars64.bat not found at $VcVars" }

$Build = Join-Path $Root 'build'
New-Item -ItemType Directory -Force -Path $Build | Out-Null

$exe = Join-Path $Build 'translate_test.exe'
Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue

$cl = "cl /nologo /EHsc /std:c++20 /I `"$Root\third_party\suyu\src`" " +
      # /Fo needs a trailing backslash to mean "a directory", and a trailing
      # backslash immediately before a closing quote escapes the quote - cl then
      # sees a literal `"` in the path. So this one is passed unquoted; none of
      # these paths contain spaces.
      "`"$Root\tests\translate_test.cpp`" /Fe:`"$exe`" /Fo:$Build\"

& cmd.exe /c "call `"$VcVars`" >nul 2>&1 && $cl"
if ($LASTEXITCODE -ne 0) { throw "compile failed (exit $LASTEXITCODE)" }

& $exe
exit $LASTEXITCODE
