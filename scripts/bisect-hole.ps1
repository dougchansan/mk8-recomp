# Narrow a static-recompiler fault to an address range inside one module.
#
# `SUYU_RECOMP_MAIN_HOLE=<lo>-<hi>` removes an address range from the module's
# lookup index. In hybrid mode an address with no entry falls through to the
# JIT, so the holed range runs under dynarmic while everything else stays
# static. Binary search on the boundary narrows the fault without a re-export
# and without rebuilding the module - the hole is read at runtime.
#
# This is the second level of the same idea as bisect-modules.ps1. That one
# finds the faulty module by staging a subset of images; this one finds the
# faulty code inside a module.
#
#   .\scripts\bisect-hole.ps1 -Target title-01006a800016e000 -Rom ... `
#       -Lo 0x2a1683c -Hi 0x2a6b7e0
#
# CONTROLS ARE NOT OPTIONAL. Run -Controls first and check both:
#   full hole  - the whole module holed. Must reach the milestone, proving the
#                hole actually routes addresses to the JIT.
#   no hole    - nothing holed. Must reproduce the failure, proving the harness
#                did not accidentally fix it.
# Skipping these is how a bisection measures nothing. A previous handover
# bisect on this project ran 24 arms that could never hand over at all, because
# SUYU_RECOMP_STRICT returns PrefetchAbort instead of falling back.
#
# READ jit_transitions ON EVERY ARM. An arm reporting ZERO transitions never
# executed anything inside its hole, so it says nothing about that range -
# it is not evidence the range is innocent. Four arms of the Smash bisection
# were uninformative for exactly this reason, including the one that appeared
# to clear atomics.
#
# The milestone is not binary. Smash produced three distinct outcomes - no
# frame at all, reaching the versus splash, and a real match - and an arm can
# satisfy one without the others. Judge with tas_peak_frame plus the captures,
# not a single pass flag.

[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][string]$Rom,
    [string]$TasFixtureDirectory = '',
    [string]$Lo = '',
    [string]$Hi = '',
    # Substring of the module the offsets belong to. Modules register under the
    # NSO's own name, so this is `cross2_Release.nss` for Smash's main, not
    # `main`. Empty holes every module, which is what the all-to-JIT control
    # wants but is wrong for a search arm.
    [string]$Module = '',
    [switch]$Controls,
    [int]$RunSeconds = 600,
    [string]$OutDir = ''
)

