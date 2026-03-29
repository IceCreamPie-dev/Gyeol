# Gyeol (결)

**Windows 전용 스토리 엔진 코어/툴체인**

Gyeol은 C++17 기반 인터랙티브 스토리 엔진입니다. 사람이 작성하는 정식 스토리 소스는 `.gyeol`이며, 공식 결과물/배포물은 `JSON IR`입니다. 런타임은 `Runner` VM을 통해 동일한 실행 규약을 유지합니다.

## 문서 바로가기

- [문서 홈](docs/index.md)
- [문서 스타일 가이드](docs/style-guide.md)
- [컴파일러 CLI](docs/tools/compiler.md)
- [JSON IR 명세](docs/json-ir-spec.md)
- [런타임 계약 (Runtime Contract v1.1)](docs/advanced/runtime-contract.md)

## 핵심 기능

- `.gyeol` 저작 + `JSON IR` 산출 파이프라인
- `Runner` VM 기반 계약 고정 실행 모델
- `locale catalog` 기반 다국어 핫스위치 + fallback 체인
- 상태 저장/복원(`.gys`) 및 conformance 테스트 체계
- Windows 중심 CLI/LSP/디버거 도구 제공

## 지원 범위

- 공식 지원 플랫폼: **Windows only**
- 공식 결과물/배포물: **JSON IR (`.json`)**
- 외부 엔진/툴 연동 구현은 파트너 책임이며, 본 저장소는 코어 계약/도구를 제공합니다.

## 빠른 시작 (`.gyeol`)

`hello.gyeol` 예시:

```text
label start:
    hero "안녕! 모험을 시작할까?"
    menu:
        "좋아" -> accept
        "다음에" -> decline

label accept:
    hero "좋아, 출발하자!"

label decline:
    hero "괜찮아, 다음에 보자."
```

검증 및 산출:

```bash
GyeolCompiler --validate hello.gyeol
GyeolCompiler --export-json-ir hello.gyeol -o hello.json
GyeolCompiler --validate-json-ir hello.json
```

## 빌드

### 사전 요구사항

- **Windows**: CMake 3.15+, Git, Visual Studio Build Tools (C++), Python 3

프로젝트는 로컬 툴체인(`.tools/`) 워크플로를 기본으로 사용합니다.

### 로컬 툴체인 부트스트랩 (Windows 권장)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

`bootstrap-toolchains.ps1`는 고정 버전 설치를 먼저 시도하고, 실패하면 기본값으로 source-build fallback(`sdk-main-64bit`)을 자동 시도합니다.

### Core + 도구 빌드

```powershell
git clone https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## 컴파일러 CLI (`.gyeol` 저작 우선)

```bash
GyeolCompiler --validate <story.gyeol>
GyeolCompiler --export-json-ir <story.gyeol> -o <story.json>
```

고급/내부 파이프라인(JSON IR 입력, graph patch, locale catalog)은 [컴파일러 CLI 문서](docs/tools/compiler.md)에 정리되어 있습니다.

## 외부 연동

외부 엔진(예: Godot/Unity/기타 런타임) 연동은 파트너 구현 범위입니다. 본 저장소는 연동 구현을 위한 계약 문서와 JSON IR/Runtime 검증 도구를 제공합니다.

## 테스트

```bash
cd build && ctest --output-on-failure
```

런타임 계약 검증:

```powershell
.\tools\dev\check-runtime-contract.ps1
```

## 프로젝트 구조

```text
schemas/gyeol.fbs           # FlatBuffers 스키마
src/
  gyeol_core/               # Core 엔진 (Story + Runner VM)
  gyeol_compiler/           # .gyeol 컴파일 + JSON IR/패치/번역 도구
  gyeol_lsp/                # LSP 서버
  gyeol_debugger/           # CLI 디버거
  tests/                    # Google Test 스위트
editors/
  vscode/                   # VS Code 확장
tools/
  dev/                      # 로컬 툴체인/검증 스크립트
```

## 라이선스

[MIT](LICENSE) - IceCreamPie
