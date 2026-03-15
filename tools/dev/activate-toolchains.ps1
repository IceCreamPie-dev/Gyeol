Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ToolsDir = Join-Path $RepoRoot ".tools"
$EmsdkDir = Join-Path $ToolsDir "emsdk"
$VenvScriptsDir = Join-Path $ToolsDir "venv\Scripts"
$EmsdkEnvBat = Join-Path $EmsdkDir "emsdk_env.bat"
$EmbeddedConfig = Join-Path $EmsdkDir ".emscripten"

function Add-ToPathFront {
    param([Parameter(Mandatory = $true)][string]$PathToAdd)
    if (-not (Test-Path $PathToAdd)) { return }
    $parts = $env:PATH -split ';'
    if ($parts -contains $PathToAdd) { return }
    $env:PATH = "$PathToAdd;$env:PATH"
}

function Import-CmdEnvironment {
    param([Parameter(Mandatory = $true)][string]$BatchFilePath)
    if (-not (Test-Path $BatchFilePath)) {
        throw "Batch file not found: $BatchFilePath"
    }

    $dump = & cmd.exe /c "`"$BatchFilePath`" >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to import environment from: $BatchFilePath"
    }

    foreach ($line in $dump) {
        if ($line -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

if (-not (Test-Path $EmsdkDir)) {
    throw "emsdk not found. Run .\tools\dev\bootstrap-toolchains.ps1 first."
}
if (-not (Test-Path $VenvScriptsDir)) {
    throw "Local venv not found. Run .\tools\dev\bootstrap-toolchains.ps1 first."
}

Import-CmdEnvironment -BatchFilePath $EmsdkEnvBat

if (Test-Path $EmbeddedConfig) {
    $env:EM_CONFIG = $EmbeddedConfig
}

# Make sure local venv tools are preferred.
Add-ToPathFront -PathToAdd $VenvScriptsDir

# Convenience fallback for CMake on local Windows installs.
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Add-ToPathFront -PathToAdd "C:\Program Files\CMake\bin"
}

$missing = [System.Collections.Generic.List[string]]::new()
foreach ($cmd in @("cmake", "scons", "emcmake", "em++")) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        $missing.Add($cmd)
    }
}

if ($missing.Count -gt 0) {
    $message = @"
Toolchain activation is incomplete. Missing command(s): $($missing -join ", ")

Check:
  1) .\tools\dev\bootstrap-toolchains.ps1
  2) If Windows ARM64 prebuilt SDK is unavailable:
     .\tools\dev\bootstrap-toolchains.ps1 -AllowSourceBuild
"@

    $emscriptenDir = Join-Path $EmsdkDir "upstream\emscripten"
    if (-not (Test-Path $emscriptenDir)) {
        $message += "`nDetected issue: Emscripten upstream directory is missing (`"$emscriptenDir`")."
    }

    throw $message
}

Write-Host "Toolchains activated for current shell."
Write-Host "  EMSDK     : $env:EMSDK"
Write-Host "  EM_CONFIG : $env:EM_CONFIG"
Write-Host "  CMake     : $((Get-Command cmake).Source)"
Write-Host "  SCons     : $((Get-Command scons).Source)"
Write-Host "  emcmake   : $((Get-Command emcmake).Source)"
