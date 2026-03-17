param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    [switch]$SkipActivate,
    [switch]$SkipDoctor
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $SkipActivate) {
    $activateScript = Join-Path $PSScriptRoot "activate-toolchains.ps1"
    if (Test-Path $activateScript) {
        . $activateScript
    }
}

$DoctorScript = Join-Path $PSScriptRoot "doctor-toolchains.ps1"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ConformanceDir = Join-Path $RepoRoot "logs/conformance"
$StoryPath = Join-Path $RepoRoot "src/tests/conformance/runtime_contract_v1_story.gyeol"
$ActionsPath = Join-Path $RepoRoot "src/tests/conformance/runtime_contract_v1_actions_cross.json"
$GoldenPath = Join-Path $RepoRoot "src/tests/conformance/runtime_contract_v1_golden_core_cross.json"
$WasmModulePath = Join-Path $RepoRoot "build_wasm/dist/gyeol.js"
$WasmScriptPath = Join-Path $RepoRoot "bindings/wasm/tests/runtime_contract_conformance.js"

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

if (-not $SkipDoctor) {
    if (-not (Test-Path $DoctorScript)) {
        throw "doctor script not found: $DoctorScript"
    }
    Invoke-Checked "Validate local toolchains (doctor)" {
        & $DoctorScript -SkipActivate
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
        cmake -S $RepoRoot -B (Join-Path $RepoRoot "build")
    }
    Invoke-Checked "Build runtime contract CLI" {
        cmake --build (Join-Path $RepoRoot "build") --target GyeolRuntimeContractCLI --config $Configuration
    }

    if (-not (Get-Command emcmake -ErrorAction SilentlyContinue)) {
        throw "emcmake command not found. Run .\\tools\\dev\\activate-toolchains.ps1 first."
    }

    Invoke-Checked "Configure WASM build" {
        emcmake cmake -S (Join-Path $RepoRoot "bindings/wasm") -B (Join-Path $RepoRoot "build_wasm") -G Ninja -DCMAKE_BUILD_TYPE=$Configuration
    }
    Invoke-Checked "Build WASM target" {
        cmake --build (Join-Path $RepoRoot "build_wasm") --parallel
    }
}

$cli = Resolve-ConformanceCli -Root $RepoRoot -Config $Configuration

New-Item -ItemType Directory -Path $ConformanceDir -Force | Out-Null

$coreActual = Join-Path $ConformanceDir "core.actual.json"
$coreExpected = Join-Path $ConformanceDir "core.expected.json"
$coreDiff = Join-Path $ConformanceDir "core.diff.json"
$godotActual = Join-Path $ConformanceDir "godot_adapter.actual.json"
$godotExpected = Join-Path $ConformanceDir "godot_adapter.expected.json"
$godotDiff = Join-Path $ConformanceDir "godot_adapter.diff.json"
$wasmActual = Join-Path $ConformanceDir "wasm.actual.json"
$wasmExpected = Join-Path $ConformanceDir "wasm.expected.json"
$wasmDiff = Join-Path $ConformanceDir "wasm.diff.json"

Invoke-Checked "Generate Core transcript" {
    & $cli generate --engine core --story $StoryPath --actions $ActionsPath --output $coreActual
}
Invoke-Checked "Compare Core transcript to golden" {
    & $cli compare --expected $GoldenPath --actual $coreActual --expected-engine core --expected-out $coreExpected --actual-out $coreActual --diff-out $coreDiff
}

Invoke-Checked "Generate Godot adapter transcript" {
    & $cli generate --engine godot_adapter --story $StoryPath --actions $ActionsPath --output $godotActual
}
Invoke-Checked "Compare Godot adapter transcript to golden" {
    & $cli compare --expected $GoldenPath --actual $godotActual --expected-engine godot_adapter --expected-out $godotExpected --actual-out $godotActual --diff-out $godotDiff
}

if (-not (Test-Path $WasmModulePath)) {
    throw "WASM module missing: $WasmModulePath"
}

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw "node command not found. Install Node.js or activate local toolchains."
}

Invoke-Checked "Compare WASM transcript to golden" {
    & node $WasmScriptPath $WasmModulePath $StoryPath $ActionsPath $GoldenPath --expected-engine wasm --expected-out $wasmExpected --actual-out $wasmActual --diff-out $wasmDiff
}

Write-Host ""
Write-Host "Runtime contract conformance passed."
Write-Host "Artifacts: $ConformanceDir"
