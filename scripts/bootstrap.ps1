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

foreach ($d in 'docs','scripts','src\runtime','src\bridge','src\patches','src\instrumentation',
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

# Upstream v0.0.4 does not configure on Windows: it requires the Qt6 Svg
# component, its own bundled Qt drop does not ship Svg, and nothing in the
# source actually uses Svg. See issue #5.
$patch = Join-Path $Root 'src\patches\0001-drop-spurious-qt6-svg-component.patch'
if (Test-Path -LiteralPath $patch) {
    git apply --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) { git apply $patch; Write-Host 'Applied 0001-drop-spurious-qt6-svg-component' }
    else { Write-Host 'Patch 0001 already applied or not applicable' }
}
Pop-Location

# --- glslang ----------------------------------------------------------------
# suyu needs glslangValidator to compile its host shaders. That normally means
# installing the whole Vulkan SDK; the standalone Khronos release is ~10 MB and
# is the only piece the build actually uses. Vulkan headers come from
# externals/Vulkan-Headers, so no SDK is needed beyond this.

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

Write-Host ''
Write-Host 'Bootstrap complete. Next: scripts\build-suyu.ps1 -Configure' -ForegroundColor Green
