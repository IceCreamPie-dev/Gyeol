param(
    [switch]$SkipActivate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return Resolve-Path (Join-Path $PSScriptRoot "..\..")
}

function Get-ToolPath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    return Join-Path (Get-RepoRoot) $RelativePath
}

function Check-Path {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$PathValue
    )
    if (Test-Path $PathValue) {
        Write-Host "[ OK ] $Label : $PathValue"
        return $true
    }

    Write-Host "[FAIL] $Label : missing ($PathValue)"
    return $false
}

function Check-Command {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$VersionScript
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $cmd) {
        Write-Host "[FAIL] $Name : not found"
        return $false
    }

    $version = & $VersionScript
    Write-Host "[ OK ] $Name : $($cmd.Source)"
    Write-Host "       $version"
    return $true
}

$activationFailed = $false
if (-not $SkipActivate) {
    try {
        . (Join-Path $PSScriptRoot "activate-toolchains.ps1")
    }
    catch {
        $activationFailed = $true
        Write-Host "[FAIL] activation : $($_.Exception.Message)"
        Write-Host "       doctor will continue and report remaining checks."
    }
}

function Get-VersionLine {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string[]]$Args
    )
    $lines = & $Command @Args 2>&1
    if ($LASTEXITCODE -ne 0) {
        return "FAILED"
    }
    if (-not $lines) {
        return "OK"
    }
    if ($lines -is [string]) {
        return $lines
    }
    return [string]$lines[0]
}

$checks = @(
    @{ Name = "cmake"; Version = { Get-VersionLine -Command "cmake" -Args @("--version") } },
    @{ Name = "python"; Version = { Get-VersionLine -Command "python" -Args @("--version") } },
    @{ Name = "scons"; Version = { Get-VersionLine -Command "scons" -Args @("--version") } },
    @{ Name = "em++"; Version = { Get-VersionLine -Command "em++" -Args @("--version") } },
    @{ Name = "emcmake"; Version = { "available" } }
)

$failed = $activationFailed
foreach ($check in $checks) {
    if (-not (Check-Command -Name $check.Name -VersionScript $check.Version)) {
        $failed = $true
    }    
}

$pathChecks = @(
    @{ Label = ".tools/emsdk"; Path = (Get-ToolPath ".tools\emsdk") },
    @{ Label = ".tools/venv"; Path = (Get-ToolPath ".tools\venv") },
    @{ Label = "emsdk_env.bat"; Path = (Get-ToolPath ".tools\emsdk\emsdk_env.bat") },
    @{ Label = "emscripten/emcmake.bat"; Path = (Get-ToolPath ".tools\emsdk\upstream\emscripten\emcmake.bat") },
    @{ Label = "venv/scons.exe"; Path = (Get-ToolPath ".tools\venv\Scripts\scons.exe") }
)

foreach ($entry in $pathChecks) {
    if (-not (Check-Path -Label $entry.Label -PathValue $entry.Path)) {
        $failed = $true
    }
}

if ($failed) {
    Write-Host ""
    Write-Host "Suggested recovery:"
    Write-Host "  .\tools\dev\bootstrap-toolchains.ps1"
    Write-Host "  .\tools\dev\bootstrap-toolchains.ps1 -AllowSourceBuild    # if ARM64 prebuilt is unavailable"
    Write-Host "  .\tools\dev\activate-toolchains.ps1"
    Write-Host ""
    throw "Toolchain doctor failed."
}

Write-Host "Toolchain doctor: all checks passed."
