# Removes the per-user application. Generated folders and preferences are preserved.
$ErrorActionPreference='Stop'
$destination=Join-Path $env:LOCALAPPDATA 'Programs\EWAF'
$shortcut=Join-Path ([Environment]::GetFolderPath('Programs')) 'EWAF.lnk'
if(Test-Path $shortcut) { Remove-Item $shortcut }
if(Test-Path $destination) { Remove-Item $destination -Recurse -Force }
Write-Host 'EWAF removed. Your generated folders and settings were preserved.'
