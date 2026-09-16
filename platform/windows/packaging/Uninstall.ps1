# Remove only package-owned files. Generated directories and preferences survive.
param(
    [string]$Destination=(Join-Path $env:LOCALAPPDATA 'Programs\EWAF'),
    [string]$ShortcutPath=(Join-Path ([Environment]::GetFolderPath('Programs')) 'EWAF.lnk')
)
$ErrorActionPreference='Stop'
$manifest=Join-Path $Destination 'install-manifest.json'
if(!(Test-Path -LiteralPath $manifest)) { throw 'The EWAF installation manifest was not found.' }
$owned=Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
$root=[IO.Path]::GetFullPath($Destination).TrimEnd('\')+'\'
$directories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($relative in $owned) {
    $target=[IO.Path]::GetFullPath((Join-Path $root $relative))
    if([IO.Path]::IsPathRooted($relative) -or !$target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package manifest path.' }
    $parent=[IO.Path]::GetDirectoryName($target)
    while($parent.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) {
        $directories.Add($parent) | Out-Null
        $parent=[IO.Path]::GetDirectoryName($parent)
    }
    # Never recurse: an unexpected directory at a package-file path is user data.
    if(Test-Path -LiteralPath $target -PathType Leaf) { Remove-Item -LiteralPath $target -Force }
}
Remove-Item -LiteralPath $manifest -Force
if(Test-Path -LiteralPath $ShortcutPath) { Remove-Item -LiteralPath $ShortcutPath }
# Only remove empty package directories; even empty generated folders survive.
$directories | Sort-Object -Descending | ForEach-Object {
    if((Test-Path -LiteralPath $_ -PathType Container) -and !(Get-ChildItem -LiteralPath $_ -Force)) { Remove-Item -LiteralPath $_ }
}
if(!(Get-ChildItem -LiteralPath $Destination -Force)) { Remove-Item -LiteralPath $Destination }
Write-Host 'EWAF removed. Your generated folders and settings were preserved.'
