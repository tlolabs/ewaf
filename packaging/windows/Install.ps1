# Per-user installation; run this from an extracted EWAF release directory.
$ErrorActionPreference='Stop'
$destination=Join-Path $env:LOCALAPPDATA 'Programs\EWAF'
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item (Join-Path $PSScriptRoot '*') $destination -Recurse -Force
$shortcut=Join-Path ([Environment]::GetFolderPath('Programs')) 'EWAF.lnk'
$shell=New-Object -ComObject WScript.Shell
$link=$shell.CreateShortcut($shortcut)
$link.TargetPath=Join-Path $destination 'EWAF.exe'
$link.WorkingDirectory=$destination
$link.Save()
Write-Host "Installed EWAF for the current user. Open EWAF from Start."
