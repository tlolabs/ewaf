# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory)][string]$PackageDirectory,
      [Parameter(Mandatory)][string]$Msi,
      [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string]$Architecture)
$ErrorActionPreference='Stop'
if ($env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Certificate trust fixtures require an isolated GitHub-hosted runner.' }
$temporary=Join-Path $env:TEMP ('EWAF-updater-tests-'+[Guid]::NewGuid())
New-Item -ItemType Directory $temporary | Out-Null
$helper=(Resolve-Path (Join-Path $PackageDirectory 'updater-installer/ewaf-installer.exe')).Path
$version=(python script/version.py).Trim()
$cert=$null; $app=$null
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
    # Ephemeral current-user trust exists only on the isolated CI runner. Production
    # enrollment, keys and application configuration are never modified.
    Write-Host 'Creating isolated fixture certificate'
    $cert=New-SelfSignedCertificate -Type CodeSigningCert -Subject ('CN=EWAF Qualification '+[Guid]::NewGuid()) -CertStoreLocation Cert:\CurrentUser\My
    # X509Store.Add to CurrentUser Root can wait for an invisible desktop consent
    # dialog. Explicit certutil enrollment is confined to this disposable runner;
    # the application's WinVerifyTrust path and revocation policy stay unchanged.
    Write-Host 'Enrolling only the disposable fixture public certificate'
    $publicCertificate=Join-Path $temporary 'fixture.cer'
    [IO.File]::WriteAllBytes($publicCertificate,$cert.Export([Security.Cryptography.X509Certificates.X509ContentType]::Cert))
    & certutil -f -user -addstore Root $publicCertificate
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
        $hostProcess.StandardInput.WriteLine('ABORT'); $hostProcess.StandardInput.Flush()
        if (!$hostProcess.WaitForExit(10000) -or $hostProcess.ExitCode -ne 0) { throw 'Installer did not abort normally.' }
        if ($app.HasExited) { throw 'Aborted installation terminated EWAF.' }
    } finally { $hostProcess.StandardInput.Close(); $hostProcess.Dispose() }
    Write-Host 'Native trust, digest, publisher, version, architecture, and abort-handshake tests passed.'
} finally {
    if ($app -and !$app.HasExited) {
        [void]$app.CloseMainWindow()
        if (!$app.WaitForExit(10000)) { Write-Warning 'EWAF did not close normally after the handshake test.' }
    }
    if ($cert) {
        Remove-Item -LiteralPath ('Cert:\CurrentUser\Root\'+$cert.Thumbprint) -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath ('Cert:\CurrentUser\My\'+$cert.Thumbprint) -DeleteKey -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
