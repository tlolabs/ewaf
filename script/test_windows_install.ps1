param([Parameter(Mandatory=$true)][string]$PackageDirectory)
$ErrorActionPreference='Stop'
$temporary=Join-Path $env:TEMP ('EWAF-install-test-'+[Guid]::NewGuid())
$destination=Join-Path $temporary 'app'
$shortcut=Join-Path $temporary 'EWAF.lnk'
New-Item -ItemType Directory -Force $temporary | Out-Null
try {
    & (Join-Path $PackageDirectory 'Install.ps1') -Destination $destination -ShortcutPath $shortcut
    if(!(Test-Path (Join-Path $destination 'EWAF.exe')) -or !(Test-Path $shortcut)) { throw 'Per-user installation failed.' }
    $generated=Join-Path $destination '09-03-2026'
    New-Item -ItemType Directory $generated | Out-Null
    $emptyGenerated=Join-Path $destination '09-10-2026'
    New-Item -ItemType Directory $emptyGenerated | Out-Null
    $contents=Join-Path $generated 'keep.txt'
    [IO.File]::WriteAllText($contents,'Keep my work')
    & (Join-Path $PackageDirectory 'Uninstall.ps1') -Destination $destination -ShortcutPath $shortcut
    if((Test-Path (Join-Path $destination 'EWAF.exe')) -or (Test-Path $shortcut)) { throw 'Uninstall left installed application files.' }
    if([IO.File]::ReadAllText($contents) -ne 'Keep my work') { throw 'Uninstall changed user contents.' }
    if(!(Test-Path $emptyGenerated -PathType Container)) { throw 'Uninstall removed an empty generated folder.' }
    Write-Host 'Per-user install/uninstall and preservation of generated folders passed.'
} finally { Remove-Item -LiteralPath $temporary -Recurse -Force }
