# Installation

## Prerequisites

| Platform | Required |
|---|---|
| Windows (recommended dev flow) | CMake 3.15+, Git, Visual Studio C++ toolchain, Python 3 |
| Linux/macOS (core build) | CMake 3.15+, Ninja, C++17 compiler, Git |

Core dependencies (FlatBuffers, Google Test, nlohmann/json) are fetched automatically by CMake.

## Clone the Repository

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol
```

`--recurse-submodules` is required for the `godot-cpp` submodule.

## Local Toolchain Bootstrap (Windows default)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

If Windows ARM64 does not provide prebuilt Emscripten binaries on your machine, retry bootstrap with:

```powershell
.\tools\dev\bootstrap-toolchains.ps1 -AllowSourceBuild
```

This prepares local tools under `.tools/`:
- `.tools/emsdk` for Emscripten
- `.tools/venv` for SCons and Ninja

## Build Core + Tools

```powershell
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Generated binaries include:
- `build/src/gyeol_compiler/GyeolCompiler`
- `build/src/gyeol_core/GyeolTest`
- `build/src/gyeol_lsp/GyeolLSP`
- `build/src/gyeol_debugger/GyeolDebugger`

## Build WASM

```powershell
.\tools\dev\build-wasm.ps1
```

Expected outputs:
- `build_wasm/dist/gyeol.js`
- `build_wasm/dist/*.wasm`

## Build Godot GDExtension

```powershell
.\tools\dev\build-godot.ps1
```

Expected output:
- `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

For ARM64 artifact:

```powershell
.\tools\dev\build-godot.ps1 -Arch arm64
```

## Optional Global Toolchain

You can still use globally installed `emsdk`/`scons`, but the project default is the local `.tools/` workflow for reproducibility.

## Next Steps

- [Quick Start](quick-start.md)
- [Godot Integration](godot-integration.md)
