param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$BuildDir = Join-Path $RepoRoot "build"
$StoryPath = Join-Path $RepoRoot "src/tests/conformance/runtime_contract_v1_story.json"
$ActionsPath = Join-Path $RepoRoot "src/tests/conformance/runtime_contract_v1_actions_cross.json"
$GoldenPath = Join-Path $RepoRoot "src/tests/conformance/runtime_contract_v1_golden_core_cross.json"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [Parameter(Mandatory = $true)][scriptblock]$Script
    )
    Write-Host "==> $Message"
    & $Script
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $Message"
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $defaultCmake = "C:\Program Files\CMake\bin"
    if (Test-Path $defaultCmake) {
        $env:PATH = "$defaultCmake;$env:PATH"
    }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake command not found. Install CMake or run activate-toolchains.ps1 first."
}

function Resolve-ConformanceCli {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Config
    )

    $candidates = @(
        (Join-Path $Root "build/src/tests/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/GyeolRuntimeContractCLI"),
        (Join-Path $Root "build/src/tests/$Config/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/$Config/GyeolRuntimeContractCLI"),
        (Join-Path $Root "build/src/tests/Debug/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/Debug/GyeolRuntimeContractCLI"),
        (Join-Path $Root "build/src/tests/Release/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/Release/GyeolRuntimeContractCLI")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "GyeolRuntimeContractCLI not found. Build target 'GyeolRuntimeContractCLI' first."
}

if (-not $SkipBuild) {
    Invoke-Checked "Configure root build" {
        cmake -S $RepoRoot -B $BuildDir
    }
    Invoke-Checked "Build runtime contract CLI" {
        cmake --build $BuildDir --target GyeolRuntimeContractCLI --config $Configuration
    }
}

$cli = Resolve-ConformanceCli -Root $RepoRoot -Config $Configuration

Invoke-Checked "Regenerate core golden transcript" {
    & $cli generate --engine core --story $StoryPath --actions $ActionsPath --output $GoldenPath
}

Write-Host ""
Write-Host "Golden updated: $GoldenPath"
Write-Host "Review diff and include contract-change rationale in PR."
