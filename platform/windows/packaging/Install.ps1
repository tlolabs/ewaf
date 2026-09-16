# Per-user installation from an extracted EWAF release directory.
param(
    [string]$Destination=(Join-Path $env:LOCALAPPDATA 'Programs\EWAF'),
    [string]$ShortcutPath=(Join-Path ([Environment]::GetFolderPath('Programs')) 'EWAF.lnk')
)
$ErrorActionPreference='Stop'
$owned=Get-Content (Join-Path $PSScriptRoot 'install-manifest.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force $Destination | Out-Null
$root=[IO.Path]::GetFullPath($Destination).TrimEnd('\')+'\'
foreach($relative in $owned) {
    $target=[IO.Path]::GetFullPath((Join-Path $root $relative))
    if([IO.Path]::IsPathRooted($relative) -or !$target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package manifest path.' }
    New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($target)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $relative) -Destination $target -Force
}
Copy-Item (Join-Path $PSScriptRoot 'install-manifest.json') $Destination -Force
$shell=New-Object -ComObject WScript.Shell
$link=$shell.CreateShortcut($ShortcutPath)
$link.TargetPath=Join-Path $Destination 'EWAF.exe'
$link.WorkingDirectory=$Destination
$link.Save()
Write-Host 'Installed EWAF for the current user. Open EWAF from Start.'
