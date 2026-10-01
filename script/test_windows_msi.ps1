# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Architecture)
$ErrorActionPreference='Stop'
$id=[Guid]::NewGuid().ToString('N')
$temporary=Join-Path $env:TEMP ('EWAF-msi-tests-'+$id)
New-Item -ItemType Directory $temporary | Out-Null
$installation=Join-Path $env:LOCALAPPDATA ('TLO Labs/EWAF-Qualification-'+$id)
$registryKey='Software\tlolabs\EWAFQualification\'+$id
$upgrade=[Guid]::NewGuid().ToString('B')
$installed=$null
function Run-Msi([string]$mode,[string]$package,[int]$expected=0) {
    $log=Join-Path $temporary ((Split-Path $package -Leaf)+'.'+$mode.TrimStart('/')+'.log')
    $process=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/msiexec.exe') -ArgumentList @($mode,('"'+$package+'"'),'/qn','/l*v',('"'+$log+'"'),'REBOOT=ReallySuppress','MSIRESTARTMANAGERCONTROL=Disable') -Wait -PassThru
    if ($process.ExitCode -ne $expected) { Get-Content $log -Tail 100; throw "MSI returned $($process.ExitCode), expected $expected" }
}
function Build-Fixture([string]$version,[bool]$fail) {
    [xml]$xml=Get-Content ('build/ewaf-'+$Architecture+'.wxs') -Raw
    $namespace='http://wixtoolset.org/schemas/v4/wxs'
    $manager=[Xml.XmlNamespaceManager]::new($xml.NameTable); $manager.AddNamespace('w',$namespace)
    $package=$xml.SelectSingleNode('//w:Package',$manager)
    $package.SetAttribute('Name','EWAF Installer Qualification '+$id)
    $package.SetAttribute('Version',$version); $package.SetAttribute('UpgradeCode',$upgrade)
    $xml.SelectSingleNode('//w:Directory[@Id="INSTALLFOLDER"]',$manager).SetAttribute('Name','EWAF-Qualification-'+$id)
    foreach ($component in $xml.SelectNodes('//w:Component',$manager)) {
        # Fixed within this test family; separate from all production components.
        $hash=[Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($id+'/'+$component.GetAttribute('Id')))
        $component.SetAttribute('Guid',[Guid]::new([byte[]]($hash[0..15])).ToString('B'))
    }
    foreach ($registry in $xml.SelectNodes('//w:RegistryValue',$manager)) { $registry.SetAttribute('Key',$registryKey) }
    foreach ($shortcut in @($xml.SelectNodes('//w:Shortcut',$manager))) { [void]$shortcut.ParentNode.RemoveChild($shortcut) }
    # A version-specific unversioned file makes successful replacement and rollback observable.
    $marker=Join-Path $temporary ('marker-'+$version+'.txt'); [IO.File]::WriteAllText($marker,$version)
    $component=$xml.CreateElement('Component',$namespace)
    $component.SetAttribute('Id','QualificationMarker'); $component.SetAttribute('Guid',$markerGuid); $component.SetAttribute('Bitness','always64')
    $file=$xml.CreateElement('File',$namespace); $file.SetAttribute('Id','QualificationMarkerFile'); $file.SetAttribute('Name','qualification-version.txt'); $file.SetAttribute('Source',$marker)
    [void]$component.AppendChild($file)
    $registry=$xml.CreateElement('RegistryValue',$namespace)
    foreach ($entry in @{Root='HKCU';Key=$registryKey;Name='Marker';Type='integer';Value='1';KeyPath='yes'}.GetEnumerator()) { $registry.SetAttribute($entry.Key,$entry.Value) }
    [void]$component.AppendChild($registry)
    [void]$xml.SelectSingleNode('//w:Directory[@Id="INSTALLFOLDER"]',$manager).AppendChild($component)
    $reference=$xml.CreateElement('ComponentRef',$namespace); $reference.SetAttribute('Id','QualificationMarker')
    [void]$xml.SelectSingleNode('//w:Feature',$manager).AppendChild($reference)
    if ($fail) {
        $custom=$xml.CreateElement('CustomAction',$namespace)
        foreach ($entry in @{Id='FailAfterCopy';Directory='SystemFolder';ExeCommand='"[SystemFolder]cmd.exe" /c exit 1';Execute='deferred';Return='check';Impersonate='yes'}.GetEnumerator()) { $custom.SetAttribute($entry.Key,$entry.Value) }
        [void]$package.AppendChild($custom)
        $sequence=$xml.CreateElement('InstallExecuteSequence',$namespace)
        $action=$xml.CreateElement('Custom',$namespace)
        $action.SetAttribute('Action','FailAfterCopy'); $action.SetAttribute('After','InstallFiles'); $action.SetAttribute('Condition','NOT Installed')
        [void]$sequence.AppendChild($action); [void]$package.AppendChild($sequence)
    }
    $source=Join-Path $temporary ($version+'.wxs'); $xml.Save($source)
    $output=Join-Path $temporary ($version+'.msi')
    & wix build -arch $Architecture.ToLowerInvariant() $source -o $output | Write-Host
    if ($LASTEXITCODE -ne 0) { throw 'Qualification MSI build failed.' }
    return $output
}
$markerGuid=[Guid]::NewGuid().ToString('B')
try {
    python script/package_windows_msi.py --arch $Architecture --emit-only
    if ($LASTEXITCODE -ne 0) { throw 'Production MSI source generation failed.' }
    $first=Build-Fixture '1.0.0' $false
    $second=Build-Fixture '1.0.1' $false
    $failure=Build-Fixture '1.0.2' $true
    Run-Msi '/i' $first; $installed=$first
    $marker=Join-Path $installation 'qualification-version.txt'
    if ([IO.File]::ReadAllText($marker) -ne '1.0.0') { throw 'Initial MSI installation failed.' }
    $generated=Join-Path $installation '09-03-2026'; New-Item -ItemType Directory $generated | Out-Null
    $contents=Join-Path $generated 'keep.txt'; [IO.File]::WriteAllText($contents,'Preserve my folders')
    Run-Msi '/i' $second; $installed=$second
    if ([IO.File]::ReadAllText($marker) -ne '1.0.1') { throw 'MSI major upgrade failed.' }
    Run-Msi '/i' $failure 1603
    if ([IO.File]::ReadAllText($marker) -ne '1.0.1') { throw 'Failed transaction did not restore the previous version.' }
    if ([IO.File]::ReadAllText($contents) -ne 'Preserve my folders') { throw 'Upgrade or rollback changed user contents.' }
    Run-Msi '/x' $second; $installed=$null
    if ((Test-Path $marker) -or !(Test-Path $contents)) { throw 'Uninstall failed to preserve only user contents.' }
    Write-Host 'Real MSI installation, major upgrade, deferred failure rollback, and removal preserve user contents.'
} finally {
    if ($installed) { Run-Msi '/x' $installed }
    # Only these uniquely named test artifacts are removed.
    Remove-Item -LiteralPath $installation -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath ('HKCU:\'+$registryKey) -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
