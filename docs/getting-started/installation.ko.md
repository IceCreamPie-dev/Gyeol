# 설치

## 사전 요구사항

| 플랫폼 | 필요 항목 |
|---|---|
| Windows (권장 개발 흐름) | CMake 3.15+, Git, Visual Studio C++ 도구체인, Python 3 |
| Linux/macOS (Core 빌드) | CMake 3.15+, Ninja, C++17 컴파일러, Git |

Core 의존성(FlatBuffers, Google Test, nlohmann/json)은 CMake가 자동으로 가져옵니다.

## 저장소 클론

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol
```

`godot-cpp` 서브모듈 때문에 `--recurse-submodules`가 필요합니다.

## 로컬 툴체인 부트스트랩 (Windows 기본)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

`bootstrap-toolchains.ps1`는 먼저 고정 버전 Emscripten(`4.0.23`) 설치를 시도하고,
prebuilt가 없으면 source-build(`sdk-main-64bit`)로 자동 fallback합니다.

Windows ARM64에서는 source-build가 일반 경로이며, 환경에 따라 30~90분 이상 소요될 수 있습니다.
fallback 필수 도구:
- Visual Studio Build Tools (Desktop C++)
- Ninja (로컬 venv 또는 Visual Studio 경로에서 자동 탐색)

자동 fallback을 명시적으로 끄고 싶다면:

```powershell
.\tools\dev\bootstrap-toolchains.ps1 -DisableSourceBuildFallback
```

이 과정은 `.tools/` 아래 로컬 도구를 준비합니다:
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

ARM64 산출물이 필요하면:

```powershell
.\tools\dev\build-godot.ps1 -Arch arm64
```

## 선택: 전역 툴체인

전역 `emsdk`/`scons`를 계속 사용할 수 있지만, 재현성을 위해 기본 권장 흐름은 로컬 `.tools/` 방식입니다.

## 로컬 표준 검증 게이트

CI 의도와 동일하게 로컬에서 점검하려면 아래 순서를 사용하세요:

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
.\tools\dev\build-wasm.ps1
.\tools\dev\build-godot.ps1
.\tools\dev\check-runtime-contract.ps1
```

## 다음 단계

- [빠른 시작](quick-start.md)
- [Godot 연동](godot-integration.md)
