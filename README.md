# Gyeol (결)

**Write Once, Run Everywhere Story Engine**

C++17 기반 고성능 인터랙티브 스토리텔링 엔진입니다. `.gyeol` 스크립트 하나만 작성하면 Godot, Unity(예정), WebAssembly(예정) 등 다양한 게임 엔진에서 수정 없이 즉시 플레이할 수 있습니다.

Ren'Py의 연출력, Ink의 구조적 강점, Yarn Spinner의 유연한 미들웨어 설계를 하나로 통합했습니다.

## 주요 기능

- **스크립트 기반 내러티브** - Ren'Py 스타일 문법으로 캐릭터, 대사, 선택지, 분기 작성
- **표현식 엔진** - 변수, 산술 연산(`$ hp = hp - 10`), 인라인 조건(`{if hp > 50}강함{endif}`)
- **함수 시스템** - 매개변수 라벨, 반환값이 있는 call/return, 로컬 스코프 섀도잉
- **방문 추적** - `visit_count()` / `visited()`를 통한 노드 방문 횟수 자동 추적
- **로컬라이징** - 자동 생성 Line ID + POT/PO 워크플로 + JSON/CSV locale 오버레이
- **저장/불러오기** - 변수, 호출 스택, 방문 횟수를 포함한 전체 상태 직렬화(FlatBuffers)
- **랜덤 분기** - 시드 기반 재현 가능한 가중치 랜덤 분기
- **제로카피 바이너리** - 빠른 로딩을 위한 `.gyb` (FlatBuffers) 포맷
- **개발 도구** - LSP 서버, CLI 디버거, VS Code 확장
- **엔진 바인딩** - Godot 4.3 GDExtension (시그널 기반 `StoryPlayer` 노드)

### 런타임 계약 하이라이트

- `saveState()` / `loadState()`는 deterministic RNG 진행 상태와 pending choice 메타데이터를 보존합니다.
- `snapshot()` / `restore()`는 `.gys` 파일과 별개인 메모리 내 체크포인트 경로를 제공합니다.
- `getLastError()`, `getMetrics()`, `getTrace()`로 바인딩/도구에서 공통 진단 표면을 사용할 수 있습니다.
- `getSeed()`로 현재 런타임 시드를 확인해 재현 실행과 디버깅에 활용할 수 있습니다.

## 빠른 예제

```gyeol
label start:
    hero "안녕, 나는 Gyeol이야. 같이 모험할래?"
    menu:
        "좋아, 같이 가자!" -> good
        "아니, 난 패스." -> bad

label good:
    hero "좋았어! 함께 출발하자!"

label bad:
    hero "그래... 다음에 보자."
```

## 빌드

### 사전 요구사항

- **Windows**: CMake 3.15+, Git, Visual Studio Build Tools (C++), Python 3
- **Linux/macOS (Core 빌드)**: CMake 3.15+, Ninja, C++17 컴파일러, Git

프로젝트는 로컬 툴체인(`.tools/`) 기준 워크플로를 기본으로 사용합니다.

### 로컬 툴체인 부트스트랩 (Windows 권장)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

Windows ARM64에서 prebuilt Emscripten SDK가 없으면 다음으로 source-build fallback을 사용할 수 있습니다:

```powershell
.\tools\dev\bootstrap-toolchains.ps1 -AllowSourceBuild
```

이 스크립트는 다음을 설정합니다:
- `.tools/emsdk` (Emscripten)
- `.tools/venv` (SCons, Ninja 포함)
- 현재 셸용 PATH/EMSDK 환경

> 전역 설치(CMake/SCons/emsdk)를 이미 쓰고 있다면 계속 사용할 수 있지만, 기본 권장 경로는 로컬 툴체인입니다.

### Core + 도구 빌드

