# Locate vcvars64.bat without hardcoding an edition, a year or an install path.
#
# MK8R_VCVARS overrides outright. Otherwise vswhere is asked - it ships with
# every Visual Studio installer since 2017 and is the supported way to find an
# installation, so this works on Community, Professional and Build Tools alike.

[CmdletBinding()]
param([string]$VcVars = $env:MK8R_VCVARS)

if ($VcVars) {
    if (-not (Test-Path -LiteralPath $VcVars)) {
        throw "MK8R_VCVARS is set but does not exist: $VcVars"
    }
    return $VcVars
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio, or set MK8R_VCVARS to a vcvars64.bat."
}

$install = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath | Select-Object -First 1

if (-not $install) {
    throw "no Visual Studio installation with the C++ toolset. Set MK8R_VCVARS to a vcvars64.bat."
}

$path = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path -LiteralPath $path)) {
    throw "vcvars64.bat not found under $install"
}
return $path
