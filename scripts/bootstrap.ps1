# Reproduces this workspace from a clean machine.
#
# Does NOT fetch the game, keys, or firmware. Those are yours to supply.

[CmdletBinding()]
param(
    [string]$Root = 'G:\mk8-recomp',
    [string]$Game = 'D:\Games\the target title.xci\the target title.xci',
    [string]$ExpectedSha = 'REDACTED',
    [switch]$SkipHash
)

$ErrorActionPreference = 'Stop'
$SuyuCommit = 'd1d09321d7ab84252291e05b3efbc8a8dfa57481'
$GlslangTag = '16.5.0'
$QtVersion  = '6.9.3'

# --- input verification -----------------------------------------------------
# The containing directory also ends in .xci, so every path touch is -LiteralPath.

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

# --- directories ------------------------------------------------------------

foreach ($d in 'docs','scripts','src\runtime','src\bridge','src\instrumentation',
                'tests','manifests','local\tools','generated','traces\baseline','build','third_party') {
    New-Item -ItemType Directory -Force -Path (Join-Path $Root $d) | Out-Null
}

# --- pinned upstream --------------------------------------------------------

$SuyuSrc = Join-Path $Root 'third_party\suyu'
if (-not (Test-Path -LiteralPath (Join-Path $SuyuSrc '.git'))) {
    git clone --quiet https://github.com/suyu-emu/suyu-v0.0.4.git $SuyuSrc
}
Push-Location $SuyuSrc
git checkout --quiet $SuyuCommit
git submodule update --init --recursive --depth 1 --jobs 8
Pop-Location

# --- glslang ----------------------------------------------------------------
# suyu needs glslangValidator to compile its host shaders. That normally means
# installing the whole Vulkan SDK; the standalone Khronos release is ~10 MB and
# is the only piece the build actually uses. Vulkan headers come from
# externals/Vulkan-Headers, so no SDK is needed beyond this one tool.
#
# glslang 16.x renamed the executable to glslang.exe, so build-suyu.ps1 passes
# -DGLSLANGVALIDATOR explicitly instead of relying on find_program.

$GlslangExe = Join-Path $Root 'local\tools\glslang\bin\glslang.exe'
if (-not (Test-Path -LiteralPath $GlslangExe)) {
    Push-Location (Join-Path $Root 'local\tools')
    gh release download $GlslangTag --repo KhronosGroup/glslang `
        --pattern "glslang-$GlslangTag-windows-x86_64-release.zip" --clobber
    Expand-Archive -LiteralPath "glslang-$GlslangTag-windows-x86_64-release.zip" `
        -DestinationPath 'glslang' -Force
    Pop-Location
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
