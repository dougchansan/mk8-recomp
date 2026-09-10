# Launch the suyu Qt frontend so its MCP server comes up.
#
# The mode flag is not optional for automation. On first launch a modal
# ModeSelector blocks startup, and ApplyAppMode - which starts the MCP listener
# - never runs. main.cpp:8059-8083 parses -gamer/-hacker/-programmer for exactly
# this case.

[CmdletBinding()]
param(
    [string]$Root = 'G:\mk8-recomp',
    [ValidateSet('hacker','gamer','programmer')]
    [string]$Mode = 'hacker',
    [int]$WaitSeconds = 90,
    [switch]$DebugLog
)

$ErrorActionPreference = 'Stop'

$Exe = Join-Path $Root 'build\suyu\bin\suyu.exe'
if (-not (Test-Path -LiteralPath $Exe)) { throw "Not built: $Exe" }

Get-Process suyu -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2

# suyu-cmd never writes its log file, and the GUI defaults to *:Info. Debug is
# what actually names the failing subsystem - it is how the titlekek crash in
# issue #2 was pinned down.
if ($DebugLog) {
    $cfg = Join-Path $env:APPDATA 'suyu\config\qt-config.ini'
    if (Test-Path -LiteralPath $cfg) {
        $text = Get-Content -LiteralPath $cfg -Raw
        $text = $text -replace 'log_filter\\default=true', 'log_filter\default=false'
        $text = $text -replace 'log_filter=.*', 'log_filter="*:Debug"'
        Set-Content -LiteralPath $cfg -Value $text -Encoding utf8
        Write-Host 'log_filter set to *:Debug'
    }
}

$p = Start-Process -FilePath $Exe -ArgumentList "-$Mode" -PassThru
Write-Host "suyu pid $($p.Id), mode $Mode"

$deadline = (Get-Date).AddSeconds($WaitSeconds)
while ((Get-Date) -lt $deadline) {
    $ok = (Test-NetConnection -ComputerName 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded
    if ($ok) {
        Write-Host 'MCP server listening on 127.0.0.1:9742' -ForegroundColor Green
        return [pscustomobject]@{ Pid = $p.Id; Mcp = $true }
    }
    Start-Sleep -Milliseconds 500
}

Write-Warning "MCP server did not come up within ${WaitSeconds}s (a modal dialog may be blocking startup)."
[pscustomobject]@{ Pid = $p.Id; Mcp = $false }
