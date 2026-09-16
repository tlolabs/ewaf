param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$env:EWAF_TEST_SESSION=[Guid]::NewGuid().ToString()
$started=Get-Date
$env:EWAF_DIAGNOSTICS_PATH=Join-Path $env:TEMP "EWAF-startup-$env:EWAF_TEST_SESSION.log"
$process=Start-Process -FilePath (Resolve-Path $Executable) -PassThru
function Wait-Element([System.Windows.Automation.AutomationElement]$Root,[string]$Name,[System.Windows.Automation.AutomationProperty]$Property=[System.Windows.Automation.AutomationElement]::NameProperty) {
    $condition=New-Object System.Windows.Automation.PropertyCondition($Property,$Name)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    do { $element=$Root.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition); if($element) { return $element }; Start-Sleep -Milliseconds 100 } while($timer.Elapsed.TotalSeconds -lt 30)
    throw "Accessible control not found: $Name"
}
function Wait-State([scriptblock]$Predicate,[string]$Message) {
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $stable=0
    do {
        if(& $Predicate) { $stable++ } else { $stable=0 }
        if($stable -ge 3) { return }
        Start-Sleep -Milliseconds 100
    } while($watch.Elapsed.TotalSeconds -lt 30)
    throw $Message
}
function Set-Text($Element,[string]$Text) {
    $pattern=$Element.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern)
    $pattern.SetValue($Text)
}
try {
    $root=[System.Windows.Automation.AutomationElement]::RootElement
    $condition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty,$process.Id)
    $timer=[Diagnostics.Stopwatch]::StartNew()
    do { $window=$root.FindFirst([System.Windows.Automation.TreeScope]::Children,$condition); if($window) { break }; Start-Sleep -Milliseconds 100 } while($timer.Elapsed.TotalSeconds -lt 30)
    if(!$window) {
        $process.Refresh()
        Write-Host "Process exited: $($process.HasExited); exit code: $($process.ExitCode); session: $($process.SessionId)"
        if(Test-Path $env:EWAF_DIAGNOSTICS_PATH) { Get-Content $env:EWAF_DIAGNOSTICS_PATH }
        Get-WinEvent -FilterHashtable @{LogName='Application';StartTime=$started} -ErrorAction SilentlyContinue | Where-Object { $_.Message -match 'EWAF' } | Select-Object -ExpandProperty Message
        throw 'EWAF did not expose a native window within 30 seconds.'
    }
    $start=Wait-Element $window 'Start date'
    $end=Wait-Element $window 'End date'
    Set-Text $start '09-01-2026'; Set-Text $end '09-30-2026'
    $create=Wait-Element $window 'CreateFolders' ([System.Windows.Automation.AutomationElement]::AutomationIdProperty)
    $count=Wait-Element $window 'FolderCount' ([System.Windows.Automation.AutomationElement]::AutomationIdProperty)
    $statusElement=Wait-Element $window 'OperationStatus' ([System.Windows.Automation.AutomationElement]::AutomationIdProperty)
    Wait-State { $count.Current.Name -eq '4 folders' -and $create.Current.IsEnabled } 'Valid date range did not produce the expected preview and enable creation.'
    Set-Text $start '02-29-2025'
    Wait-State { !$create.Current.IsEnabled -and $statusElement.Current.Name -like '*Choose a valid date*' } 'Invalid date did not disable creation and explain the error.'
    Wait-Element $window 'Weekday' | Out-Null
    Wait-Element $window 'Find a folder date' | Out-Null
    Wait-Element $window 'ChooseDestination' ([System.Windows.Automation.AutomationElement]::AutomationIdProperty) | Out-Null
    Set-Text $start '01-01-2000'; Set-Text $end '12-31-2010'
    Wait-State { $count.Current.Name -eq '574 folders' -and $create.Current.IsEnabled } 'Large range preview did not finish.'
    $create.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    $continue=Wait-Element $window 'Continue'
    if(!$continue.Current.IsEnabled) { throw 'Large-operation confirmation was not available.' }
    # The dialog cancel is enabled; the background operation cancel is disabled.
    $cancelCondition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::NameProperty,'Cancel')
    $buttons=$window.FindAll([System.Windows.Automation.TreeScope]::Descendants,$cancelCondition)
    $dismissed=$false
    foreach($button in $buttons) { if($button.Current.IsEnabled) { $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke(); $dismissed=$true; break } }
    if(!$dismissed) { throw 'Could not cancel the large-operation confirmation.' }
    Write-Host 'WinUI launch, accessible controls, date validation and large-operation confirmation passed.'
} catch {
    if($window) {
        foreach($field in @($start,$end)) { if($field) { Write-Host ($field.Current.Name+': '+$field.GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value) } }
        $statusCondition=New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::AutomationIdProperty,'OperationStatus')
        $statusElement=$window.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$statusCondition)
        if($statusElement) { Write-Host ('Operation status: '+$statusElement.Current.Name) }
        if($count) { Write-Host ('Folder count: '+$count.Current.Name) }
        if($create) { Write-Host ('Create enabled: '+$create.Current.IsEnabled+'; control: '+$create.Current.ControlType.ProgrammaticName) }
    }
    if(Test-Path $env:EWAF_DIAGNOSTICS_PATH) { Get-Content $env:EWAF_DIAGNOSTICS_PATH }
    throw
} finally {
    if($window) { try { $window.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).Close() } catch {} }
    if(!$process.HasExited) { $process.WaitForExit(5000) | Out-Null }
    if(!$process.HasExited) { Stop-Process -Id $process.Id }
    Remove-Item -Path "HKCU:/Software/tlolabs/EWAF/TestSessions/$env:EWAF_TEST_SESSION" -Recurse -Force -ErrorAction SilentlyContinue
}
