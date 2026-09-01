param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Version = '0.1.0'
)
$ErrorActionPreference = 'Stop'
$build = [IO.Path]::GetFullPath($BuildDirectory)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (!(Test-Path -LiteralPath (Join-Path $build 'EmotionSprite.exe'))) { throw 'Release executable is missing.' }
if (!(Test-Path -LiteralPath (Join-Path $build 'agent-runtime\agent-core.exe'))) { throw 'Bundled Agent Runtime is missing.' }

# A clean CI build contains the executable but not the deployable Qt runtime.
# Make packaging self-contained instead of relying on a prior manual
# windeployqt invocation in the developer build directory.
$needsQtDeploy = !(Test-Path -LiteralPath (Join-Path $build 'D3Dcompiler_47.dll')) -or
    !(Test-Path -LiteralPath (Join-Path $build 'platforms\qwindows.dll'))
if ($needsQtDeploy) {
    $windeployqt = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if (!$windeployqt) { throw 'windeployqt.exe is required to assemble the Windows runtime.' }
    $qmlSource = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\qml'))
    & $windeployqt.Source --release --qmldir $qmlSource --dir $build (Join-Path $build 'EmotionSprite.exe')
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE." }
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
$stage = Join-Path $output ("Stellacandie-$Version-stage")
if (Test-Path -LiteralPath $stage) {
    $resolvedStage=[IO.Path]::GetFullPath($stage)
    if (!$resolvedStage.StartsWith($output,[StringComparison]::OrdinalIgnoreCase) -or $resolvedStage -eq $output) { throw 'Unsafe staging directory refused.' }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
$payload = Join-Path $stage 'payload';New-Item -ItemType Directory -Force -Path $payload | Out-Null
$files = @('EmotionSprite.exe','D3Dcompiler_47.dll','opengl32sw.dll')
$files += Get-ChildItem -LiteralPath $build -Filter 'Qt6*.dll' -File | ForEach-Object Name
foreach ($name in $files | Select-Object -Unique) { Copy-Item -LiteralPath (Join-Path $build $name) -Destination $payload -Force }

# Bundle the application-local MSVC runtime so a target Windows machine does
# not need Visual Studio or a preinstalled VC++ redistributable.  The UCRT is
# part of supported Windows 10/11 installations and is intentionally omitted.
$redistCandidates = @()
if ($env:VCToolsRedistDir) {
    $candidate = Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT'
    if (Test-Path -LiteralPath $candidate) { $redistCandidates += Get-Item -LiteralPath $candidate }
}
$vsRoot = Join-Path ${env:ProgramFiles} 'Microsoft Visual Studio\2022'
if (Test-Path -LiteralPath $vsRoot) {
    $redistCandidates += Get-ChildItem -Path (Join-Path $vsRoot '*\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT') -Directory -ErrorAction SilentlyContinue
}
$redist = $redistCandidates | Sort-Object FullName -Descending | Select-Object -First 1
if (!$redist) { throw 'Microsoft VC143 x64 runtime was not found; refusing to create a non-portable package.' }
foreach ($name in @('concrt140.dll','msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    $source = Join-Path $redist.FullName $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $payload -Force }
}
foreach ($required in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $payload $required))) { throw "Required MSVC runtime is missing: $required" }
}
foreach ($name in @('agent-runtime','EmotionSprite','imageformats','networkinformation','platforms','qml','sqldrivers','styles','tls','translations')) {
    $source=Join-Path $build $name;if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $payload -Recurse -Force }
}
$forbiddenPayload = Get-ChildItem -LiteralPath $payload -Recurse -File | Where-Object {
    $_.Extension -in @('.db','.wal','.env','.log','.dmp') -or $_.Name -match '\.db-(wal|shm)$'
}
if ($forbiddenPayload) { throw "Forbidden data entered release payload: $($forbiddenPayload.FullName -join ', ')" }
$sensitivePatterns = @('DEEPSEEK_API_KEY\s*=','SILICONFLOW_API_KEY\s*=','sk-[A-Za-z0-9_-]{16,}','D:\\codex_qxjl','C:\\Users\\20278')
foreach ($file in Get-ChildItem -LiteralPath $payload -Recurse -File | Where-Object { $_.Length -lt 5MB -and $_.Extension -in @('.txt','.json','.yaml','.yml','.ini','.conf','.ps1','.cmd','.py','.qml') }) {
    $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
    foreach ($pattern in $sensitivePatterns) {
        if ($content -match $pattern) { throw "Sensitive or development-only content found in release payload: $($file.FullName)" }
    }
}
foreach ($name in @('Install.ps1','Uninstall.ps1','Repair-Agent.ps1','Install.cmd','Uninstall.cmd')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "..\release\$name") -Destination $stage -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\LICENSE') -Destination (Join-Path $stage 'LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\THIRD_PARTY_NOTICES.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\docs\user-guide.md') -Destination (Join-Path $stage 'USER_GUIDE.md') -Force
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '..\licenses\LGPL-3.0.txt') -Destination (Join-Path $stage 'licenses\LGPL-3.0.txt') -Force
Compress-Archive -Path (Join-Path $payload 'agent-runtime\*') -DestinationPath (Join-Path $stage 'agent-runtime-backup.zip') -CompressionLevel Optimal
$manifestFiles = Get-ChildItem -LiteralPath $payload -Recurse -File | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($payload.Length+1).Replace('\','/');size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
}
$manifest=[ordered]@{name='Stellacandie';version=$Version;created_at=(Get-Date).ToUniversalTime().ToString('o');files=@($manifestFiles);user_data_included=$false}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'release-manifest.json') -Encoding UTF8
$archive=Join-Path $output "Stellacandie-$Version-Windows-x64.zip";if(Test-Path -LiteralPath $archive){Remove-Item -LiteralPath $archive -Force}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
Write-Output (ConvertTo-Json ([ordered]@{passed=$true;archive=$archive;files=$manifestFiles.Count;bytes=(Get-Item $archive).Length}) -Compress)
