# 설치

## 사전 요구사항

| 의존성 | 버전 | 비고 |
|--------|------|------|
| CMake | 3.15+ | 빌드 시스템 |
| Ninja | 모든 버전 | 빌드 백엔드 |
| C++17 컴파일러 | GCC 8+ / MinGW / Clang 7+ | GDExtension은 MSVC 필요 |
| Git | 모든 버전 | 서브모듈 가져오기용 |

이외의 의존성은 자동으로 가져옵니다:

| 라이브러리 | 버전 | 용도 |
|-----------|------|------|
| FlatBuffers | v24.3.25 | 바이너리 직렬화 |
| Google Test | v1.14.0 | 유닛 테스트 |
| nlohmann/json | v3.11.3 | LSP 서버 |

## 저장소 클론

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol
```

> **참고:** `--recurse-submodules` 플래그는 GDExtension 빌드에 필요한 `godot-cpp` 서브모듈을 가져오기 위해 반드시 필요합니다.

## Core + 도구 빌드

```bash
cmake -B build -G Ninja
cmake --build build
```

빌드가 완료되면 다음 실행 파일이 생성됩니다:

| 실행 파일 | 위치 | 용도 |
|----------|------|------|
| `GyeolCompiler` | `build/src/gyeol_compiler/` | `.gyeol` → `.gyb` 컴파일러 |
| `GyeolTest` | `build/src/gyeol_core/` | 콘솔 인터랙티브 플레이어 |
| `GyeolLSP` | `build/src/gyeol_lsp/` | Language Server Protocol 서버 |
| `GyeolDebugger` | `build/src/gyeol_debugger/` | CLI 인터랙티브 디버거 |
| `GyeolTests` | `build/src/tests/` | 유닛 테스트 실행기 |
| `GyeolLSPTests` | `build/src/tests/` | LSP 테스트 실행기 |

## GDExtension 빌드 (Godot)

Windows에서 MSVC와 [SCons](https://scons.org/)가 필요합니다:

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

출력: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

> **참고:** 첫 빌드 시 godot-cpp 바인딩 전체를 컴파일하므로 약 5분 정도 소요됩니다.

## 테스트 실행

```bash
cd build && ctest --output-on-failure
```

또는 테스트 실행 파일을 직접 실행합니다:

```bash
./build/src/tests/GyeolTests       # Core + Parser + Runner
./build/src/tests/GyeolLSPTests    # LSP Analyzer + Server
```

## 설치 확인

```bash
# 테스트 스토리 컴파일
./build/src/gyeol_compiler/GyeolCompiler test.gyeol -o test.gyb

# 콘솔에서 플레이
./build/src/gyeol_core/GyeolTest test.gyb
```

## 프로젝트 구조

```
Gyeol/
  schemas/
    gyeol.fbs              # FlatBuffers 스키마
  src/
    gyeol_core/            # Core 엔진 라이브러리 + Runner VM
    gyeol_compiler/        # Parser + Compiler
    gyeol_lsp/             # LSP 서버
    gyeol_debugger/        # CLI 디버거
    tests/                 # Google Test 테스트 스위트
  bindings/
    godot_extension/       # Godot 4.3 GDExtension (SCons)
  editors/
    vscode/                # VS Code 확장 (gyeol-lang)
  demo/
    godot/                 # Godot 데모 프로젝트
  docs/                    # 이 문서
```

## 다음 단계

- [빠른 시작](quick-start.md) - 첫 스토리 작성 및 실행
- [Godot 연동](godot-integration.md) - Godot에서 Gyeol 사용하기
