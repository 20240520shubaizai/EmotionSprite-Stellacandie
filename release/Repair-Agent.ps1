param([string]$InstallDirectory = $PSScriptRoot)
$ErrorActionPreference = 'Stop'
$install = [IO.Path]::GetFullPath($InstallDirectory)
$runtime = Join-Path $install 'agent-runtime'
$backup = Join-Path $install 'agent-runtime-backup.zip'
if (!(Test-Path -LiteralPath $backup)) { throw 'Agent rollback package is missing.' }
$programs = [IO.Path]::GetFullPath("$env:LOCALAPPDATA\Programs")
if (!$install.StartsWith($programs,[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe repair directory refused.' }
if (Test-Path -LiteralPath $runtime) {
    $quarantine = Join-Path $install ("agent-runtime.failed-" + (Get-Date -Format 'yyyyMMddHHmmss'))
    Move-Item -LiteralPath $runtime -Destination $quarantine
}
Expand-Archive -LiteralPath $backup -DestinationPath $runtime -Force
$agent = Join-Path $runtime 'agent-core.exe'
if (!(Test-Path -LiteralPath $agent)) { throw 'Agent repair package is incomplete.' }
& $agent --help | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Agent self-test failed after repair.' }
Write-Host 'Agent Runtime restored. The quarantined failed copy remains for manual review.'
