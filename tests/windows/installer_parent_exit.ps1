# SPDX-License-Identifier: GPL-3.0-or-later
# Disposable native integration fixture. No trust or identity bypasses.
param([Parameter(Mandatory)][string]$Configuration)
$ErrorActionPreference='Stop'
if ($env:RUNNER_ENVIRONMENT -ne 'github-hosted') { throw 'Requires a disposable GitHub-hosted Windows runner.' }
$config=Get-Content -LiteralPath $Configuration -Raw | ConvertFrom-Json
$parent=Get-Process -Id $PID
$start=[Diagnostics.ProcessStartInfo]::new($config.helper)
$start.UseShellExecute=$false; $start.CreateNoWindow=$true
$start.RedirectStandardInput=$true; $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
foreach ($arg in @('--install',$config.package,$config.digest,$config.publisher,$config.version,$config.arch,$PID.ToString(),$parent.StartTime.ToUniversalTime().Ticks.ToString())) { $start.ArgumentList.Add($arg) }
$installer=[Diagnostics.Process]::Start($start)
function Read-Protocol([string]$expected) {
    $line=$installer.StandardOutput.ReadLineAsync()
    if (!$line.Wait(120000) -or $line.Result -ne $expected) { throw "Installer did not acknowledge $expected" }
}
try {
    Read-Protocol 'READY'
    $installer.StandardInput.WriteLine('COMMIT'); $installer.StandardInput.Flush()
    Read-Protocol 'COMMITTED'
    @{installer_pid=$installer.Id; parent_pid=$PID} | ConvertTo-Json | Set-Content -LiteralPath ($config.receipt+'.tmp')
    [IO.File]::Move($config.receipt+'.tmp',$config.receipt)
    # Allow the supervisor to retain the child's OS handle before this real
    # parent exits, so the test can assert the actual installation exit code.
    $deadline=[DateTime]::UtcNow.AddSeconds(30)
    while (!(Test-Path -LiteralPath ($config.receipt+'.exit'))) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Supervisor did not capture installer handle.' }
        Start-Sleep -Milliseconds 20
    }
} finally { $installer.StandardInput.Close(); $installer.Dispose() }
exit 0
