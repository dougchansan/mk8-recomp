# Capture the suyu window and report whether anything is actually being drawn.
#
# "Does it render a frame" cannot be answered from the log alone: suyu reports
# first_frame_displayed as soon as the presentation path runs once, whether or
# not the frame has content in it. This grabs the window's pixels and reports
# how many distinct colours are in them, which separates a real frame from a
# cleared buffer.
#
#   .\scripts\capture-window.ps1 -Out shot.png
#   .\scripts\capture-window.ps1 -Repeat 6 -IntervalSeconds 15 -OutDir $env:TEMP\mk8r-shots

[CmdletBinding()]
param(
    [string]$Out = '',
    [string]$OutDir = '',
    [int]$Repeat = 1,
    [int]$IntervalSeconds = 10
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

# GetWindowRect rather than the form bounds: the render surface is a child of
# the main window and PrintWindow on the parent misses it under Vulkan, so this
# grabs the screen region the window occupies instead.
if (-not ('Win32Rect' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public class Win32Rect {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
}
'@
}

function Get-SuyuWindow {
    $p = Get-Process suyu -ErrorAction SilentlyContinue |
         Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if (-not $p) { throw 'no suyu window found' }
    return $p
}

function Capture-One([string]$path) {
    $p = Get-SuyuWindow

    # Never steal focus, and never capture unless suyu is already frontmost.
    #
    # This used to call SetForegroundWindow and then copy that screen region.
    # Run periodically during a benchmark it did two bad things: it yanked focus
    # away from whatever the user was doing every few seconds, and when they
    # clicked back the copy grabbed their window instead of the emulator. A
    # benchmark left running captured the user's email.
    #
    # Screen copy is still the method - PrintWindow does not capture the Vulkan
    # surface - so the only safe rule is to skip the shot when suyu is not on
    # top rather than to force it there.
    if ([Win32Rect]::GetForegroundWindow() -ne $p.MainWindowHandle) {
        throw 'suyu is not the foreground window; skipping capture'
    }

    $r = New-Object Win32Rect+RECT
    if (-not [Win32Rect]::GetWindowRect($p.MainWindowHandle, [ref]$r)) {
        throw 'GetWindowRect failed'
    }
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    if ($w -le 0 -or $h -le 0) { throw "window has no area ($w x $h)" }

    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $h))
    $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)

    # Sample a grid rather than every pixel - enough to tell a cleared buffer
    # from a drawn frame without reading four million pixels in PowerShell.
    $colors = @{}
    for ($y = 0; $y -lt $h; $y += 8) {
        for ($x = 0; $x -lt $w; $x += 8) {
            $c = $bmp.GetPixel($x, $y).ToArgb()
            $colors[$c] = $true
        }
    }
    $bmp.Dispose()

    [pscustomobject]@{
        Path     = $path
        Width    = $w
        Height   = $h
        Distinct = $colors.Count
    }
}

if ($Repeat -le 1) {
    if (-not $Out) { $Out = Join-Path $PWD 'suyu-window.png' }
    Capture-One $Out
} else {
    if (-not $OutDir) { $OutDir = Join-Path $env:TEMP 'suyu-shots' }
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    for ($i = 1; $i -le $Repeat; $i++) {
        $path = Join-Path $OutDir ('shot-{0:d2}.png' -f $i)
        try {
            Capture-One $path
        } catch {
            [pscustomobject]@{ Path = $path; Width = 0; Height = 0; Distinct = -1 }
        }
        if ($i -lt $Repeat) { Start-Sleep -Seconds $IntervalSeconds }
    }
}
