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
    [int]$WaitSeconds = 120,
    # Passed through to suyu's log_filter. '*:Info Render:Debug' is the useful
    # one for graphics questions; a bare '*:Debug' floods the log with kernel
    # traffic and slows the run enough to change what it measures.
    [string]$LogFilter = '',
    # Load only these modules, by directory name (e.g. main,rtld). Everything
    # else runs on the JIT, which is how a wrong answer gets localised to one
    # module without rebuilding anything.
    [string[]]$Only = @(),
    # Remove the frame limiter and vsync. At the default 100% cap both engines
    # sit at exactly 60 FPS and are indistinguishable by construction, so this is
    # the only way to see whether recompiled code is actually faster than the
    # JIT or merely fast enough.
    [switch]$Unlimited
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

    # A subset is staged as hard links rather than copies: the main image alone
    # is 200 MB and the point of bisecting is to iterate quickly. Directory
    # junctions would be cheaper still, but neither Get-ChildItem -Recurse nor
    # suyu's own scan follows a reparse point, so the staged modules would be
    # invisible and the run would silently be a baseline.
    if ($Only.Count -gt 0) {
        $stage = Join-Path $Root 'build\recomp-stage'
        if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Force -Recurse }
        New-Item -ItemType Directory -Force -Path $stage | Out-Null
        foreach ($m in $Only) {
            $src = Join-Path $RecompIn $m
            if (-not (Test-Path -LiteralPath $src)) { throw "No such module: $m" }
            $dstDir = Join-Path $stage $m
            New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
            Get-ChildItem -LiteralPath $src -Filter '*.dll' -File | ForEach-Object {
                New-Item -ItemType HardLink -Path (Join-Path $dstDir $_.Name) `
                         -Target $_.FullName | Out-Null
            }
        }
        $RecompIn = $stage
        Write-Host ("SUBSET: only " + ($Only -join ', ')) -ForegroundColor Magenta
    }

    $dlls = Get-ChildItem -LiteralPath $RecompIn -Recurse -Filter '*.dll'
    if (-not $dlls) { throw "No DLLs under $RecompIn" }
    $env:SUYU_RECOMP_DIR = $RecompIn
    Write-Host "HYBRID: $($dlls.Count) AOT modules from $RecompIn" -ForegroundColor Green
    $dlls | ForEach-Object { '    {0,-28} {1,12:n0} bytes' -f $_.Directory.Name, $_.Length }
}

# Same story as the log filter: suyu rewrites qt-config.ini on exit, so this is
# set per run. The \default flags matter - a setting whose companion default is
# true is ignored, which is how the log_filter change silently did nothing the
# first time it was tried.
if ($Unlimited) {
    $cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
    if (Test-Path -LiteralPath $cfg) {
        $text = Get-Content -LiteralPath $cfg -Raw
        $text = $text -replace 'use_speed_limit\\default=true', 'use_speed_limit\default=false'
        $text = $text -replace 'use_speed_limit=true', 'use_speed_limit=false'
        $text = $text -replace 'use_vsync\\default=true', 'use_vsync\default=false'
        $text = $text -replace 'use_vsync=\d+', 'use_vsync=0'
        [System.IO.File]::WriteAllText($cfg, $text, (New-Object System.Text.UTF8Encoding $false))
        Write-Host 'UNLIMITED: frame limiter and vsync off' -ForegroundColor Yellow
    }
}

# suyu rewrites qt-config.ini on exit, so the filter has to be set before every
# run rather than once. log_filter\default=true makes the value itself ignored.
if ($LogFilter) {
    $cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
    if (Test-Path -LiteralPath $cfg) {
        $text = Get-Content -LiteralPath $cfg -Raw
        $text = $text -replace 'log_filter\\default=true', 'log_filter\default=false'
        $text = $text -replace 'log_filter=.*', ('log_filter="' + $LogFilter + '"')
        # -Encoding utf8 writes a BOM on PowerShell 5.1, which corrupts the
        # first line of the file. Write the bytes without one.
        [System.IO.File]::WriteAllText($cfg, $text, (New-Object System.Text.UTF8Encoding $false))
        Write-Host "log_filter set to $LogFilter"
    }
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
if (-not $p.WaitForExit(240000)) {
    Write-Warning 'suyu did not exit within 90s; killing (coverage report will be missing)'
    $p.Kill()
}
Start-Sleep -Seconds 3

Write-Host ''
Write-Host '=== coverage report ===' -ForegroundColor Cyan
Get-Content -LiteralPath $logPath -ErrorAction SilentlyContinue |
    Select-String -Pattern 'COVERAGE|static blocks|SVCs to HLE|static ->|JIT ->|blocks per transition|import traps|distinct|by execution|^s+[0-9A-F]{8}|Using ArmRecomp' |
    ForEach-Object { $_.Line }
