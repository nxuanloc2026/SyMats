# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Loc Ngo and Symats contributors
[CmdletBinding()]
param([string]$BuildDirectory = 'build', [string]$OutputDirectory = 'out/windows-release')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory, $repoRoot)
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory, $repoRoot)
$executable = @('cli/Release/symats-cli.exe', 'cli/symats-cli.exe') |
    ForEach-Object { Join-Path $buildRoot $_ } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (!$executable) { throw "Missing built CLI in $buildRoot" }

# Reject incorrectly named or wrong-architecture executables before packaging.
$bytes = [IO.File]::ReadAllBytes($executable)
if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
    throw 'CLI is not a Windows PE executable'
}
$pe = [BitConverter]::ToInt32($bytes, 0x3c)
if ($pe -lt 0 -or $pe -gt $bytes.Length - 6 -or
    [BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550 -or
    [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) {
    throw 'CLI is not a Windows x64 executable'
}

# Fresh staging prevents stale files from entering a repeat build.
$stage = Join-Path $outputRoot ('stage-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $stage 'symats-cli.exe')
foreach ($file in @('run-symats.bat', 'LICENSE')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $file) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'windows-package-readme.txt') -Destination (Join-Path $stage 'README.txt')
$revision = git -C $repoRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source revision' }
"Source: https://github.com/nxuanloc2026/SyMats/tree/$revision" |
    Set-Content -LiteralPath (Join-Path $stage 'SOURCE.txt') -Encoding utf8
$archive = Join-Path $outputRoot 'symats-windows-x64.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Write-Output "Created $archive"
