# Launch suyu with a recompiled AOT image loaded, then boot the target.
#
# suyu's frontend scans SUYU_RECOMP_DIR recursively for *.dll, resolves
# recomp_image_lookup / recomp_image_set_base out of each, and derives the
# module identity from the containing directory name (main.cpp:6266-6405).
# build\recomp\<target>\<module>\recompiled_<module>.dll already has that shape.
#
# Loading has to happen before BootGame: the CPU backend is chosen once, at
# process start (main.cpp:718-735). So this sets the variable and launches,
# rather than attaching to a running suyu.
#
#   .\scripts\run-hybrid.ps1
#   .\scripts\run-hybrid.ps1 -Baseline      # same, without the AOT image

[CmdletBinding()]
param(
    [string]$Root   = 'G:\mk8-recomp',
    [string]$Target = 'target',
    [string]$Rom    = 'D:\Games\the target title.xci\the target title.xci',
    [int]$RunSeconds = 60,
    [switch]$Baseline,
    [int]$WaitSeconds = 120
)

$ErrorActionPreference = 'Stop'

$Exe      = Join-Path $Root 'build\suyu\bin\suyu.exe'
$RecompIn = Join-Path $Root "build\recomp\$Target"
if (-not (Test-Path -LiteralPath $Exe)) { throw "Not built: $Exe" }
if (-not (Test-Path -LiteralPath $Rom)) { throw "No ROM at $Rom" }

Get-Process suyu -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

if ($Baseline) {
    Remove-Item Env:\SUYU_RECOMP_DIR -ErrorAction SilentlyContinue
    Write-Host 'BASELINE: no AOT image (dynarmic only)' -ForegroundColor Yellow
} else {
    if (-not (Test-Path -LiteralPath $RecompIn)) { throw "No recompiled modules at $RecompIn" }
    $dlls = Get-ChildItem -LiteralPath $RecompIn -Recurse -Filter '*.dll'
    if (-not $dlls) { throw "No DLLs under $RecompIn" }
    $env:SUYU_RECOMP_DIR = $RecompIn
    Write-Host "HYBRID: $($dlls.Count) AOT modules from $RecompIn" -ForegroundColor Green
    $dlls | ForEach-Object { '    {0,-28} {1,12:n0} bytes' -f $_.Directory.Name, $_.Length }
}

# Rotate the log so the run's coverage report is unambiguous.
$logPath = Join-Path $env:APPDATA 'suyu\log\suyu_log.txt'
if (Test-Path -LiteralPath $logPath) { Remove-Item -LiteralPath $logPath -Force }

$p = Start-Process -FilePath $Exe -ArgumentList '-hacker' -PassThru
Write-Host "suyu pid $($p.Id)"

$deadline = (Get-Date).AddSeconds($WaitSeconds)
$mcp = $false
while ((Get-Date) -lt $deadline) {
    if ((Test-NetConnection 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded) {
        $mcp = $true; break
    }
    Start-Sleep -Seconds 2
}
if (-not $mcp) { throw 'MCP server did not come up' }

python (Join-Path $Root 'scripts\boot-and-stop.py') $Rom $RunSeconds

# The execution-coverage report is printed from the last ArmRecomp destructor,
# which only runs when the emulated process is actually torn down. Killing suyu
# here loses the whole report, so shut down properly and wait for it.
Write-Host 'closing suyu for teardown ...'
$null = $p.CloseMainWindow()
if (-not $p.WaitForExit(90000)) {
    Write-Warning 'suyu did not exit within 90s; killing (coverage report will be missing)'
    $p.Kill()
}
Start-Sleep -Seconds 3

Write-Host ''
Write-Host '=== coverage report ===' -ForegroundColor Cyan
Get-Content -LiteralPath $logPath -ErrorAction SilentlyContinue |
    Select-String -Pattern 'COVERAGE|static blocks|SVCs to HLE|static ->|JIT ->|blocks per transition|import traps|distinct|by execution|^s+[0-9A-F]{8}|Using ArmRecomp' |
    ForEach-Object { $_.Line }
