param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipActivate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $SkipActivate) {
    . (Join-Path $PSScriptRoot "activate-toolchains.ps1")
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$BuildDir = Join-Path $RepoRoot "build_wasm"
$DistDir = Join-Path $BuildDir "dist"

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

Invoke-Checked "Configure WASM build with emcmake" {
    emcmake cmake -S bindings/wasm -B build_wasm -G Ninja -DCMAKE_BUILD_TYPE=$Configuration
}

Invoke-Checked "Build WASM target" {
    cmake --build build_wasm --parallel
}

$JsFile = Join-Path $DistDir "gyeol.js"
$WasmFile = Get-ChildItem -Path $DistDir -Filter "*.wasm" -File -ErrorAction SilentlyContinue | Select-Object -First 1

if (-not (Test-Path $JsFile)) {
    throw "Missing artifact: $JsFile"
}
if (-not $WasmFile) {
    throw "Missing artifact: *.wasm in $DistDir"
}

Write-Host "WASM build succeeded."
Write-Host "  JS   : $JsFile"
Write-Host "  WASM : $($WasmFile.FullName)"
