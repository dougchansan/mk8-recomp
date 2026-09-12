# Reproduces this workspace from a clean machine.
#
# Does NOT fetch the game, keys, or firmware. Those are yours to supply.

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [string]$Game = $env:MK8R_ROM,
    [string]$ExpectedSha = 'REDACTED',
    [switch]$SkipHash,
    [switch]$SkipGame
)

# $PSScriptRoot is not populated while parameter defaults are bound under
# -File, so the fallback lives here rather than in the param block.
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }

$ErrorActionPreference = 'Stop'
$SuyuCommit = 'd1d09321d7ab84252291e05b3efbc8a8dfa57481'
$GlslangTag = '16.5.0'
$QtVersion  = '6.9.3'

# --- input verification -----------------------------------------------------
# The containing directory also ends in .xci, so every path touch is -LiteralPath.
#
# -SkipGame exists because everything below this block is toolchain setup: a
# machine can be prepared before a dump is on it.

if ($SkipGame -or -not $Game) {
    Write-Host 'ROM  : not supplied, skipping verification (-SkipGame)'
} else {
    if (-not (Test-Path -LiteralPath $Game)) { throw "Game dump not found: $Game" }
    $item = Get-Item -LiteralPath $Game
    Write-Host "ROM  : $($item.FullName)"
    Write-Host "Size : $($item.Length) bytes"

    if (-not $SkipHash) {
        Write-Host 'Hashing (15 GiB, takes a minute)...'
        $sha = (Get-FileHash -Algorithm SHA256 -LiteralPath $Game).Hash
        if ($sha -ne $ExpectedSha) {
            throw "SHA-256 mismatch.`n  expected $ExpectedSha`n  actual   $sha"
        }
        Write-Host "SHA256: $sha (matches)" -ForegroundColor Green
    }
}

# --- directories ------------------------------------------------------------

foreach ($d in 'docs','scripts','src\runtime','src\bridge','src\instrumentation',
                'tests','manifests','local\tools','generated','traces\baseline','build','third_party') {
    New-Item -ItemType Directory -Force -Path (Join-Path $Root $d) | Out-Null
}

# --- pinned upstream --------------------------------------------------------

# third_party/suyu is a submodule of the fork, pinned by the superproject. A
# clone here is only the fallback for a tree that was never a git checkout -
# checking out $SuyuCommit unconditionally would throw away the fork's commits.
$SuyuSrc = Join-Path $Root 'third_party\suyu'
if (Test-Path -LiteralPath (Join-Path $Root '.gitmodules')) {
    git -C $Root submodule update --init --recursive --jobs 8
} elseif (Test-Path -LiteralPath (Join-Path $SuyuSrc '.git')) {
    Push-Location $SuyuSrc
    git checkout --quiet $SuyuCommit
    git submodule update --init --recursive --depth 1 --jobs 8
    Pop-Location
} else {
    git clone --quiet https://github.com/dougchansan/suyu-v0.0.4.git -b mk8-recomp $SuyuSrc
    Push-Location $SuyuSrc
    git submodule update --init --recursive --depth 1 --jobs 8
    Pop-Location
}

# --- glslang ----------------------------------------------------------------
# suyu needs glslangValidator to compile its host shaders. That normally means
# installing the whole Vulkan SDK; the standalone Khronos release is ~10 MB and
# is the only piece the build actually uses. Vulkan headers come from
# externals/Vulkan-Headers, so no SDK is needed beyond this one tool.
#
# glslang 16.x renamed the executable to glslang.exe, so build-suyu.ps1 passes
# -DGLSLANGVALIDATOR explicitly instead of relying on find_program.

#
# The asset is fetched over plain HTTPS rather than through `gh`: the release
# is public, and gh would demand an authenticated CLI on a fresh machine.

$GlslangExe = Join-Path $Root 'local\tools\glslang\bin\glslang.exe'
if (-not (Test-Path -LiteralPath $GlslangExe)) {
    $zipName = "glslang-$GlslangTag-windows-x86_64-release.zip"
    $zip = Join-Path $Root "local\tools\$zipName"
    Invoke-WebRequest -UseBasicParsing -OutFile $zip `
        "https://github.com/KhronosGroup/glslang/releases/download/$GlslangTag/$zipName"
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $Root 'local\tools\glslang') -Force
}
& $GlslangExe --version | Select-Object -First 1

# --- Qt ---------------------------------------------------------------------
# YUZU_USE_BUNDLED_QT is not usable here. The Eden-CI Qt 6.11.1 Windows drop is
# built against a newer MSVC STL than either installed toolset provides, so
# suyu.exe cannot link against it (issue #16), and it ships no Qt6Svg although
# CMakeLists requires the component (issue #5). An official Qt built for MSVC
# 2022 links against older STL symbols, which a newer toolset still supplies -
# the compatible direction.

$QtRoot = Join-Path $Root 'local\tools\Qt'
$QtDir  = Join-Path $QtRoot "$QtVersion\msvc2022_64"
if (-not (Test-Path -LiteralPath $QtDir)) {
    python -m pip install --quiet aqtinstall
    python -m aqt install-qt windows desktop $QtVersion win64_msvc2022_64 -m qtcharts -O $QtRoot
}
if (-not (Test-Path -LiteralPath $QtDir)) { throw "Qt install failed: $QtDir" }
Write-Host "Qt   : $QtDir"

Write-Host ''
Write-Host 'Bootstrap complete. Next: scripts\build-suyu.ps1 -Configure' -ForegroundColor Green
