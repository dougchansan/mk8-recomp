# Install one built AOT title into suyu's automatic title-ID cache.
[CmdletBinding()]
param(
    [string]$Root = $env:MK8R_ROOT,
    [Parameter(Mandatory)][string]$Target,
    [Parameter(Mandatory)][string]$TitleId,
    [string]$CacheRoot = ''
)

$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = [IO.Path]::GetFullPath($Root)
$buildRoot = [IO.Path]::GetFullPath((Join-Path $Root 'build\recomp'))
$source = [IO.Path]::GetFullPath((Join-Path $buildRoot $Target))
if (-not ($source.StartsWith($buildRoot + [IO.Path]::DirectorySeparatorChar,
                            [StringComparison]::OrdinalIgnoreCase))) {
    throw "target escapes the recomp build root: $Target"
}
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "no built AOT target at $source"
}

$normalizedTitleId = $TitleId.Trim().ToUpperInvariant()
if ($normalizedTitleId.StartsWith('0X')) { $normalizedTitleId = $normalizedTitleId.Substring(2) }
if ($normalizedTitleId -notmatch '^[0-9A-F]{16}$') {
    throw 'TitleId must be exactly 16 hexadecimal digits'
}

if (-not $CacheRoot) {
    if (-not $env:APPDATA) { throw 'APPDATA is not set; pass -CacheRoot explicitly' }
    $CacheRoot = Join-Path $env:APPDATA 'suyu\cache\aot'
}
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
$destination = [IO.Path]::GetFullPath((Join-Path $CacheRoot $normalizedTitleId))
if (-not ($destination.StartsWith($CacheRoot + [IO.Path]::DirectorySeparatorChar,
                                 [StringComparison]::OrdinalIgnoreCase))) {
    throw "destination escapes the AOT cache root: $destination"
}

New-Item -ItemType Directory -Force -Path $CacheRoot | Out-Null
if ((Get-Item -LiteralPath $CacheRoot -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw "AOT cache root is a reparse point: $CacheRoot"
}
if (Test-Path -LiteralPath $destination) {
    $redirect = @((Get-Item -LiteralPath $destination -Force)) +
        @(Get-ChildItem -LiteralPath $destination -Recurse -Directory -Force)
    $redirect = $redirect | Where-Object {
        $_.Attributes -band [IO.FileAttributes]::ReparsePoint
    } | Select-Object -First 1
    if ($redirect) {
        throw "refusing AOT cache tree containing a reparse point: $($redirect.FullName)"
    }
}

$images = @()
foreach ($module in Get-ChildItem -LiteralPath $source -Directory) {
    $matches = @(Get-ChildItem -LiteralPath $module.FullName -Recurse -File |
        Where-Object { $_.Name -like 'recompiled_*.dll' })
    foreach ($group in $matches | Group-Object Name) {
        # A multi-config build may leave Debug and Release copies. Install the
        # newest completed image for each module/name pair.
        $image = $group.Group | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        $images += [pscustomobject]@{ Module = $module.Name; File = $image }
    }
}
if ($images.Count -eq 0) { throw "no recompiled_*.dll images under $source" }

if (-not ('RecompImageAbiProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class RecompImageAbiProbe {
    [DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryW(string path);
    [DllImport("kernel32", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("kernel32")]
    private static extern bool FreeLibrary(IntPtr module);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint AbiFunction();

    public static uint Read(string path) {
        IntPtr module = LoadLibraryW(path);
        if (module == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        try {
            IntPtr address = GetProcAddress(module, "recomp_image_abi");
            if (address == IntPtr.Zero) return 0;
            var function = (AbiFunction)Marshal.GetDelegateForFunctionPointer(
                address, typeof(AbiFunction));
            return function();
        } finally {
            FreeLibrary(module);
        }
    }
}
'@
}
# suyu accepts ABI 5 or ABI 6 images, but never both in one bundle: ABI 6
# extends the shared GuestContext, so the manifest records the single ABI.
$bundleAbi = 0
foreach ($image in $images) {
    $abi = [RecompImageAbiProbe]::Read($image.File.FullName)
    if ($abi -ne 5 -and $abi -ne 6) {
        throw "$($image.File.FullName) exports image ABI $abi, not 5 or 6; regenerate it with the current emitter"
    }
    if ($bundleAbi -eq 0) {
        $bundleAbi = $abi
    } elseif ($abi -ne $bundleAbi) {
        throw "$($image.File.FullName) exports image ABI $abi but the bundle is ABI $bundleAbi; re-export every module together"
    }
}

New-Item -ItemType Directory -Force -Path $destination | Out-Null
# Remove only prior image files after the destination has been proven to live
# beneath the requested cache root. Manifests and module directories are kept.
Get-ChildItem -LiteralPath $destination -Recurse -File -Filter 'recompiled_*.dll' |
    Remove-Item -Force

$manifestImages = @()
foreach ($image in $images) {
    $moduleDir = Join-Path $destination $image.Module
    New-Item -ItemType Directory -Force -Path $moduleDir | Out-Null
    $installed = Join-Path $moduleDir $image.File.Name
    Copy-Item -LiteralPath $image.File.FullName -Destination $installed -Force
    $manifestImages += [ordered]@{
        module = $image.Module
        file = "$($image.Module)/$($image.File.Name)"
        bytes = (Get-Item -LiteralPath $installed).Length
        sha256 = (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$manifest = [ordered]@{
    title_id = $normalizedTitleId
    target = $Target
    image_abi = $bundleAbi
    staged_utc = (Get-Date).ToUniversalTime().ToString('o')
    images = $manifestImages
}
$manifest | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $destination 'bundle.json') -Encoding utf8

Write-Host "staged $($images.Count) ABI $bundleAbi AOT image(s) for $normalizedTitleId"
Write-Host $destination
$destination
