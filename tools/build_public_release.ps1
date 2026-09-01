param(
    [string]$BuildDirectory = "$PSScriptRoot\..\build\release",
    [string]$OutputDirectory = "$PSScriptRoot\..\release-output",
    [string]$Version = '0.9.0-rc.1',
    [string]$InnoCompiler = 'D:\InnoSetup6\ISCC.exe'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output | Out-Null

& (Join-Path $PSScriptRoot 'package_release.ps1') -BuildDirectory $BuildDirectory -OutputDirectory $output -Version $Version
if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) { throw 'Portable package generation failed.' }

$stage = Join-Path $output "Stellacandie-$Version-stage"
if (!(Test-Path -LiteralPath $InnoCompiler)) { throw "Inno Setup compiler was not found: $InnoCompiler" }
& $InnoCompiler "/DMyAppVersion=$Version" "/DStageDir=$stage" "/DOutputDir=$output" (Join-Path $root 'release\Stellacandie.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed.' }

$portable = Join-Path $output "Stellacandie-$Version-Windows-x64.zip"
$installer = Join-Path $output "Stellacandie-$Version-Windows-x64-Setup.exe"
foreach ($artifact in @($portable,$installer)) {
    if (!(Test-Path -LiteralPath $artifact)) { throw "Release artifact is missing: $artifact" }
}
$hashLines = foreach ($artifact in @($installer,$portable)) {
    $hash = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash
    "$hash  $([IO.Path]::GetFileName($artifact))"
}
$hashLines | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
[ordered]@{
    passed = $true
    version = $Version
    installer = $installer
    portable = $portable
    sha256_file = (Join-Path $output 'SHA256SUMS.txt')
} | ConvertTo-Json
