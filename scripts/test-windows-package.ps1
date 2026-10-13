# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
[CmdletBinding()]
param([string]$Archive = 'out/windows-release/symats-windows-x64.zip')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$archivePath = [IO.Path]::GetFullPath($Archive, $repoRoot)
$extract = Join-Path (Split-Path $archivePath -Parent) ('test package ' + [guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath $archivePath -DestinationPath $extract
foreach ($file in @('symats-cli.exe', 'run-symats.bat', 'LICENSE', 'README.txt', 'SOURCE.txt')) {
    if (!(Test-Path -LiteralPath (Join-Path $extract $file) -PathType Leaf)) {
        throw "Package is missing $file"
    }
}
$script = Join-Path $extract 'smoke test.sym'
@('2+3', 'a=7', 'a^2', 'D[x^3,x]') | Set-Content -LiteralPath $script -Encoding utf8NoBOM
# Invoke from a different directory and a path containing spaces.
$output = & (Join-Path $extract 'run-symats.bat') $script
if ($LASTEXITCODE -ne 0) { throw "Extracted CLI exited with $LASTEXITCODE" }
$text = $output -join "`n"
foreach ($expected in @('Out[1] = 5', 'Out[2] = 7', 'Out[3] = 49', 'Out[4] = 3*x^2')) {
    if (!$text.Contains($expected)) { throw "Missing result '$expected': $text" }
}
Write-Output 'Extracted Windows CLI and launcher smoke test passed.'
