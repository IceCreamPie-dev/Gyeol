# Gyeol (결)

**한번 작성하고, 어디서든 실행하는 스토리 엔진**

Gyeol은 C++17 기반 인터랙티브 스토리 엔진입니다. 정식 소스 산출물은 canonical `JSON IR`(`format: "gyeol-json-ir"`, `format_version: 2`)로 고정되며, 런타임은 `Runner` VM을 통해 동일한 실행 규약을 유지합니다.

## 문서 바로가기

- [문서 홈](docs/index.md)
- [문서 스타일 가이드](docs/style-guide.md)
- [컴파일러 CLI](docs/tools/compiler.md)
- [JSON IR 명세](docs/json-ir-spec.md)
- [런타임 계약 (Runtime Contract v1.1)](docs/advanced/runtime-contract.md)

## 핵심 기능

- `JSON IR` 검증/컴파일/패치 파이프라인
- `Runner` VM 기반 엔진 독립 실행 모델
- `locale catalog` 기반 다국어 핫스위치 + fallback 체인
- 상태 저장/복원(`.gys`) 및 conformance 테스트 체계
- LSP/디버거/Godot 연동 도구 제공

## 빠른 시작 (JSON IR)

`hello.json` 예시:

```json
{
  "format": "gyeol-json-ir",
  "format_version": 2,
  "version": "0.2.0",
  "start_node_name": "start",
  "nodes": [
    {
      "name": "start",
      "instructions": [
        { "type": "Line", "character": "hero", "text": "안녕! 모험을 시작할까?" },
        { "type": "Choice", "text": "좋아", "target_node": "accept" },
        { "type": "Choice", "text": "다음에", "target_node": "decline" }
      ]
    },
    {
      "name": "accept",
      "instructions": [
        { "type": "Line", "character": "hero", "text": "좋아, 출발하자!" }
      ]
    },
    {
      "name": "decline",
      "instructions": [
        { "type": "Line", "character": "hero", "text": "괜찮아, 다음에 보자." }
      ]
    }
  ]
}
```

컴파일 및 실행:

```bash
# 빈 템플릿부터 시작할 때만 사용
# GyeolCompiler --init-json-ir hello.json
GyeolCompiler --lint-json-ir hello.json
GyeolCompiler --validate-json-ir hello.json
GyeolCompiler --compile-json-ir hello.json -o hello.gyb
GyeolTest hello.gyb
```

## 빌드

### 사전 요구사항

- **Windows**: CMake 3.15+, Git, Visual Studio Build Tools (C++), Python 3
- **Linux/macOS (Core 빌드)**: CMake 3.15+, Ninja, C++17 컴파일러, Git

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
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

### WASM 빌드

```powershell
.\tools\dev\build-wasm.ps1
```

### GDExtension 빌드 (Godot)

```powershell
.\tools\dev\build-godot.ps1
```

## 컴파일러 CLI (JSON IR 전용)

```bash
GyeolCompiler --init-json-ir <story.json>
GyeolCompiler --lint-json-ir <story.json> [--format text|json]
GyeolCompiler --validate-json-ir <story.json>
GyeolCompiler --format-json-ir <story.json> [-o <story.json>]
GyeolCompiler --compile-json-ir <story.json> -o <story.gyb>
GyeolCompiler --export-graph-json <story.json> -o <story.graph.json>
GyeolCompiler --preview-graph-patch <story.json> --patch <patch.json> [--format text|json]
GyeolCompiler --apply-graph-patch <story.json> --patch <patch.json> -o <story.json>

GyeolCompiler --export-strings-po-from-json-ir <story.json> -o <strings.pot>
GyeolCompiler --export-locale-template <story.json> -o <locale.template.json>
GyeolCompiler --po-to-locale-json <strings.po> --story <story.json> -o <locale.json> [--locale <code>]
GyeolCompiler --validate-locale-json <locale.json> --story <story.json>
GyeolCompiler --build-locale-catalog <localeA.json> <localeB.json> ... -o <catalog.json> [--default-locale <code>]
```

자세한 옵션은 [컴파일러 CLI 문서](docs/tools/compiler.md)를 참고하세요.

## Godot 연동 요약

1. `JSON IR` 컴파일: `GyeolCompiler --compile-json-ir story.json -o demo/godot/story.gyb`
2. `GDExtension` 빌드: `.\tools\dev\build-godot.ps1`
3. Godot 4.3에서 `demo/godot/` 프로젝트 실행

`StoryPlayer` API 상세는 [StoryPlayer 문서](docs/api/class-story-player.md)를 참고하세요.

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
  gyeol_compiler/           # JSON IR 검증/컴파일/패치 도구
  gyeol_lsp/                # LSP 서버
  gyeol_debugger/           # CLI 디버거
  tests/                    # Google Test 스위트
bindings/
  godot_extension/          # Godot 4.3 GDExtension
editors/
  vscode/                   # VS Code 확장
demo/
  godot/                    # Godot 데모 프로젝트
tools/
  dev/                      # 로컬 툴체인/검증 스크립트
```

## 라이선스

[MIT](LICENSE) - IceCreamPie
