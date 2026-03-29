param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$BuildDir = Join-Path $RepoRoot "build"
$ConformanceDir = Join-Path $RepoRoot "logs/conformance"
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

function Resolve-ConformanceCli {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Config
    )

    $candidates = @(
        (Join-Path $Root "build/src/tests/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/$Config/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/Debug/GyeolRuntimeContractCLI.exe"),
        (Join-Path $Root "build/src/tests/Release/GyeolRuntimeContractCLI.exe")
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

New-Item -ItemType Directory -Path $ConformanceDir -Force | Out-Null

$coreActual = Join-Path $ConformanceDir "core.actual.json"
$coreExpected = Join-Path $ConformanceDir "core.expected.json"
$coreDiff = Join-Path $ConformanceDir "core.diff.json"

Invoke-Checked "Generate Core transcript (JSON IR input)" {
    & $cli generate --engine core --story $StoryPath --actions $ActionsPath --output $coreActual
}
Invoke-Checked "Compare Core transcript to golden" {
    & $cli compare --expected $GoldenPath --actual $coreActual --expected-engine core --expected-out $coreExpected --actual-out $coreActual --diff-out $coreDiff
}

Write-Host ""
Write-Host "Runtime contract conformance passed (core/windows scope)."
Write-Host "Artifacts: $ConformanceDir"
