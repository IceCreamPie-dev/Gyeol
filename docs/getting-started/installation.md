# Installation

## Prerequisites

| Dependency | Version | Notes |
|-----------|---------|-------|
| CMake | 3.15+ | Build system |
| Ninja | any | Build backend |
| C++17 compiler | GCC 8+ / MinGW / Clang 7+ | MSVC for GDExtension |
| Git | any | For submodules |

All other dependencies are fetched automatically:

| Library | Version | Purpose |
|---------|---------|---------|
| FlatBuffers | v24.3.25 | Binary serialization |
| Google Test | v1.14.0 | Unit testing |
| nlohmann/json | v3.11.3 | LSP server |

## Clone the Repository

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol
```

> **Note:** The `--recurse-submodules` flag is required to fetch the `godot-cpp` submodule for GDExtension builds.

## Build Core + Tools

```bash
cmake -B build -G Ninja
cmake --build build
```

This produces the following executables:

| Executable | Location | Purpose |
|-----------|----------|---------|
| `GyeolCompiler` | `build/src/gyeol_compiler/` | `.gyeol` to `.gyb` compiler |
| `GyeolTest` | `build/src/gyeol_core/` | Console interactive player |
| `GyeolLSP` | `build/src/gyeol_lsp/` | Language Server Protocol server |
| `GyeolDebugger` | `build/src/gyeol_debugger/` | CLI interactive debugger |
| `GyeolTests` | `build/src/tests/` | Unit test runner |
| `GyeolLSPTests` | `build/src/tests/` | LSP test runner |

## Build GDExtension (Godot)

Requires MSVC (Windows) and [SCons](https://scons.org/):

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

Output: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

> **Note:** The first build takes ~5 minutes as it compiles all godot-cpp bindings.

## Run Tests

```bash
cd build && ctest --output-on-failure
```

Or run test executables directly:

```bash
./build/src/tests/GyeolTests       # Core + Parser + Runner
./build/src/tests/GyeolLSPTests    # LSP Analyzer + Server
```

## Verify Installation

```bash
# Compile a test story
./build/src/gyeol_compiler/GyeolCompiler test.gyeol -o test.gyb

# Play it in console
./build/src/gyeol_core/GyeolTest test.gyb
```

## Project Structure

```
Gyeol/
  schemas/
    gyeol.fbs              # FlatBuffers schema
  src/
    gyeol_core/            # Core engine library + Runner VM
    gyeol_compiler/        # Parser + Compiler
    gyeol_lsp/             # LSP server
    gyeol_debugger/        # CLI debugger
    tests/                 # Google Test suites
  bindings/
    godot_extension/       # Godot 4.3 GDExtension (SCons)
  editors/
    vscode/                # VS Code extension (gyeol-lang)
  demo/
    godot/                 # Godot demo project
  docs/                    # This documentation
```

## Next Steps

- [Quick Start](quick-start.md) - Write and run your first story
- [Godot Integration](godot-integration.md) - Use Gyeol in Godot
