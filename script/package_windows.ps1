param([ValidateSet('x64','ARM64')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$version = python script/version.py
$target = if ($Architecture -eq 'ARM64') { 'aarch64-pc-windows-msvc' } else { 'x86_64-pc-windows-msvc' }
$runtime = if ($Architecture -eq 'ARM64') { 'win-arm64' } else { 'win-x64' }
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build command failed: $LASTEXITCODE" } }
rustup target add $target; Check-Exit
cargo build --release --locked -p ewaf-ffi -p ewaf-update --target $target; Check-Exit
$stage="dist/windows-$Architecture"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$env:AVALONIA_TELEMETRY_OPTOUT="1"
$env:DOTNET_CLI_TELEMETRY_OPTOUT="1"
dotnet publish platform/avalonia/EWAF/EWAF.csproj -c Release -r $runtime --self-contained true -p:Platform=$Architecture -p:RestoreLockedMode=true -o $stage; Check-Exit

Copy-Item "target/$target/release/ewaf_ffi.dll" "$stage/ewaf_ffi.dll"
Copy-Item "target/$target/release/ewaf-update.exe" "$stage/ewaf-update.exe"
dotnet publish platform/windows/EWAF.Installer/EWAF.Installer.csproj -c Release -r $runtime --self-contained true -o "$stage/updater-installer"; Check-Exit
# A single executable can be copied away from the installation before the MSI replaces it.
if (!(Test-Path "$stage/updater-installer/ewaf-installer.exe")) { throw 'Native installer helper is missing.' }
Copy-Item "platform/windows/packaging/Install.ps1" "$stage/Install.ps1"
Copy-Item "platform/windows/packaging/Uninstall.ps1" "$stage/Uninstall.ps1"
Copy-Item THIRD_PARTY_NOTICES.md "$stage/THIRD_PARTY_NOTICES.md"
Copy-Item LICENSE "$stage/LICENSE"
Copy-Item docs/DEPENDENCIES.md "$stage/DEPENDENCIES.md"
if ($env:WINDOWS_CERTIFICATE_PATH) {
    if (!$env:WINDOWS_CERTIFICATE_PASSWORD) { throw 'Signing certificate password is required.' }
    $signtool=(Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" | Sort-Object FullName -Descending | Select-Object -First 1).FullName
    if (!$signtool) { throw "Windows SDK signtool was not found." }
    & $signtool sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /f $env:WINDOWS_CERTIFICATE_PATH /p $env:WINDOWS_CERTIFICATE_PASSWORD "$stage/EWAF.exe" "$stage/ewaf_ffi.dll"; Check-Exit
    & $signtool verify /pa "$stage/EWAF.exe"; Check-Exit
}
python script/collect_dotnet_notices.py $stage; Check-Exit
# Record package-owned files so uninstall preserves any user-created folders.
$stageRoot=(Resolve-Path $stage).Path
$owned=@(Get-ChildItem $stageRoot -File -Recurse | Where-Object { $_.Name -ne 'install-manifest.json' } | ForEach-Object { $_.FullName.Substring($stageRoot.Length+1) })
ConvertTo-Json -InputObject $owned | Set-Content (Join-Path $stageRoot 'install-manifest.json') -Encoding utf8
New-Item -ItemType Directory -Force dist | Out-Null
$archive="dist/EWAF-$version-windows-$Architecture.zip"
Compress-Archive -Path "$stage/*" -DestinationPath $archive -Force
python script/validate_package.py $archive; Check-Exit
