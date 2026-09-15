# Re-export the target on Windows and rebuild every module, JIT-free.
#
# The Linux equivalent is scripts/reexport-rebuild.sh. Same shape, same two
# things that are load-bearing:
#
#   - the previous export is cleared first. A surviving aot_manifest.json is a
#     cache hit, and the "re-export" then regenerates nothing at all.
#   - nothing reports success if a built image is older than the emitter. Every
#     measurement this project has had to throw away was a build that silently
#     did not happen.
#
# SUYU_AOT_TRANSLATE_ALL turns on the two families that are deliberately left to
# the JIT in a hybrid build; a JIT-free image has nothing to leave them to.
# SUYU_AOT_EXTRA_ROOTS seeds block discovery with addresses a previous run
# reached but discovery could not see - without them the image takes dispatch
# misses even on the path they were recorded from.

[CmdletBinding()]
param(
    [string]$Root   = $env:MK8R_ROOT,
    [string]$Rom    = $env:MK8R_ROM,
    [string]$Target = $env:MK8R_TARGET,
    [switch]$SkipExport,
    [switch]$Hybrid          # leave the gated families to the JIT
)

if (-not $Root)   { $Root = Split-Path -Parent $PSScriptRoot }
if (-not $Target) { $Target = 'mario-kart-8-deluxe' }
$ErrorActionPreference = 'Stop'

$gen = Join-Path $Root "generated\$Target"
$emitter = Join-Path $Root 'third_party\suyu\src\core\recompiler\arm64_to_c.h'
if (-not (Test-Path -LiteralPath $emitter)) { throw "no emitter at $emitter" }

if (-not $SkipExport) {
    if (-not $Rom) { throw 'set MK8R_ROM (or -Rom) to the ROM to export from' }

    if (-not $Hybrid) { $env:SUYU_AOT_TRANSLATE_ALL = '1' }
    $roots = Join-Path $Root 'local\roots'
    if (Test-Path -LiteralPath $roots) {
        $env:SUYU_AOT_EXTRA_ROOTS = $roots
        Write-Host "seeding discovery from $roots" -ForegroundColor DarkGray
    }

    # The emitter is compiled into suyu.exe, so editing the header and exporting
    # without rebuilding silently exports from the old emitter. The Linux
    # equivalent builds first for this reason; leaving it out here cost one
    # wasted export and very nearly a wrong conclusion.
    Write-Host '=== 0/3 suyu ===' -ForegroundColor Cyan
    & (Join-Path $Root 'scripts\build-suyu.ps1') | Select-Object -Last 3
    if ($LASTEXITCODE -ne 0) { throw "suyu build failed ($LASTEXITCODE)" }

    Write-Host '=== 1/3 clearing the previous export ===' -ForegroundColor Cyan
    if (Test-Path -LiteralPath $gen) { Remove-Item -LiteralPath $gen -Recurse -Force }
    Write-Host "removed $gen"

    Write-Host '=== 2/3 export ===' -ForegroundColor Cyan
    Get-Process -Name suyu -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2
    $log = Join-Path $env:APPDATA 'suyu\log\suyu_log.txt'
    Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue

    $exe = Join-Path $Root 'build\suyu\bin\suyu.exe'
    Start-Process -FilePath $exe -ArgumentList @('-hacker') | Out-Null
    # The MCP listener is what the export driver talks to.
    $up = $false
    foreach ($i in 1..60) {
        try { (New-Object Net.Sockets.TcpClient('127.0.0.1', 9742)).Close(); $up = $true; break }
        catch { Start-Sleep -Seconds 2 }
    }
    if (-not $up) { throw 'suyu MCP did not come up' }

    $env:MK8R_OUT = Join-Path $gen 'out'
    $env:MK8R_ROM = $Rom
    python (Join-Path $Root 'scripts\export-recomp.py') source
    Get-Process -Name suyu -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 2

    Select-String -Path $log -Pattern 'AOT coverage \[' |
        ForEach-Object { ($_.Line -split 'RunAotPrecompile: ')[-1] }
}

Write-Host '=== 3/3 modules ===' -ForegroundColor Cyan
$pkg = Get-ChildItem -LiteralPath $gen -Recurse -Directory -Filter 'aot_cache' -ErrorAction SilentlyContinue |
       Select-Object -First 1
if (-not $pkg) { throw "no aot_cache under $gen - the export produced nothing" }
$package = $pkg.Parent.FullName.Substring($gen.Length + 1)
Write-Host "package: $package"

foreach ($m in @('main', 'sdk', 'subsdk0', 'rtld')) {
    # The export directory name carries the title and version, so a re-export
    # can land somewhere new. CMake refuses to reuse a cache whose source path
    # moved, and its error names the old path rather than saying that is why.
    $b = Join-Path $Root "build\recomp\$Target\$m"
    if (Test-Path -LiteralPath $b) { Remove-Item -LiteralPath $b -Recurse -Force }
    & (Join-Path $Root 'scripts\build-recomp.ps1') -Module $m -Target $Target -Package $package
}

# Refuse to hand a stale build to anything downstream.
$emitterTime = (Get-Item -LiteralPath $emitter).LastWriteTime
$stale = Get-ChildItem -LiteralPath (Join-Path $Root "build\recomp\$Target") -Recurse -Filter '*.dll' |
         Where-Object { $_.LastWriteTime -lt $emitterTime }
if ($stale) {
    $stale | ForEach-Object { Write-Host "STALE: $($_.FullName)" -ForegroundColor Red }
    throw 'refusing to report success with images older than the emitter'
}
Write-Host 'all modules newer than the emitter' -ForegroundColor Green
Get-ChildItem -LiteralPath (Join-Path $Root "build\recomp\$Target") -Recurse -Filter '*.dll' |
    ForEach-Object { '{0,-28} {1,12:n0} bytes' -f $_.Name, $_.Length }
