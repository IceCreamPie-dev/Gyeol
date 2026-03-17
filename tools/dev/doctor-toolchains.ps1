param(
    [switch]$SkipActivate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return Resolve-Path (Join-Path $PSScriptRoot "..\..")
}

function Add-Issue {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Buckets,
        [Parameter(Mandatory = $true)][string]$Bucket,
        [Parameter(Mandatory = $true)][string]$Message
    )
    $Buckets[$Bucket].Add($Message) | Out-Null
}

function Get-VersionLine {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string[]]$Args
    )
    $lines = & $Command @Args 2>&1
    if ($LASTEXITCODE -ne 0) {
        return $null
    }
    if (-not $lines) {
        return "OK"
    }
    if ($lines -is [string]) {
        return $lines
    }
    return [string]$lines[0]
}

function Check-Path {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$PathValue,
        [Parameter(Mandatory = $true)][hashtable]$Issues,
        [Parameter(Mandatory = $true)][string]$Bucket
    )
    if (Test-Path $PathValue) {
        Write-Host "[ OK ] $Label : $PathValue"
        return
    }

    Write-Host "[FAIL] $Label : missing ($PathValue)"
    Add-Issue -Buckets $Issues -Bucket $Bucket -Message "$Label missing: $PathValue"
}

function Check-CommandRuntime {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$VersionArgs,
        [Parameter(Mandatory = $true)][hashtable]$Issues,
        [Parameter(Mandatory = $true)][string]$Bucket,
        [string]$ExpectedRoot = ""
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        Write-Host "[FAIL] $Name : not found"
        Add-Issue -Buckets $Issues -Bucket $Bucket -Message "$Name command not found"
        return
    }

    $version = Get-VersionLine -Command $Name -Args $VersionArgs
    if (-not $version) {
        Write-Host "[FAIL] $Name : found but failed to execute"
        Add-Issue -Buckets $Issues -Bucket $Bucket -Message "$Name command failed to execute"
        return
    }

    Write-Host "[ OK ] $Name : $($cmd.Source)"
    Write-Host "       $version"

    if (-not [string]::IsNullOrWhiteSpace($ExpectedRoot)) {
        if (-not (Test-Path $ExpectedRoot)) {
            Add-Issue -Buckets $Issues -Bucket $Bucket -Message "$Name expected root missing: $ExpectedRoot"
            return
        }
        $resolvedExpected = (Resolve-Path $ExpectedRoot).Path
        $resolvedActual = (Resolve-Path $cmd.Source).Path
        if (-not $resolvedActual.StartsWith($resolvedExpected, [System.StringComparison]::OrdinalIgnoreCase)) {
            $message = "$Name resolved from unexpected path: $resolvedActual (expected under $resolvedExpected)"
            Write-Host "[FAIL] $Name : path mismatch"
            Add-Issue -Buckets $Issues -Bucket $Bucket -Message $message
        }
    }
}

$RepoRoot = Get-RepoRoot
$VersionScript = Join-Path $PSScriptRoot "toolchain-versions.ps1"
if (Test-Path $VersionScript) {
    . $VersionScript
    $versions = Get-GyeolToolchainVersions
    Write-Host "Expected toolchain versions:"
    Write-Host "  Emscripten : $($versions.EMSCRIPTEN_VERSION)"
    Write-Host "  SCons      : $($versions.SCONS_VERSION)"
    Write-Host "  Ninja      : $($versions.NINJA_VERSION)"
}

$issues = @{
    emsdk = [System.Collections.Generic.List[string]]::new()
    venv = [System.Collections.Generic.List[string]]::new()
    msvc = [System.Collections.Generic.List[string]]::new()
    ninja = [System.Collections.Generic.List[string]]::new()
    other = [System.Collections.Generic.List[string]]::new()
}

if (-not $SkipActivate) {
    try {
        . (Join-Path $PSScriptRoot "activate-toolchains.ps1")
    }
    catch {
        Write-Host "[FAIL] activation : $($_.Exception.Message)"
        Add-Issue -Buckets $issues -Bucket "emsdk" -Message "activate-toolchains failed"
        Add-Issue -Buckets $issues -Bucket "venv" -Message "activate-toolchains failed"
    }
}

$toolsRoot = Join-Path $RepoRoot ".tools"
$emsdkRoot = Join-Path $toolsRoot "emsdk"
$venvRoot = Join-Path $toolsRoot "venv"
$venvScripts = Join-Path $venvRoot "Scripts"
$emscriptenRoot = Join-Path $emsdkRoot "upstream\emscripten"

Check-Path -Label ".tools/emsdk" -PathValue $emsdkRoot -Issues $issues -Bucket "emsdk"
Check-Path -Label ".tools/venv" -PathValue $venvRoot -Issues $issues -Bucket "venv"
Check-Path -Label "emsdk_env.bat" -PathValue (Join-Path $emsdkRoot "emsdk_env.bat") -Issues $issues -Bucket "emsdk"
Check-Path -Label "emscripten/emcmake.bat" -PathValue (Join-Path $emscriptenRoot "emcmake.bat") -Issues $issues -Bucket "emsdk"
Check-Path -Label "venv/scons.exe" -PathValue (Join-Path $venvScripts "scons.exe") -Issues $issues -Bucket "venv"

Check-CommandRuntime -Name "cmake" -VersionArgs @("--version") -Issues $issues -Bucket "other"
Check-CommandRuntime -Name "python" -VersionArgs @("--version") -Issues $issues -Bucket "other"
Check-CommandRuntime -Name "ninja" -VersionArgs @("--version") -Issues $issues -Bucket "ninja"
Check-CommandRuntime -Name "scons" -VersionArgs @("--version") -Issues $issues -Bucket "venv" -ExpectedRoot $venvScripts
Check-CommandRuntime -Name "em++" -VersionArgs @("--version") -Issues $issues -Bucket "emsdk" -ExpectedRoot $emscriptenRoot
Check-CommandRuntime -Name "emcmake" -VersionArgs @("--version") -Issues $issues -Bucket "emsdk" -ExpectedRoot $emscriptenRoot

if ($env:OS -eq "Windows_NT") {
    Check-CommandRuntime -Name "cl" -VersionArgs @("/?") -Issues $issues -Bucket "msvc"
}

$failed = $false
foreach ($bucket in @("emsdk", "venv", "msvc", "ninja", "other")) {
    if ($issues[$bucket].Count -gt 0) {
        $failed = $true
    }
}

if ($failed) {
    Write-Host ""
    Write-Host "Toolchain doctor found issues:"
    foreach ($bucket in @("emsdk", "venv", "msvc", "ninja", "other")) {
        if ($issues[$bucket].Count -eq 0) {
            continue
        }
        Write-Host ""
        Write-Host "[$bucket]"
        foreach ($msg in $issues[$bucket]) {
            Write-Host "  - $msg"
        }
    }

    Write-Host ""
    Write-Host "Suggested recovery:"
    Write-Host "  .\tools\dev\bootstrap-toolchains.ps1"
    Write-Host "  .\tools\dev\activate-toolchains.ps1"
    Write-Host "  .\tools\dev\doctor-toolchains.ps1 -SkipActivate"
    Write-Host ""
    Write-Host "If source-build fallback fails on ARM64, verify:"
    Write-Host "  - Visual Studio Build Tools (Desktop C++)"
    Write-Host "  - Ninja is available in PATH"
    throw "Toolchain doctor failed."
}

Write-Host "Toolchain doctor: all checks passed."