if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$ErrorActionPreference = 'Stop'
if (-not $OutDir) { $OutDir = Join-Path $Root "local\bench\hole-$Target" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Preflight: prove the binary reads the hole at all.
#
# This script was committed against an implementation that only ever existed in
# an uncommitted working tree. Every arm it ran afterwards was an ordinary
# all-static run: suyu ignored the variable, nothing fell through to the JIT,
# and each arm reported zero transitions - indistinguishable from an arm whose
# range simply never executed. A whole bisection can be spent on that.
$exe = Join-Path $Root 'build\suyu\bin\suyu.exe'
if (Test-Path -LiteralPath $exe) {
    $bytes = [System.IO.File]::ReadAllBytes($exe)
    $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
    if ($ascii -notmatch 'SUYU_RECOMP_MAIN_HOLE') {
        throw ("$exe does not contain SUYU_RECOMP_MAIN_HOLE. The hole would be " +
               'ignored and every arm would report zero transitions. Rebuild suyu.')
    }
    Write-Host "preflight ok: $exe reads SUYU_RECOMP_MAIN_HOLE" -ForegroundColor DarkGray
} else {
    Write-Host "preflight skipped: $exe not found" -ForegroundColor Yellow
}

function Invoke-Hole([string]$label, [string]$range) {
    Get-Process suyu* -ErrorAction SilentlyContinue | Stop-Process -Force
    # A force-killed suyu keeps MCP port 9742 bound for several seconds after
    # the process is gone; starting too soon fails with "Emulation is already
    # running" while Get-Process shows nothing.
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        if (-not (Test-NetConnection 127.0.0.1 -Port 9742 -WarningAction SilentlyContinue).TcpTestSucceeded) { break }
        Start-Sleep -Seconds 2
    }

    Write-Host ""
    Write-Host "=== $label  hole=$(if ($range) { $range } else { '(none)' }) ===" -ForegroundColor Cyan
    if ($range) { $env:SUYU_RECOMP_MAIN_HOLE = $range }
    else { Remove-Item Env:\SUYU_RECOMP_MAIN_HOLE -ErrorAction SilentlyContinue }
    if ($Module) { $env:SUYU_RECOMP_HOLE_MODULE = $Module }
    else { Remove-Item Env:\SUYU_RECOMP_HOLE_MODULE -ErrorAction SilentlyContinue }

    $evidence = Join-Path $OutDir $label
    $a = @{
        Root = $Root; Target = $Target; Rom = $Rom
        RunSeconds = $RunSeconds; EvidenceRoot = $evidence; NoStrict = $true
        CaptureSeconds = @(60, 120, 180)
    }
    if ($TasFixtureDirectory) {
        $a['TasReplay'] = $true; $a['TasFixtureDirectory'] = $TasFixtureDirectory
    }
    try { & (Join-Path $Root 'scripts\playtest-static-title.ps1') @a *> (Join-Path $OutDir "$label.log") }
    catch { Write-Host "  threw: $_" -ForegroundColor Yellow }
    finally {
        Remove-Item Env:\SUYU_RECOMP_MAIN_HOLE -ErrorAction SilentlyContinue
        Remove-Item Env:\SUYU_RECOMP_HOLE_MODULE -ErrorAction SilentlyContinue
    }

    $run = Get-ChildItem $evidence -Directory -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -match '^\d{8}T\d{6}Z$' } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $run) { Write-Host '  no evidence'; return $null }
    $o = Get-Content (Join-Path $run.FullName 'observation.json') -Raw | ConvertFrom-Json
    # Did suyu actually arm the hole? An unarmed hole reports exactly what a
    # range that never executes reports, and the two were confused for a whole
    # bisection once. The emulator says so in its own log; read it rather than
    # inferring it from the counters.
    $slog = Join-Path $run.FullName 'suyu_log.txt'
    $armed = $null
    if ($range -and (Test-Path -LiteralPath $slog)) {
        $armed = [bool](Select-String -LiteralPath $slog -Pattern 'main hole .* armed on' -Quiet)
    }
    $r = [pscustomobject]@{
        Arm = $label; Hole = $range; Outcome = $o.tas_outcome
        FirstFrame = $o.first_frame; PeakFrame = $o.tas_peak_frame
        Blocks = $o.max_static_blocks; JitTr = $o.max_jit_transitions
        Armed = $armed
        Informative = ($o.max_jit_transitions -gt 0); Evidence = $run.Name
    }
    if ($range -and $armed -eq $false) {
        Write-Host '  HOLE NEVER ARMED - suyu did not apply it. Arm is void, not innocent.' -ForegroundColor Red
    }
    Write-Host ("  frame={0} peak={1} blocks={2} jit={3}{4}" -f `
        $r.FirstFrame, $r.PeakFrame, $r.Blocks, $r.JitTr,
        $(if (-not $r.Informative) { '   <- ZERO TRANSITIONS: hole never executed, arm says nothing' } else { '' }))
    return $r
}

$results = @()
if ($Controls) {
    # 0-0x7fffffff covers any plausible module extent.
    $results += Invoke-Hole 'ctl-fullhole' '0x0-0x7fffffff'
    $results += Invoke-Hole 'ctl-nohole'   ''
} else {
    if (-not $Lo -or -not $Hi) { throw 'give -Lo and -Hi, or -Controls' }
    $results += Invoke-Hole "hole-$Lo-$Hi" "$Lo-$Hi"
}

Write-Host ""
Write-Host '=== arms ===' -ForegroundColor Green
$results | Where-Object { $_ } | Format-Table -AutoSize | Out-String -Width 200 | Write-Host
Write-Host 'Judge by tas_peak_frame AND the captures. Discard any arm with JitTr=0.'
$results | Where-Object { $_ } | ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath (Join-Path $OutDir 'arms.json')
