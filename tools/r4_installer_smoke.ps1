param(
    [string]$StageDirectory = "$PSScriptRoot\..\release-output\Stellacandie-0.9.0-rc.1-stage",
    [string]$InnoCompiler = 'D:\InnoSetup6\ISCC.exe'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testRoot = Join-Path $root '.tmp\r4-installer-smoke'
$outputOld = Join-Path $testRoot 'old'
$outputNew = Join-Path $testRoot 'new'
$install = Join-Path $testRoot 'install'
$profile = Join-Path $testRoot 'profile'
$appId = '{{D4F2ABAC-1B0F-49DC-AF5B-93061BA56410}'
foreach ($directory in @($outputOld,$outputNew,$install,$profile)) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }

function Compile-SmokeInstaller([string]$version,[string]$output) {
    $installer=Join-Path $output "Stellacandie-$version-Windows-x64-Setup.exe"
    if (Test-Path -LiteralPath $installer) { return $installer }
    & $InnoCompiler "/DMyAppVersion=$version" "/DMyAppId=$appId" "/DStageDir=$([IO.Path]::GetFullPath($StageDirectory))" "/DOutputDir=$output" '/DCompressionMode=lzma2/fast' '/DSolidMode=no' (Join-Path $root 'release\Stellacandie.iss') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Smoke installer compilation failed for $version" }
    $installer
}

$oldInstaller = Compile-SmokeInstaller '0.8.0' $outputOld
$newInstaller = Compile-SmokeInstaller '0.9.0-rc.1' $outputNew
$oldInstallProcess=Start-Process -FilePath $oldInstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/DIR=$install",'/MERGETASKS=!desktopicon',"/LOG=$(Join-Path $testRoot 'install-old.log')") -Wait -PassThru
if ($oldInstallProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath (Join-Path $install 'EmotionSprite.exe'))) { throw "R4 fresh installation failed with exit code $($oldInstallProcess.ExitCode)." }
$oldManifest = Get-Content (Join-Path $install 'release-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json

$sentinel = Join-Path $profile 'upgrade-preservation.txt'
'preserve-me' | Set-Content -LiteralPath $sentinel -Encoding ascii
$upgradeProcess=Start-Process -FilePath $newInstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/DIR=$install",'/MERGETASKS=!desktopicon',"/LOG=$(Join-Path $testRoot 'install-upgrade.log')") -Wait -PassThru
if ($upgradeProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath $sentinel)) { throw "R4 upgrade or data preservation check failed with exit code $($upgradeProcess.ExitCode)." }

$previousAppData=$env:APPDATA;$previousLocalAppData=$env:LOCALAPPDATA
$env:APPDATA=Join-Path $profile 'Roaming';$env:LOCALAPPDATA=Join-Path $profile 'Local'
New-Item -ItemType Directory -Force -Path $env:APPDATA,$env:LOCALAPPDATA | Out-Null
$process=Start-Process -FilePath (Join-Path $install 'EmotionSprite.exe') -PassThru
Start-Sleep -Seconds 15
if ($process.HasExited) { throw "R4 installed application exited early: $($process.ExitCode)" }
$agent=Get-Process agent-core -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$install*" } | Select-Object -First 1
if (!$agent) { throw 'R4 bundled Agent Runtime did not start.' }
if (Test-Path -LiteralPath (Join-Path $install 'agent_core_sync_dev.db')) { throw 'Development sync database was created in the install directory.' }
Stop-Process -Id $process.Id -Force
if ($agent -and !$agent.HasExited) { Stop-Process -Id $agent.Id -Force }
$env:APPDATA=$previousAppData;$env:LOCALAPPDATA=$previousLocalAppData

$uninstaller=Get-ChildItem -LiteralPath $install -Filter 'unins*.exe' -File | Select-Object -First 1
if (!$uninstaller) { throw 'R4 standard uninstaller was not registered.' }
$uninstallProcess=Start-Process -FilePath $uninstaller.FullName -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=$(Join-Path $testRoot 'uninstall.log')") -Wait -PassThru
if ($uninstallProcess.ExitCode -ne 0) { throw "R4 uninstall failed with exit code $($uninstallProcess.ExitCode)." }
Start-Sleep -Seconds 3
if (Test-Path -LiteralPath (Join-Path $install 'EmotionSprite.exe')) { throw 'R4 uninstall did not remove the application.' }
if (!(Test-Path -LiteralPath $sentinel)) { throw 'R4 uninstall removed preserved user data.' }

[ordered]@{
    passed=$true
    fresh_install=$true
    upgrade=$true
    bundled_agent_started=$true
    install_directory_clean=$true
    uninstall=$true
    user_data_preserved=$true
    evidence_directory=$testRoot
    manifest_version=$oldManifest.version
} | ConvertTo-Json
