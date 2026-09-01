$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location $root
try {
    $tracked = @(git ls-files)
    $forbidden = $tracked | Where-Object {
        $_ -match '(^|/)(build|dist|release-output|\.venv|\.tmp)/' -or
        $_ -match '\.(db|db-wal|db-shm|env|dmp|zip|exe|pdb)$'
    }
    if ($forbidden) { throw "Generated or private files are tracked: $($forbidden -join ', ')" }

    $textExtensions = @('.cpp','.h','.py','.qml','.ps1','.cmd','.json','.yaml','.yml','.md','.txt','.iss','.cmake')
    $patterns = @(
        'sk-[A-Za-z0-9_-]{20,}',
        'gh[opsu]_[A-Za-z0-9]{20,}',
        'DEEPSEEK_API_KEY\s*=\s*[^\s"'']+',
        'SILICONFLOW_API_KEY\s*=\s*[^\s"'']+',
        'C:\\Users\\20278',
        'D:\\codex_qxjl'
    )
    foreach ($path in $tracked) {
        # The audit definition necessarily contains the forbidden expressions.
        if ($path -eq 'tools/release_source_audit.ps1') { continue }
        if (!(Test-Path -LiteralPath $path) -or [IO.Path]::GetExtension($path) -notin $textExtensions) { continue }
        $content=Get-Content -LiteralPath $path -Raw -ErrorAction SilentlyContinue
        foreach ($pattern in $patterns) {
            if ($content -match $pattern) { throw "Potential secret or developer path in tracked file: $path (pattern: $pattern)" }
        }
    }
    Write-Output "Release source audit passed for $($tracked.Count) tracked files."
} finally { Pop-Location }
