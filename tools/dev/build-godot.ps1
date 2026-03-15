param(
    [string]$Platform = "windows",
    [string]$Target = "template_debug",
    [string]$Arch = "x86_64",
    [switch]$SkipActivate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $SkipActivate) {
    . (Join-Path $PSScriptRoot "activate-toolchains.ps1")
}

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$FlatbuffersInclude = Join-Path $RepoRoot "build\_deps\flatbuffers-src\include"
$GodotBindingDir = Join-Path $RepoRoot "bindings\godot_extension"
$OutputDir = Join-Path $RepoRoot "demo\godot\bin"

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

function Ensure-MsvcCompiler {
    param([Parameter(Mandatory = $true)][string]$RequestedArch)
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

    $argVariants = if ($RequestedArch -eq "arm64") {
        @("-arch=arm64 -host_arch=arm64", "-arch=arm64 -host_arch=x64", "-arch=x64 -host_arch=arm64", "-arch=x64 -host_arch=x64")
    } else {
        @("-arch=x64 -host_arch=arm64", "-arch=x64 -host_arch=x64", "-arch=amd64 -host_arch=amd64")
    }

    foreach ($vsDevCmdPath in $vsDevCmdCandidates) {
        foreach ($args in $argVariants) {
            try {
                Import-CmdEnvironment -CommandLine "`"$vsDevCmdPath`" $args"
                if (Get-Command cl -ErrorAction SilentlyContinue) {
                    return
                }
            } catch {
                # Try next combination.
            }
        }
    }

    throw "MSVC compiler (cl.exe) is not available. Install Visual Studio Build Tools (Desktop C++) or run from Developer PowerShell."
}

if (-not (Test-Path $FlatbuffersInclude)) {
    Invoke-Checked "Configure root build to fetch FlatBuffers headers" {
        cmake -S . -B build
    }
}

Ensure-MsvcCompiler -RequestedArch $Arch

Push-Location $GodotBindingDir
try {
    Invoke-Checked "Build Godot GDExtension via SCons" {
        scons "platform=$Platform" "target=$Target" "arch=$Arch"
    }
}
finally {
    Pop-Location
}

$Artifact = Get-ChildItem -Path $OutputDir -Filter "libgyeol*" -File -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $Artifact) {
    throw "Missing artifact: libgyeol* in $OutputDir"
}

Write-Host "Godot extension build succeeded."
Write-Host "  Artifact: $($Artifact.FullName)"
