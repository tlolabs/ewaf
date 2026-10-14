# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory)][string]$PackageDirectory,
      [Parameter(Mandatory)][string]$Msi,
      [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Architecture)
$ErrorActionPreference='Stop'
if ($env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Certificate trust fixtures require an isolated GitHub-hosted runner.' }
$temporary=Join-Path $env:TEMP ('EWAF-updater-tests-验证-é-'+[Guid]::NewGuid())
New-Item -ItemType Directory $temporary | Out-Null
$helper=(Resolve-Path (Join-Path $PackageDirectory 'updater-installer/ewaf-installer.exe')).Path
$version=(python script/version.py).Trim()
$cert=$null; $app=$null; $committedInstaller=$null; $ownedInstallRoot=$null
function Check-Helper([string]$package,[string]$digest,[string]$publisher,[string]$targetVersion,[string]$arch,[string]$expected) {
    Write-Host "Checking native verifier: $expected ($targetVersion / $arch)"
    $errors=Join-Path $temporary 'verify-error.json'
    $output=& $helper --verify-only $package $digest $publisher $targetVersion $arch 2> $errors
    $code=$LASTEXITCODE
    if ($expected -eq 'valid') {
        if ($code -ne 0 -or !(($output | ConvertFrom-Json).verified)) { throw "Valid MSI verification failed: $(Get-Content $errors -Raw)" }
    } else {
        if ($code -eq 0) { throw "Expected rejection: $expected" }
        $failure=Get-Content $errors -Raw | ConvertFrom-Json
        if ($failure.code -ne $expected) { throw "Expected $expected, received $($failure.code): $($failure.error)" }
    }
}
try {
    $unsigned=Join-Path $temporary 'unsigned.msi'
    [IO.File]::WriteAllText($unsigned,'This is not a signed installer')
    Check-Helper $unsigned (Get-FileHash $unsigned).Hash 'CN=No publisher' $version $Architecture.ToLowerInvariant() 'signature'
    # Ephemeral fixture trust exists only on the isolated CI runner. Production
    # enrollment, keys and application configuration are never modified.
    Write-Host 'Creating isolated fixture certificate'
    $cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject ('CN=EWAF Qualification 验证 é '+[Guid]::NewGuid()) -CertStoreLocation Cert:\CurrentUser\My
    # CurrentUser Root enrollment waits for desktop consent even with certutil.
    # Administrative LocalMachine enrollment is confined to this disposable runner;
    # the application's WinVerifyTrust path and revocation policy stay unchanged.
    Write-Host 'Enrolling only the disposable fixture public certificate'
    $publicCertificate=Join-Path $temporary 'fixture.cer'
    [IO.File]::WriteAllBytes($publicCertificate,$cert.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert))
    & certutil -f -addstore Root $publicCertificate
    if ($LASTEXITCODE -ne 0) { throw 'Fixture public-certificate enrollment failed.' }
    $signtool=(Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" | Sort-Object FullName -Descending | Select-Object -First 1).FullName
    if (!$signtool) { throw 'Windows SDK signtool is required.' }
    $signed=Join-Path $temporary 'signed.msi'
    Copy-Item $Msi $signed
    & $signtool sign /fd SHA256 /sha1 $cert.Thumbprint $signed
    if ($LASTEXITCODE -ne 0) { throw 'Fixture signing failed.' }
    $digest=(Get-FileHash $signed).Hash
    $arch=$Architecture.ToLowerInvariant()
    Check-Helper $signed $digest $cert.Subject $version $arch 'valid'
    # Exercise parent junctions; checking only the final .msi's attributes misses them.
    $realDirectory=Join-Path $temporary 'ordinary'
    New-Item -ItemType Directory $realDirectory | Out-Null
    Copy-Item $signed (Join-Path $realDirectory 'signed.msi')
    $junction=Join-Path $temporary 'junction'
    New-Item -ItemType Junction -Path $junction -Target $realDirectory | Out-Null
    try { Check-Helper (Join-Path $junction 'signed.msi') $digest $cert.Subject $version $arch 'package' }
    finally { (Get-Item -LiteralPath $junction).Delete() }

    Check-Helper $signed ('0'*64) $cert.Subject $version $arch 'digest'
    Check-Helper $signed $digest 'CN=Another publisher' $version $arch 'publisher'
    Check-Helper $signed $digest $cert.Subject '0.0.0' $arch 'identity'
    $wrongArch=if ($arch -eq 'x64') {'arm64'} else {'x64'}
    Check-Helper $signed $digest $cert.Subject $version $wrongArch 'identity'

    $wrongApp=Join-Path $temporary 'wrong-application.msi'
    Copy-Item $signed $wrongApp
    $windowsInstaller=New-Object -ComObject WindowsInstaller.Installer
    $database=$windowsInstaller.OpenDatabase($wrongApp,1)
    $view=$database.OpenView("UPDATE ``Property`` SET ``Value``='Another application' WHERE ``Property``='ProductName'")
    $view.Execute(); $view.Close(); $database.Commit()
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($view)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($database)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($windowsInstaller)
    & $signtool sign /fd SHA256 /sha1 $cert.Thumbprint $wrongApp
    if ($LASTEXITCODE -ne 0) { throw 'Wrong-application fixture signing failed.' }
    Check-Helper $wrongApp (Get-FileHash $wrongApp).Hash $cert.Subject $version $arch 'identity'

    # A real EWAF process remains open after the verified helper receives ABORT.
    # The packaged app and helper run with the runner's unchanged script policy.
    Write-Host 'Checking READY/ABORT with a running EWAF process'
    $app=Start-Process -FilePath (Join-Path $PackageDirectory 'EWAF.exe') -PassThru
    Start-Sleep -Seconds 3
    if ($app.HasExited) { throw 'EWAF failed to launch for installer handshake.' }
    $start=[Diagnostics.ProcessStartInfo]::new($helper)
    $start.UseShellExecute=$false; $start.CreateNoWindow=$true
    $start.RedirectStandardInput=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    foreach ($arg in @('--install',$signed,$digest,$cert.Subject,$version,$arch,$app.Id.ToString(),$app.StartTime.ToUniversalTime().Ticks.ToString())) { $start.ArgumentList.Add($arg) }
    $hostProcess=[Diagnostics.Process]::Start($start)
    try {
        $line=$hostProcess.StandardOutput.ReadLineAsync()
        if (!$line.Wait(120000)) { throw 'Native verification timed out before readiness.' }
        if ($line.Result -ne 'READY') { throw "No verified readiness handshake: $($hostProcess.StandardError.ReadToEnd())" }
        $replacementDenied=$false
        try { [IO.File]::WriteAllText($signed,'must never replace a verified MSI') }
        catch [IO.IOException] { $replacementDenied=$true }
        if (!$replacementDenied) { throw 'Verified MSI bytes were not locked against replacement.' }
        $hostProcess.StandardInput.WriteLine('ABORT'); $hostProcess.StandardInput.Flush()
        if (!$hostProcess.WaitForExit(10000) -or $hostProcess.ExitCode -ne 0) { throw 'Installer did not abort normally.' }
        if ($app.HasExited) { throw 'Aborted installation terminated EWAF.' }
    } finally { $hostProcess.StandardInput.Close(); $hostProcess.Dispose() }
    [void]$app.CloseMainWindow()
    if (!$app.WaitForExit(10000)) { throw 'EWAF must exit normally before the committed-install fixture.' }
    $installRoot=Join-Path $env:LOCALAPPDATA 'TLO Labs/EWAF'
    if (Test-Path -LiteralPath $installRoot) { throw 'Disposable fixture refuses to use an existing installation directory.' }
    $ownedInstallRoot=$installRoot
    $generated=Join-Path $installRoot '09-03-2026'
    New-Item -ItemType Directory -Path $generated -Force | Out-Null
    $preserved=Join-Path $generated 'keep.txt'
    [IO.File]::WriteAllText($preserved,'parent-exit fixture data')
    $commitPackage=Join-Path $temporary 'committed.msi'
    Copy-Item $signed $commitPackage
    $receipt=Join-Path $temporary 'parent-exit.json'
    $configuration=Join-Path $temporary 'parent-config.json'
    @{helper=$helper;package=$commitPackage;digest=$digest;publisher=$cert.Subject;version=$version;arch=$arch;receipt=$receipt} |
        ConvertTo-Json | Set-Content -LiteralPath $configuration
    $start=[Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    $start.UseShellExecute=$false
    foreach ($arg in @('-NoLogo','-NoProfile','-File',(Join-Path $PSScriptRoot '../tests/windows/installer_parent_exit.ps1'),$configuration)) { $start.ArgumentList.Add($arg) }
    $realParent=[Diagnostics.Process]::Start($start)
    $deadline=[DateTime]::UtcNow.AddSeconds(120)
    while (!(Test-Path -LiteralPath $receipt)) {
        if ($realParent.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Real parent did not complete installer commitment.' }
        Start-Sleep -Milliseconds 20
    }
    $handoff=Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    $committedInstaller=[Diagnostics.Process]::GetProcessById($handoff.installer_pid)
    [void]$committedInstaller.Handle
    [IO.File]::WriteAllText($receipt+'.exit','exit normally')
    if (!$realParent.WaitForExit(10000) -or $realParent.ExitCode -ne 0) { throw 'Real installer parent did not exit normally.' }
    if (!$committedInstaller.WaitForExit(120000) -or $committedInstaller.ExitCode -ne 0) { throw 'Native installer failed after normal parent exit.' }
    $installedApp=Join-Path $installRoot 'EWAF.exe'
    if ((Get-FileHash $installedApp).Hash -ne (Get-FileHash (Join-Path $PackageDirectory 'EWAF.exe')).Hash) { throw 'Installed executable differs from MSI source payload.' }
    if ([IO.File]::ReadAllText($preserved) -ne 'parent-exit fixture data') { throw 'Native parent-exit installation changed generated folder contents.' }
    Write-Host 'Native trust, Unicode, junction, locked-file, abort and real parent-exit installation fixtures passed.'

} finally {
    if ($committedInstaller -and !$committedInstaller.HasExited) {
        # This PID belongs solely to this disposable fixture, including an error
        # MessageBox. Do not leave a failed test blocking later runner cleanup.
        Stop-Process -Id $committedInstaller.Id -ErrorAction SilentlyContinue
    }
    if ($ownedInstallRoot) {
        $remove=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/msiexec.exe') -ArgumentList @('/x',('"'+$signed+'"'),'/qn','REBOOT=ReallySuppress','MSIRESTARTMANAGERCONTROL=Disable') -Wait -PassThru
        if ($remove.ExitCode -notin @(0,1605)) { Write-Warning "Fixture MSI removal failed: $($remove.ExitCode); retaining fixture directory for recovery." }
        else { Remove-Item -LiteralPath $ownedInstallRoot -Recurse -Force }
    }
    if ($app -and !$app.HasExited) {
        [void]$app.CloseMainWindow()
        if (!$app.WaitForExit(10000)) { Write-Warning 'EWAF did not close normally after the handshake test.' }
    }
    if ($cert) {
        Remove-Item -LiteralPath ('Cert:\LocalMachine\Root\'+$cert.Thumbprint) -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath ('Cert:\CurrentUser\My\'+$cert.Thumbprint) -DeleteKey -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
# Every native exit above has been asserted, including expected rejection codes.
# Do not let the Actions PowerShell wrapper reuse the last negative-test status.
exit 0