```powershell
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol

cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

다음 타깃이 빌드됩니다: `GyeolCompiler`, `GyeolLSP`, `GyeolDebugger`, `GyeolTest`(콘솔 플레이어), 테스트 스위트.

### WASM 빌드

```powershell
.\tools\dev\build-wasm.ps1
```

산출물: `build_wasm/dist/gyeol.js`, `build_wasm/dist/*.wasm`

### GDExtension 빌드 (Godot)

```powershell
.\tools\dev\build-godot.ps1
```

출력: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

ARM64 타깃으로 빌드하려면:

```powershell
.\tools\dev\build-godot.ps1 -Arch arm64
```

## 사용법

### 스크립트 컴파일

```bash
# .gyeol 텍스트 -> .gyb 바이너리
GyeolCompiler story.gyeol -o story.gyb

# 번역 템플릿(POT) 추출
GyeolCompiler story.gyeol --export-strings-po strings.pot

# (하위 호환) 번역 CSV 추출
GyeolCompiler story.gyeol --export-strings strings.csv

# 번역 PO -> 런타임 JSON locale 변환
GyeolCompiler --po-to-json strings_ko.po -o ko.locale.json --locale ko
```

### 콘솔에서 플레이

```bash
GyeolTest story.gyb
```

### 인터랙티브 디버깅

```bash
GyeolDebugger story.gyb
# Commands: step, continue, break node:pc, locals, print var, where, info node
```

### Godot 연동

1. 스토리 컴파일: `GyeolCompiler story.gyeol -o demo/godot/story.gyb`
2. GDExtension 빌드(위 참조)
3. Godot 4.3에서 `demo/godot/` 프로젝트 열기

`StoryPlayer` 노드 API:

| Method | Description |
|--------|-------------|
| `load_story(path)` | `.gyb` 파일 로드 |
| `advance()` | 다음 단계 진행 |
| `choose(index)` | 선택지 선택 |
| `save_state(path)` / `load_state(path)` | 저장/불러오기 |
| `get_variable(name)` / `set_variable(name, value)` | 변수 접근 |
| `load_locale(path)` / `clear_locale()` | 로컬라이징 |

Signals: `dialogue_line`, `choices_presented`, `command_received`, `story_ended`

## 스크립트 문법

```gyeol
import "common.gyeol"            # 다른 파일 import

$ health = 100                   # 전역 변수

label greet(name, title):        # 매개변수가 있는 함수형 라벨
    narrator "안녕, {name}!"      # 문자열 보간
    narrator "{if health > 50}강해 보이네{else}약해 보이네{endif}"
    @ play_sfx "hello.wav"       # 엔진 커맨드
    menu:
        "계속" -> next
        "나가기" -> exit if has_key    # 조건부 선택지
    if health > 80 -> strong
    elif health > 30 -> normal
    else -> weak
    random:                      # 가중치 랜덤 분기
        50 -> common_path
        10 -> rare_path
    $ result = call some_func(1, 2)    # 반환값 받는 call
    return result + health       # 함수 반환

label next:
    narrator "계속 진행한다..."
```

## 개발 도구

### VS Code 확장

`editors/vscode/`에 위치하며 아래 기능을 제공합니다.
- `.gyeol` 문법 하이라이팅
- LSP 연동(자동완성, 정의로 이동, hover, 진단)
- 디버거 어댑터

### LSP 서버

stdin/stdout JSON-RPC 기반입니다. 진단, 자동완성(키워드/라벨/변수/내장 함수), 정의로 이동, hover, 문서 심볼을 지원합니다.

## 테스트

Google Test 기반 자동화 테스트 318개:

```bash
# 전체 테스트 실행
cd build && ctest --output-on-failure

# 직접 실행
./build/src/tests/GyeolTests       # 259 tests (Core + Parser + Runner)
./build/src/tests/GyeolLSPTests    # 59 tests (LSP Analyzer + Server)
```

## 프로젝트 구조

```text
schemas/gyeol.fbs           # FlatBuffers 스키마(전체 데이터 구조)
src/
  gyeol_core/               # Core 엔진(Story 로더, Runner VM)
  gyeol_compiler/           # .gyeol -> .gyb 컴파일러 + 파서
  gyeol_lsp/                # Language Server Protocol 서버
  gyeol_debugger/           # CLI 인터랙티브 디버거
  tests/                    # Google Test 스위트
bindings/
  godot_extension/          # Godot 4.3 GDExtension
editors/
  vscode/                   # VS Code 확장(gyeol-lang)
demo/
  godot/                    # Godot 데모 프로젝트
tools/
  dev/                      # 로컬 툴체인 부트스트랩/검증 스크립트
```

## 라이선스

[MIT](LICENSE) - IceCreamPie
