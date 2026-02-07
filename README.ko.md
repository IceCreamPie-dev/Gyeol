# Gyeol (결)

**Write Once, Run Everywhere Story Engine**

[English](README.md)

C++17 기반 고성능 인터랙티브 스토리텔링 엔진. `.gyeol` 스크립트 하나만 작성하면, Godot, Unity(예정), WebAssembly(예정) 등 어떤 게임 엔진에서든 수정 없이 즉시 플레이할 수 있는 미들웨어입니다.

Ren'Py의 연출력 + Ink의 구조적 강점 + Yarn Spinner의 유연함을 하나로 통합했습니다.

## 주요 기능

- **스크립트 기반 내러티브** — Ren'Py 스타일 문법 (캐릭터, 대사, 선택지, 분기)
- **표현식 엔진** — 변수, 산술 연산 (`$ hp = hp - 10`), 인라인 조건 (`{if hp > 50}강함{endif}`)
- **함수 시스템** — 매개변수 지원 label, call/return 반환값, 로컬 스코프 섀도잉
- **방문 추적** — 노드 방문 횟수 자동 추적, `visit_count()` / `visited()` 함수
- **다국어** — 자동 생성 Line ID, CSV 기반 로케일 오버레이
- **세이브/로드** — 전체 상태 직렬화 (변수, 콜 스택, 방문 횟수) via FlatBuffers
- **랜덤 분기** — 가중치 기반 확률 분기, 결정적 시드 지원
- **제로 카피 바이너리** — `.gyb` (FlatBuffers) 포맷으로 빠른 로딩
- **개발 도구** — LSP 서버, CLI 디버거, VS Code 확장 (구문 하이라이팅)
- **엔진 바인딩** — Godot 4.3 GDExtension (Signal 기반 StoryPlayer 노드)

## 빠른 예제

```
label start:
    hero "안녕? 나는 결(Gyeol)이야. 같이 모험할래?"
    menu:
        "좋아, 같이 가자!" -> good
        "아니, 난 싫어." -> bad

label good:
    hero "좋았어! 우리 함께 떠나자!"

label bad:
    hero "그래... 다음에 또 만나자."
```

## 빌드

### 사전 요구사항

- **CMake** 3.15+
- **Ninja** 빌드 시스템
- **C++17 컴파일러** — GCC 8+ / MinGW / Clang 7+
- **Git** (서브모듈용)

의존성(FlatBuffers, Google Test, nlohmann/json)은 CMake FetchContent로 자동 다운로드됩니다.

### Core + 도구 빌드

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol

cmake -B build -G Ninja
cmake --build build
```

빌드 결과: `GyeolCompiler`, `GyeolLSP`, `GyeolDebugger`, `GyeolTest` (콘솔 플레이어), 테스트 스위트

### GDExtension 빌드 (Godot)

MSVC (Windows)와 [SCons](https://scons.org/) 필요:

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

출력: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

## 사용법

### 스크립트 컴파일

```bash
# .gyeol 텍스트 → .gyb 바이너리
GyeolCompiler story.gyeol -o story.gyb

# 번역 CSV 추출
GyeolCompiler story.gyeol --export-strings strings.csv
```

### 콘솔에서 플레이

```bash
GyeolTest story.gyb
```

### 인터랙티브 디버깅

```bash
GyeolDebugger story.gyb
# 커맨드: step, continue, break node:pc, locals, print var, where, info node
```

### Godot 연동

1. 스토리 컴파일: `GyeolCompiler story.gyeol -o demo/godot/story.gyb`
2. GDExtension 빌드 (위 참조)
3. Godot 4.3에서 `demo/godot/` 프로젝트 열기

`StoryPlayer` 노드 API:

| 메서드 | 설명 |
|--------|------|
| `load_story(path)` | `.gyb` 파일 로드 |
| `advance()` | 다음 단계로 진행 |
| `select_choice(index)` | 선택지 선택 |
| `save_state(path)` / `load_state(path)` | 세이브/로드 |
| `get_variable(name)` / `set_variable(name, value)` | 변수 접근 |
| `load_locale(path)` / `clear_locale()` | 다국어 로케일 |

시그널: `dialogue_line`, `choices_presented`, `command_received`, `story_ended`

## 스크립트 문법

```
import "common.gyeol"            # 다른 파일 임포트

$ health = 100                   # 전역 변수

label greet(name, title):        # 매개변수 함수
    narrator "안녕, {name}!"     # 문자열 보간
    narrator "{if health > 50}강해 보이네{else}약해 보이네{endif}"
    @ play_sfx "hello.wav"       # 엔진 커맨드
    menu:
        "계속하기" -> next
        "떠나기" -> exit if has_key    # 조건부 선택지
    if health > 80 -> strong
    elif health > 30 -> normal
    else -> weak
    random:                      # 가중치 랜덤 분기
        50 -> common_path
        10 -> rare_path
    $ result = call some_func(1, 2)   # 반환값 있는 호출
    return result + health       # 함수에서 반환

label next:
    narrator "계속 진행합니다..."
```

## 개발 도구

### VS Code 확장

`editors/vscode/`에 위치. 제공 기능:
- `.gyeol` 파일 구문 하이라이팅
- LSP 연동 (자동완성, 정의로 이동, 호버, 진단)
- 디버거 어댑터

### LSP 서버

JSON-RPC over stdin/stdout. 기능: 진단, 자동완성 (키워드/라벨/변수/내장 함수), 정의로 이동, 호버, 문서 심볼.

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

```
schemas/gyeol.fbs           # FlatBuffers 스키마 (전체 데이터 구조)
src/
  gyeol_core/               # Core 엔진 (Story 로더, Runner VM)
  gyeol_compiler/           # .gyeol → .gyb 컴파일러 + 파서
  gyeol_lsp/                # Language Server Protocol 서버
  gyeol_debugger/           # CLI 인터랙티브 디버거
  tests/                    # Google Test 스위트
bindings/
  godot_extension/          # Godot 4.3 GDExtension
editors/
  vscode/                   # VS Code 확장 (gyeol-lang)
demo/
  godot/                    # Godot 데모 프로젝트
```

## 라이선스

[MIT](LICENSE) - IceCreamPie
