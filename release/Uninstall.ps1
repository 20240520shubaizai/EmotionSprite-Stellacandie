param(
    [string]$InstallDirectory = $PSScriptRoot,
    [switch]$DeleteUserData,
    [string]$Confirmation = ''
)
$ErrorActionPreference = 'Stop'
$install = [IO.Path]::GetFullPath($InstallDirectory)
$localPrograms = [IO.Path]::GetFullPath("$env:LOCALAPPDATA\Programs")
if (!$install.StartsWith($localPrograms,[StringComparison]::OrdinalIgnoreCase) -or $install -eq $localPrograms) {
    throw 'Unsafe uninstall directory refused.'
}
if ($DeleteUserData) {
    if ($Confirmation -ne 'DELETE STELLACANDIE DATA') { throw 'Exact deletion confirmation is required.' }
    $agent = Join-Path $install 'agent-runtime\agent-core.exe'
    if (!(Test-Path -LiteralPath $agent)) { throw 'Maintenance runtime is missing; uninstall stopped.' }
    & $agent --delete-data --confirmation $Confirmation --delete-credentials
    if ($LASTEXITCODE -ne 0) { throw 'User data deletion failed; installation was preserved.' }
}
$shortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Stellacandie.lnk'
if (Test-Path -LiteralPath $shortcut) {
    $shell = New-Object -ComObject WScript.Shell
    $shortcutTarget = $shell.CreateShortcut($shortcut).TargetPath
    $expectedTarget = Join-Path $install 'EmotionSprite.exe'
    if ([IO.Path]::GetFullPath($shortcutTarget) -eq [IO.Path]::GetFullPath($expectedTarget)) {
        Remove-Item -LiteralPath $shortcut -Force
    }
}
$parent = Split-Path -Parent $install
$leaf = Split-Path -Leaf $install
$cleanup = "Start-Sleep -Milliseconds 800; Remove-Item -LiteralPath '$install' -Recurse -Force"
Start-Process powershell.exe -WindowStyle Hidden -ArgumentList '-NoProfile','-Command',$cleanup
Write-Host 'Uninstall started. User data is preserved unless deletion was explicitly confirmed.'
