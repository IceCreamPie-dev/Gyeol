Set-StrictMode -Version Latest

function Get-GyeolToolchainVersions {
    return [ordered]@{
        EMSCRIPTEN_VERSION = "4.0.23"
        SCONS_VERSION = "4.8.1"
        NINJA_VERSION = "1.11.1.1"
    }
}
