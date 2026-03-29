# 설치

## 사전 요구사항

| 플랫폼 | 필요 항목 |
|---|---|
| Windows (권장 개발 흐름) | CMake 3.15+, Git, Visual Studio C++ 도구체인, Python 3 |

Core 의존성(FlatBuffers, Google Test, nlohmann/json)은 CMake가 자동으로 가져옵니다.

## 저장소 클론

```bash
git clone https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol
```

## 로컬 툴체인 부트스트랩 (Windows 기본)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

이 과정은 `.tools/` 아래 로컬 도구를 준비합니다.

## Core + 도구 빌드

```powershell
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

생성되는 주요 바이너리:
- `build/src/gyeol_compiler/GyeolCompiler`
- `build/src/gyeol_lsp/GyeolLSP`
- `build/src/gyeol_debugger/GyeolDebugger`
- `build/src/tests/GyeolRuntimeContractCLI`
- `build/src/tests/GyeolRuntimePerfCLI`

## 런타임 계약 검증

```powershell
.\tools\dev\check-runtime-contract.ps1
```

## 로컬 표준 검증 게이트

CI 의도와 동일하게 로컬에서 점검하려면 아래 순서를 사용하세요:

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
.\tools\dev\check-runtime-contract.ps1
```

## 다음 단계

- [빠른 시작](quick-start.md)
- [JSON IR 도구 흐름](json-ir-workflow.md)
