param(
    [switch]$AllowSourceBuild,
    [switch]$DisableSourceBuildFallback
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$VersionScript = Join-Path $PSScriptRoot "toolchain-versions.ps1"
if (-not (Test-Path $VersionScript)) {
    throw "Version script not found: $VersionScript"
}
. $VersionScript
$ToolchainVersions = Get-GyeolToolchainVersions

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ToolsDir = Join-Path $RepoRoot ".tools"
$EmsdkDir = Join-Path $ToolsDir "emsdk"
$VenvDir = Join-Path $ToolsDir "venv"

$EmscriptenVersion = $ToolchainVersions.EMSCRIPTEN_VERSION
$SConsVersion = $ToolchainVersions.SCONS_VERSION
$NinjaVersion = $ToolchainVersions.NINJA_VERSION
$EnableSourceBuildFallback = -not $DisableSourceBuildFallback
$IsArm64Host = ($env:PROCESSOR_ARCHITECTURE -eq "ARM64" -or $env:PROCESSOR_ARCHITEW6432 -eq "ARM64")

if ($AllowSourceBuild) {
    Write-Host "Note: -AllowSourceBuild is now implied (automatic fallback is enabled by default)."
}

Write-Host "Toolchain versions:"
Write-Host "  Emscripten : $EmscriptenVersion"
Write-Host "  SCons      : $SConsVersion"
Write-Host "  Ninja      : $NinjaVersion"
if ($IsArm64Host) {
    Write-Host "Host architecture: ARM64 (source-build fallback may take 30-90+ minutes)."
}

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

function Ensure-Command {
    param([Parameter(Mandatory = $true)][string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $Name"
    }
}

function Add-ToPathFront {
    param([Parameter(Mandatory = $true)][string]$PathToAdd)
    if (-not (Test-Path $PathToAdd)) { return }
    $parts = $env:PATH -split ';'
    if ($parts -contains $PathToAdd) { return }
    $env:PATH = "$PathToAdd;$env:PATH"
}

function Import-CmdEnvironment {
    param([Parameter(Mandatory = $true)][string]$CommandLine)
    $dump = & cmd.exe /c "$CommandLine >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to import environment from command: $CommandLine"
    }

    foreach ($line in $dump) {
        if ($line -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

function Ensure-MsvcForNinjaSourceBuild {
    if (Get-Command cl -ErrorAction SilentlyContinue) {
        return
    }

    $vsDevCmdCandidates = [System.Collections.Generic.List[string]]::new()
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPaths = & $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -eq 0) {
            foreach ($installPath in $installPaths) {
                if ([string]::IsNullOrWhiteSpace($installPath)) { continue }
                $candidate = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
                if (Test-Path $candidate) {
                    $vsDevCmdCandidates.Add($candidate)
                }
            }
        }
    }

    foreach ($root in @("C:\Program Files\Microsoft Visual Studio", "C:\Program Files (x86)\Microsoft Visual Studio")) {
        if (-not (Test-Path $root)) { continue }
        $found = Get-ChildItem $root -Recurse -Filter "VsDevCmd.bat" -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName
        foreach ($item in $found) {
            $vsDevCmdCandidates.Add($item)
        }
    }

    $vsDevCmdCandidates = $vsDevCmdCandidates | Select-Object -Unique
    if (-not $vsDevCmdCandidates -or $vsDevCmdCandidates.Count -eq 0) {
        throw "Visual Studio developer command script (VsDevCmd.bat) was not found."
    }

    $argVariants = @(
        "-arch=arm64 -host_arch=arm64",
        "-arch=x64 -host_arch=arm64",
        "-arch=x64 -host_arch=x64",
        "-arch=amd64 -host_arch=amd64"
    )

    foreach ($vsDevCmdPath in $vsDevCmdCandidates) {
        foreach ($args in $argVariants) {
            try {
                Import-CmdEnvironment -CommandLine "`"$vsDevCmdPath`" $args"
                if (Get-Command cl -ErrorAction SilentlyContinue) {
                    return
                }
            }
            catch {
                # Try next Visual Studio environment combination.
            }
        }
    }

    throw "MSVC compiler (cl.exe) is not available after importing Visual Studio build environment."
}

function Ensure-NinjaForSourceBuild {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        return
    }

    $candidateNinjaDirs = @(
        "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "C:\Program Files\Microsoft Visual Studio\17\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "C:\Program Files\Microsoft Visual Studio\17\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        "C:\Program Files\Microsoft Visual Studio\17\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
    )

    foreach ($dir in $candidateNinjaDirs) {
        if (Test-Path (Join-Path $dir "ninja.exe")) {
            Add-ToPathFront -PathToAdd $dir
            break
        }
    }

    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw @"
Ninja is required for emsdk source-build fallback on Windows.
Install Ninja or ensure one of the Visual Studio Ninja paths exists, then retry:
  .\tools\dev\bootstrap-toolchains.ps1
"@
    }
}

function Clear-EmsdkSourceBuildCache {
    param([Parameter(Mandatory = $true)][string]$EmsdkRoot)
    $llvmGitDir = Join-Path $EmsdkRoot "llvm\git"
    if (-not (Test-Path $llvmGitDir)) { return }

    Get-ChildItem -Path $llvmGitDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "build_*" } |
        ForEach-Object {
            Write-Host "Removing stale emsdk build directory: $($_.FullName)"
            Remove-Item -Path $_.FullName -Recurse -Force -ErrorAction Stop
        }
}

function Resolve-PythonCommand {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        return @{Exe = "py"; Args = @("-3")}
    }
    if (Get-Command python -ErrorAction SilentlyContinue) {
        return @{Exe = "python"; Args = @()}
    }
    throw "Python is required but was not found (py/python)."
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $DefaultCmake = "C:\Program Files\CMake\bin"
    if (Test-Path $DefaultCmake) {
        $env:PATH = "$DefaultCmake;$env:PATH"
    }
}

Ensure-Command git
Ensure-Command cmake

if (-not (Test-Path $ToolsDir)) {
    New-Item -ItemType Directory -Path $ToolsDir | Out-Null
}

if (-not (Test-Path $EmsdkDir)) {
    Invoke-Checked "Clone emsdk" {
        git clone https://github.com/emscripten-core/emsdk.git "$EmsdkDir"
    }
}

Push-Location $EmsdkDir
try {
    $EmscriptenTarget = $EmscriptenVersion
    Invoke-Checked "Update emsdk tags" {
        .\emsdk.bat update-tags
    }
    Write-Host "==> Install Emscripten $EmscriptenVersion"
    .\emsdk.bat install $EmscriptenVersion
    if ($LASTEXITCODE -ne 0) {
        if ($EnableSourceBuildFallback) {
            Write-Host "Pinned Emscripten install failed. Switching to source-build fallback (sdk-main-64bit)."
            try {
                Ensure-MsvcForNinjaSourceBuild
                Ensure-NinjaForSourceBuild
                Clear-EmsdkSourceBuildCache -EmsdkRoot $EmsdkDir
                Invoke-Checked "Fallback install (source build): sdk-main-64bit" {
                    .\emsdk.bat install sdk-main-64bit --generator=Ninja
                }
                $EmscriptenTarget = "sdk-main-64bit"
            } catch {
                $armHint = ""
                if ($IsArm64Host) {
                    $armHint = "`nARM64 note: source-build is expected on this platform and can require substantial disk/time."
                }
                throw @"
Failed to install pinned Emscripten version '$EmscriptenVersion', and automatic source-build fallback also failed.
Cause: $($_.Exception.Message)

Required for source-build fallback:
  - Visual Studio Build Tools with Desktop C++ workload (cl.exe available)
  - Ninja available in PATH
$armHint
"@
            }
        } else {
            throw @"
Failed to install pinned Emscripten version '$EmscriptenVersion'.
If you are on Windows ARM64, precompiled SDKs may be unavailable.
Options:
  1) Run this script on Windows x64.
  2) Retry with source-build fallback (default behavior):
     .\tools\dev\bootstrap-toolchains.ps1
  3) Remove explicit fallback disable flag if used:
     .\tools\dev\bootstrap-toolchains.ps1 -DisableSourceBuildFallback:$false
"@
        }
    }
    Invoke-Checked "Activate Emscripten $EmscriptenTarget (embedded config)" {
        .\emsdk.bat activate $EmscriptenTarget --embedded
    }
}
finally {
    Pop-Location
}

$PythonCmd = Resolve-PythonCommand
if (-not (Test-Path $VenvDir)) {
    Invoke-Checked "Create local Python virtualenv (.tools/venv)" {
        & $PythonCmd.Exe @($PythonCmd.Args) -m venv $VenvDir
    }
}

$PipExe = Join-Path $VenvDir "Scripts\pip.exe"
if (-not (Test-Path $PipExe)) {
    throw "pip.exe not found in local venv: $PipExe"
}

Invoke-Checked "Upgrade pip in local venv" {
    & $PipExe install --upgrade pip
}
Invoke-Checked "Install SCons $SConsVersion and Ninja $NinjaVersion in local venv" {
    & $PipExe install "scons==$SConsVersion" "ninja==$NinjaVersion"
}

Write-Host ""
Write-Host "Bootstrap complete."
Write-Host "Next:"
Write-Host "  1) .\tools\dev\activate-toolchains.ps1"
Write-Host "  2) .\tools\dev\doctor-toolchains.ps1"
Write-Host "  3) .\tools\dev\build-wasm.ps1"
Write-Host "  4) .\tools\dev\build-godot.ps1"
