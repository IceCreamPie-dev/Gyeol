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

function Test-CommandUnderPath {
    param(
        [Parameter(Mandatory = $true)][string]$CommandName,
        [Parameter(Mandatory = $true)][string]$ExpectedRoot
    )

    $cmd = Get-Command $CommandName -ErrorAction SilentlyContinue
    if (-not $cmd) {
        return $false
    }

    if (-not (Test-Path $ExpectedRoot)) {
        return $false
    }

    $resolvedExpected = (Resolve-Path $ExpectedRoot).Path
    $resolvedActual = (Resolve-Path $cmd.Source).Path
    return $resolvedActual.StartsWith($resolvedExpected, [System.StringComparison]::OrdinalIgnoreCase)
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
} elseif (-not $env:EM_CONFIG) {
    throw "Embedded Emscripten config not found: $EmbeddedConfig. Re-run bootstrap."
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

if (-not (Test-Path $env:EM_CONFIG)) {
    throw @"
Toolchain activation is incomplete. EM_CONFIG points to a missing file:
  EM_CONFIG=$env:EM_CONFIG
Expected:
  $EmbeddedConfig

Recovery:
  .\tools\dev\bootstrap-toolchains.ps1
  .\tools\dev\activate-toolchains.ps1
"@
}

if ($missing.Count -gt 0) {
    $message = @"
Toolchain activation is incomplete. Missing command(s): $($missing -join ", ")

Recovery:
  .\tools\dev\bootstrap-toolchains.ps1
  .\tools\dev\activate-toolchains.ps1
"@

    $emscriptenDir = Join-Path $EmsdkDir "upstream\emscripten"
    if (-not (Test-Path $emscriptenDir)) {
        $message += "`nDetected issue: Emscripten upstream directory is missing (`"$emscriptenDir`")."
    }

    throw $message
}

$pathMismatches = [System.Collections.Generic.List[string]]::new()
if (-not (Test-CommandUnderPath -CommandName "scons" -ExpectedRoot $VenvScriptsDir)) {
    $pathMismatches.Add("scons is not resolved from local venv ($VenvScriptsDir).")
}

$ExpectedEmscriptenRoot = Join-Path $EmsdkDir "upstream\emscripten"
foreach ($cmd in @("emcmake", "em++")) {
    if (-not (Test-CommandUnderPath -CommandName $cmd -ExpectedRoot $ExpectedEmscriptenRoot)) {
        $pathMismatches.Add("$cmd is not resolved from local emsdk ($ExpectedEmscriptenRoot).")
    }
}

if ($pathMismatches.Count -gt 0) {
    throw @"
Toolchain activation resolved commands from unexpected locations:
$($pathMismatches -join "`n")

Recovery:
  .\tools\dev\bootstrap-toolchains.ps1
  .\tools\dev\activate-toolchains.ps1
"@
}

Write-Host "Toolchains activated for current shell."
Write-Host "  EMSDK     : $env:EMSDK"
Write-Host "  EM_CONFIG : $env:EM_CONFIG"
Write-Host "  CMake     : $((Get-Command cmake).Source)"
Write-Host "  SCons     : $((Get-Command scons).Source)"
Write-Host "  emcmake   : $((Get-Command emcmake).Source)"
