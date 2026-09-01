param(
    [string]$InstallDirectory = "$env:LOCALAPPDATA\Programs\Stellacandie",
    [string]$ShortcutDirectory = ''
)
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'payload'
$running = Get-Process -Name 'EmotionSprite','agent-core' -ErrorAction SilentlyContinue
if ($running) {
    $names = ($running | Select-Object -ExpandProperty ProcessName -Unique) -join ', '
    throw "Stellacandie is currently running ($names). Exit it from the system tray, then run the installer again. No files were changed."
}
$manifest = Get-Content (Join-Path $PSScriptRoot 'release-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($entry in $manifest.files) {
    $candidate = Join-Path $source $entry.path
    if (!(Test-Path -LiteralPath $candidate) -or (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Release verification failed: $($entry.path)"
    }
}
$resolvedParent = [IO.Path]::GetFullPath((Split-Path -Parent $InstallDirectory))
if (!$resolvedParent.StartsWith([IO.Path]::GetFullPath($env:LOCALAPPDATA), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Install directory must be inside the current user LocalAppData.'
}
New-Item -ItemType Directory -Force -Path $InstallDirectory | Out-Null
Copy-Item -Path (Join-Path $source '*') -Destination $InstallDirectory -Recurse -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Uninstall.ps1') -Destination $InstallDirectory -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Repair-Agent.ps1') -Destination $InstallDirectory -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'agent-runtime-backup.zip') -Destination $InstallDirectory -Force
$shortcutPath = $null
$shortcutStatus = 'not_attempted'
$shortcutMessage = ''
try {
    $shell = New-Object -ComObject WScript.Shell
    if ([string]::IsNullOrWhiteSpace($ShortcutDirectory)) {
        $ShortcutDirectory = [Environment]::GetFolderPath('Desktop')
    }
    if ([string]::IsNullOrWhiteSpace($ShortcutDirectory)) {
        throw 'Windows did not provide a desktop directory.'
    }
    $shortcutPath = Join-Path $ShortcutDirectory 'Stellacandie.lnk'
    $mayWriteShortcut = $true
    if (Test-Path -LiteralPath $shortcutPath) {
        $existingShortcut = $shell.CreateShortcut($shortcutPath)
        $existingTarget = [IO.Path]::GetFullPath($existingShortcut.TargetPath)
        $newTarget = [IO.Path]::GetFullPath((Join-Path $InstallDirectory 'EmotionSprite.exe'))
        $mayWriteShortcut = $existingTarget -eq $newTarget
    }
    if ($mayWriteShortcut) {
        $shortcut = $shell.CreateShortcut($shortcutPath)
        $shortcut.TargetPath = Join-Path $InstallDirectory 'EmotionSprite.exe'
        $shortcut.WorkingDirectory = $InstallDirectory
        $shortcut.Save()
        $shortcutStatus = 'created'
    } else {
        $shortcutStatus = 'skipped_existing_conflict'
        $shortcutMessage = 'An unrelated Stellacandie shortcut already exists and was not overwritten.'
        Write-Warning $shortcutMessage
    }
} catch {
    $shortcutStatus = 'warning_failed'
    $shortcutMessage = $_.Exception.Message
    Write-Warning "The application was installed, but the desktop shortcut could not be created: $shortcutMessage"
}
$result = [ordered]@{
    installed = $true
    install_directory = [IO.Path]::GetFullPath($InstallDirectory)
    shortcut_status = $shortcutStatus
    shortcut_path = $shortcutPath
    shortcut_message = $shortcutMessage
    user_data_preserved = $true
    completed_at = (Get-Date).ToUniversalTime().ToString('o')
}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $InstallDirectory 'install-result.json') -Encoding UTF8
Write-Host "Install completed: $InstallDirectory (user data was preserved; shortcut status: $shortcutStatus)."
