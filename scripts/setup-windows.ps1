# One-shot setup for Windows: install toolchain, clone, bootstrap, build.
#
# Standalone - it can be run straight from the web before the repository
# exists:
#
#   irm https://raw.githubusercontent.com/dougchansan/mk8-recomp/main/scripts/setup-windows.ps1 | iex
#
# Run it from inside an existing clone and it builds that clone in place.
# Everything it installs comes from winget, which ships with Windows 11 and
# with Windows 10 1809 or newer.
#
# Does NOT fetch a game, keys, or firmware. Those are yours to supply.
# Settings come from the environment so that the piped form above can be
# configured without arguments:
#
#   $env:MK8R_ROOT   where to clone to        (default .\mk8-recomp)
#   $env:MK8R_ROM    your own dump, verified by bootstrap.ps1 when set
#   $env:MK8R_SETUP  'deps' to stop after installing tools,
#                    'nobuild' to stop after cloning

[CmdletBinding()]
param(
    [string]$Root  = $env:MK8R_ROOT,
    [string]$Game  = $env:MK8R_ROM,
    [string]$Stage = $env:MK8R_SETUP,
    [string]$RepoUrl = 'https://github.com/dougchansan/mk8-recomp'
)

$ErrorActionPreference = 'Stop'

function Say  ($m) { Write-Host "==> $m" -ForegroundColor Green }
function Warn ($m) { Write-Host "warn $m" -ForegroundColor Yellow }

if ([Environment]::Is64BitOperatingSystem -eq $false) { throw 'x86-64 Windows is required.' }
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw @'
winget not found. Install "App Installer" from the Microsoft Store (or update
Windows), then re-run. Without it, install by hand: Git, Python 3, CMake,
Ninja, and Visual Studio 2022 with the "Desktop development with C++" workload.
'@
}

$admin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)

# --- toolchain --------------------------------------------------------------
# winget is idempotent, but it exits non-zero when a package is already current
# and $ErrorActionPreference does not cover native exit codes, so each install
# is checked explicitly. A missing command is the real test either way.

# Windows ships App Execution Alias stubs under WindowsApps - python.exe is one
# - which resolve through Get-Command and then open the Store instead of
# running anything. They are zero-byte reparse points, so size tells them apart
# from a real install without executing them and popping the Store open.
function Test-RealCommand ($Name) {
    $c = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue |
         Select-Object -First 1
    if (-not $c) { return $false }
    try { if ((Get-Item -LiteralPath $c.Source).Length -eq 0) { return $false } } catch { }
    return $true
}

function Install-Winget ($Id, $Probe, $Override) {
    if ($Probe -and (Test-RealCommand $Probe)) {
        Say "$Probe already installed"
        return
    }
    Say "installing $Id"
    # --source winget is not optional: when a package also matches in msstore,
    # winget refuses the install and asks for a source rather than picking one,
    # so without this every install fails while the script carries on.
    $wargs = @('install', '--id', $Id, '--exact', '--source', 'winget', '--silent',
               '--accept-package-agreements', '--accept-source-agreements',
               '--disable-interactivity')
    if ($Override) { $wargs += @('--override', $Override) }
    & winget @wargs
    # 0x8A150061 = already installed, 0x8A15002B = no applicable upgrade.
    if ($LASTEXITCODE -notin 0, -1978335135, -1978335189) {
        Warn "winget returned $LASTEXITCODE for $Id"
    }
    # Whatever winget reported, the package either landed or it did not. Fail
    # here rather than three steps later inside cmake, where the cause is gone.
    if ($Probe) {
        $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
                    [Environment]::GetEnvironmentVariable('Path', 'User')
        if (-not (Test-RealCommand $Probe)) {
            throw "$Id did not install - '$Probe' is still not on PATH. Install it by hand and re-run."
        }
    }
}

Install-Winget 'Git.Git'          'git'
Install-Winget 'Kitware.CMake'    'cmake'
Install-Winget 'Ninja-build.Ninja' 'ninja'
Install-Winget 'Python.Python.3.12' 'python'

# winget puts new commands on the machine PATH, which this session inherited
# before they existed. Rebuild it in place rather than telling you to reopen
# the terminal, which the piped one-liner cannot do anyway.
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
            [Environment]::GetEnvironmentVariable('Path', 'User')

# MSVC. vswhere is the supported way to ask whether the C++ toolset is present,
# and it is installed by every Visual Studio since 2017 - Community and Build
# Tools alike - so an existing Visual Studio is reused rather than duplicated.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$haveMsvc = (Test-Path -LiteralPath $vswhere) -and
            (& $vswhere -latest -products * `
                -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                -property installationPath | Select-Object -First 1)
if ($haveMsvc) {
    Say 'MSVC C++ toolset already installed'
} elseif (-not $admin) {
    throw @'
The MSVC C++ toolset is missing and installing it needs an elevated shell.
Re-run this script from an Administrator PowerShell, or install "Desktop
development with C++" through the Visual Studio Installer yourself.
'@
} else {
    Say 'installing the Visual Studio 2022 build tools (several GB, slow)'
    Install-Winget 'Microsoft.VisualStudio.2022.BuildTools' $null `
        '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
}

if ($Stage -eq 'deps') { Say 'tools only - stopping here'; return }

# --- source -----------------------------------------------------------------
if ((Test-Path 'scripts\build-suyu.ps1') -and (Test-Path '.gitmodules')) {
    $Root = (Get-Location).Path
    Say "using the clone in $Root"
    git submodule update --init --recursive --jobs 8
} else {
    if (-not $Root) { $Root = Join-Path (Get-Location).Path 'mk8-recomp' }
    if (Test-Path (Join-Path $Root '.git')) {
        Say "updating the existing clone at $Root"
        git -C $Root pull --ff-only
        git -C $Root submodule update --init --recursive --jobs 8
    } else {
        Say "cloning into $Root"
        git clone --recursive --jobs 8 $RepoUrl $Root
    }
}
if ($LASTEXITCODE -ne 0) { throw "git failed (exit $LASTEXITCODE)" }

if ($Stage -eq 'nobuild') {
    Say "cloned to $Root - build with: $Root\scripts\build-suyu.ps1"
    return
}

# --- build ------------------------------------------------------------------
# bootstrap.ps1 fetches glslang and Qt into local\tools. It also verifies a
# dump when one is pointed at; without MK8R_ROM that step is skipped rather
# than blocking a toolchain-only setup.
$env:MK8R_ROOT = $Root
$bootstrap = @{ Root = $Root }
if ($Game) { $bootstrap.Game = $Game } else { $bootstrap.SkipGame = $true }

Say 'bootstrapping glslang and Qt'
& (Join-Path $Root 'scripts\bootstrap.ps1') @bootstrap

Say 'building - this takes a while'
& (Join-Path $Root 'scripts\build-suyu.ps1')

Say 'done'
Write-Host @"
  binaries : $Root\build\suyu\bin
  run      : $Root\scripts\run-gui.ps1

You still need your own legally dumped game and your own keys; nothing here
fetches either. See $Root\docs\assumptions.md.
"@
