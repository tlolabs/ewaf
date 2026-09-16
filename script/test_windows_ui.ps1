param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$process=Start-Process -FilePath (Resolve-Path $Executable) -PassThru
function Wait-Element([System.Windows.Automation.AutomationElement]$Root,[string]$Name) {
    $condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty,$Name)
    $deadline=(Get-Date).AddSeconds(20)
    do { $element=$Root.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition); if($element) { return $element }; Start-Sleep -Milliseconds 100 } while((Get-Date) -lt $deadline)
    throw "Accessible control not found: $Name"
}
function Set-Text($Element,[string]$Text) {
    $pattern=$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $pattern.SetValue($Text)
}
try {
    $root=[System.Windows.Automation.AutomationElement]::RootElement
    $condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$process.Id)
    $deadline=(Get-Date).AddSeconds(30)
    do { $window=$root.FindFirst([System.Windows.Automation.TreeScope]::Children,$condition); if($window) { break }; Start-Sleep -Milliseconds 100 } while((Get-Date) -lt $deadline)
    if(!$window) { throw 'EWAF did not expose a native window within 30 seconds.' }
    $start=Wait-Element $window 'Start date'
    $end=Wait-Element $window 'End date'
    Set-Text $start '09-01-2026'; Set-Text $end '09-30-2026'
    $create=Wait-Element $window 'Create Folders'
    $deadline=(Get-Date).AddSeconds(10)
    while(!$create.Current.IsEnabled -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if(!$create.Current.IsEnabled) { throw 'Valid date range did not enable creation.' }
    Set-Text $start '02-29-2025'
    $deadline=(Get-Date).AddSeconds(10)
    while($create.Current.IsEnabled -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if($create.Current.IsEnabled) { throw 'Invalid date did not disable creation.' }
    Wait-Element $window 'Weekday' | Out-Null
    Wait-Element $window 'Find a folder date' | Out-Null
    Wait-Element $window 'Choose Destination…' | Out-Null
    Write-Host 'WinUI launch, accessible controls and date validation passed.'
} finally {
    if($window) { try { $window.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).Close() } catch {} }
    if(!$process.HasExited) { $process.WaitForExit(5000) | Out-Null }
    if(!$process.HasExited) { Stop-Process -Id $process.Id }
}
