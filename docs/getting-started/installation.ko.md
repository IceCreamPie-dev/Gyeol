# 설치

## 사전 요구사항

| 플랫폼 | 필요 항목 |
|---|---|
| Windows (권장 개발 흐름) | CMake 3.15+, Git, Visual Studio C++ 툴체인, Python 3 |
| Linux/macOS (Core 빌드) | CMake 3.15+, Ninja, C++17 컴파일러, Git |

Core 의존성(FlatBuffers, Google Test, nlohmann/json)은 CMake가 자동으로 내려받습니다.

## 저장소 클론

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol
```

`godot-cpp` 서브모듈이 필요하므로 `--recurse-submodules`를 사용해야 합니다.

## 로컬 툴체인 부트스트랩 (Windows 기본)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

Windows ARM64 환경에서 prebuilt Emscripten 바이너리가 없으면 다음 fallback으로 다시 실행하세요:

```powershell
.\tools\dev\bootstrap-toolchains.ps1 -AllowSourceBuild
```

이 과정에서 `.tools/` 아래에 로컬 툴체인이 준비됩니다:
- `.tools/emsdk` (Emscripten)
- `.tools/venv` (SCons, Ninja)

## Core + 도구 빌드

```powershell
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

생성되는 주요 바이너리:
- `build/src/gyeol_compiler/GyeolCompiler`
- `build/src/gyeol_core/GyeolTest`
- `build/src/gyeol_lsp/GyeolLSP`
- `build/src/gyeol_debugger/GyeolDebugger`

## WASM 빌드

```powershell
.\tools\dev\build-wasm.ps1
```

기대 산출물:
- `build_wasm/dist/gyeol.js`
- `build_wasm/dist/*.wasm`

## Godot GDExtension 빌드

```powershell
.\tools\dev\build-godot.ps1
```

기대 산출물:
- `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

ARM64 아티팩트가 필요하면:

```powershell
.\tools\dev\build-godot.ps1 -Arch arm64
```

## 전역 툴체인 (선택)

전역 `emsdk`/`scons`를 계속 사용할 수 있지만, 재현성을 위해 기본 권장 흐름은 로컬 `.tools/` 방식입니다.

## 다음 단계

- [빠른 시작](quick-start.md)
- [Godot 연동](godot-integration.md)
