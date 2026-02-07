# Gyeol (결) - Narrative Engine

> "Write Once, Run Everywhere Story Engine"

## Project Vision

Ren'Py의 연출력 + Ink의 구조적 강점 + Yarn의 유연함을 통합한 C++ 기반 고성능 인터랙티브 스토리텔링 엔진. 텍스트 스크립트(.gyeol) 하나만 작성하면, 어떤 게임 엔진에서든 수정 없이 즉시 플레이 가능한 미들웨어.

## Roadmap

- ~~**Step 1:** 데이터 순환 고리 완성 — Compiler → .gyb 저장 → Core Loader~~ (완료)
- ~~**Step 2:** VM 로직 — Stepper, 변수/조건문 처리, 선택지 분기, 콘솔 플레이~~ (완료)
- ~~**Step 3:** 텍스트 파서 — .gyeol 스크립트 → .gyb 컴파일 (순수 C++ 파서)~~ (완료)
- ~~**Step 4:** Godot 연동 — GDExtension, StoryPlayer 노드, Signal 기반 UI 연결~~ (완료)

## Build System

- **CMake 3.15+** with **Ninja** backend (Core + Compiler)
- **SCons** (GDExtension 빌드)
- **Compiler:** GCC/MinGW (C:\Strawberry\c\bin\c++.exe), MSVC (GDExtension)
- **C++ Standard:** C++17 (required)
- FlatBuffers v24.3.25 fetched automatically via CMake FetchContent

### Build Commands

```bash
# Core + Compiler (CMake)
cmake -B build -G Ninja
cmake --build build

# GDExtension (SCons)
cd bindings/godot_extension
scons platform=windows target=template_debug
```

### Build Targets

- `GyeolCore` — Main engine library (CMake)
- `GyeolParser` — Parser library (CMake, GyeolCompiler + GyeolTests 공유)
- `GyeolCompiler` — .gyeol → .gyb compiler (CMake)
- `GyeolTest` — Console interactive player (CMake)
- `GyeolTests` — Google Test 유닛 테스트 (CMake)
- `libgyeol.dll` — Godot GDExtension (SCons)

## Project Structure

```
schemas/
  gyeol.fbs              # FlatBuffers schema (모든 데이터 구조 정의)
src/
  gyeol_core/            # Core engine library
    include/
      gyeol_story.h      # Story 클래스 (로드, 출력, 버퍼 접근)
      gyeol_runner.h     # Runner VM (step/choose 이벤트 기반 실행)
      generated/         # Auto-generated FlatBuffers C++ headers (from .fbs)
    src/
      gyeol_story.cpp    # Story 구현 (loadFromFile, printStory)
      gyeol_runner.cpp   # Runner VM 구현 (step, choose, 변수, 조건, 콜스택)
      test_main.cpp      # 콘솔 인터랙티브 플레이어
  gyeol_compiler/        # Compiler: .gyeol 텍스트 → .gyb 바이너리
    compiler_main.cpp    # CLI 엔트리포인트
    gyeol_parser.h       # Parser 클래스 API
    gyeol_parser.cpp     # Line-by-line 파서 구현
  tests/                 # Google Test 유닛 테스트 (109 tests)
    test_helpers.h       # 테스트 유틸리티 (compileScript, startRunner)
    test_parser.cpp      # Parser 테스트 (39 cases)
    test_runner.cpp      # Runner VM 테스트 (48 cases)
    test_story.cpp       # Story loader 테스트 (4 cases)
    test_saveload.cpp    # Save/Load 라운드트립 테스트 (9 cases)
bindings/
  godot_extension/       # GDExtension (SCons 빌드)
    godot-cpp/           # git submodule (4.3 branch)
    src/
      gyeol_story_player.h/cpp  # StoryPlayer 노드 (Godot 래퍼)
      register_types.h/cpp      # GDExtension 엔트리포인트
    SConstruct
  unity_plugin/          # Unity Native Plugin (planned)
  wasm/                  # WebAssembly (planned)
demo/
  godot/                 # Godot 데모 프로젝트
    project.godot
    main.tscn / main.gd  # 데모 씬 (StoryPlayer + UI)
    bin/
      gyeol.gdextension  # Extension 설정
      libgyeol.*.dll      # 빌드 출력
    test.gyb              # 컴파일된 테스트 스토리
```

## Code Conventions

- **Namespaces:** `Gyeol` (public API), `ICPDev::Gyeol::Schema` (FlatBuffers data)
- **Classes/Targets:** PascalCase (`Story`, `NodeT`, `GyeolCore`)
- **Files:** snake_case (`gyeol_story.h`, `compiler_main.cpp`)
- **Header guards:** `#pragma once`
- **Memory:** `std::unique_ptr` for owned objects
- Comments may be in Korean

## Architecture

- **String Pool** — 모든 텍스트를 중앙 풀에 저장, 인덱스로 참조 (메모리 효율 + 다국어)
- **FlatBuffers Object API** — `T` suffix types (`StoryT`, `NodeT`) for building, Pack/Unpack for serialization
- **Union types** — `OpData` (Line/Choice/Jump/Command/SetVar/Condition), `ValueData` (Bool/Int/Float/String), `ExprOp` (산술 연산)
- **Expression system** — RPN 토큰 리스트 (Shunting-yard 알고리즘으로 infix→RPN 변환, 스택 머신 평가)
- **Node graph** — 노드 이름 기반 참조, Jump으로 분기, is_call로 스택 Call/Return 지원
- **Binary format** — `.gyb` (Gyeol Binary, Story), `.gys` (Gyeol Save, SaveState) — zero-copy FlatBuffers
- **Runner VM** — 이벤트 기반 step()/choose() API, 엔진 독립적 설계
  - `StepResult` 반환: LINE (대사), CHOICES (선택지), COMMAND (엔진 명령), END
  - `Variant` 타입으로 변수 저장 (Bool/Int/Float/String)
  - 산술 표현식: `$ x = x + 1`, `$ y = a * 2 + b` — Shunting-yard → RPN → 스택 머신
  - 논리 연산자: `if hp > 0 and has_key == true`, `or`, `not` — 비교+논리 RPN 플래튼
  - 문자열 보간: `"Hello {name}!"` → 런타임 변수 치환, LINE/CHOICES 텍스트 지원
  - Call stack으로 Jump is_call=true 지원 (서브루틴 호출/복귀)
  - Save/Load: `saveState(filepath)` / `loadState(filepath)` — `.gys` FlatBuffers 바이너리
  - Variable API: `getVariable()`, `setVariable()`, `hasVariable()`, `getVariableNames()`
- **Parser** — Ren'Py 스타일 순수 C++ line-by-line 파서, 외부 의존성 없음
  - 에러 복구: 첫 에러에서 멈추지 않고 모든 에러 수집 (`getErrors()`)
  - Jump target 검증: 파싱 후 모든 jump/choice/condition 타겟 유효성 검사
  - global_vars: label 앞 `$ var = val` 선언 → Story.global_vars에 저장
  - voice_asset_id: 대사 뒤 `#voice:파일명` 태그로 보이스 에셋 연결
- **Compiler CLI** — `GyeolCompiler <input> [-o output]`, `-h`/`--help`, `--version`, 다중 에러 출력
- **GDExtension** — StoryPlayer 노드, Signal 기반 (dialogue_line, choices_presented, command_received, story_ended)
  - `save_state(path)` / `load_state(path)` — Godot 경로 (res://, user://) 지원
  - `get_variable(name)` / `set_variable(name, value)` / `has_variable(name)` — 변수 접근

## .gyeol 스크립트 문법

```
$ 전역변수 = 값                     # Global variable (label 앞에 선언)

label 노드이름:                     # 노드 선언 (첫 label = start_node)
    캐릭터 "대사"                   # Line (캐릭터 대사)
    캐릭터 "Hello {name}!"          # Line + 문자열 보간 ({변수} → 런타임 치환)
    캐릭터 "대사" #voice:파일명      # Line + voice asset
    "나레이션 텍스트"                # Line (character_id = -1)
    menu:                          # 선택지 블록
        "선택지 텍스트" -> 노드      # Choice
        "선택지" -> 노드 if 변수     # 조건부 Choice
    jump 노드이름                   # Jump (is_call=false)
    call 노드이름                   # Jump (is_call=true, 서브루틴)
    $ 변수 = 값                     # SetVar (true/false/정수/실수/"문자열")
    $ 변수 = 표현식                  # SetVar + 산술 (x + 1, a * 2 + b, (x + y) * 3)
    if 변수 op 값 -> 참 else 거짓    # Condition (==, !=, >, <, >=, <=)
    if 표현식 op 표현식 -> 참 else 거짓 # Condition + 산술 (hp-10 > 0, x+y == z)
    if 조건 and 조건 -> 참 else 거짓   # 논리 AND (둘 다 참)
    if 조건 or 조건 -> 참 else 거짓    # 논리 OR (하나 이상 참)
    if not 조건 -> 참 else 거짓       # 논리 NOT (부정)
    @ 명령 파라미터1 파라미터2        # Command (bg, sfx 등)
    # 주석
```

## Schema (schemas/gyeol.fbs)

| Type | Purpose |
|------|---------|
| `Story` | Root object — version, string_pool, global_vars, nodes, start_node_name |
| `Node` | 스토리 단위 (= Ren'Py Label, Ink Knot) — name + instructions |
| `Instruction` | OpData union wrapper |
| `Line` | 대사 — character_id, text_id, voice_asset_id |
| `Choice` | 선택지 — text_id, target_node, optional condition |
| `Jump` | 흐름 제어 — target_node, is_call flag |
| `Command` | 엔진 명령 (bg, sfx 등) — type_id, params[] |
| `SetVar` | 변수 설정 — var_name_id, ValueData (리터럴), Expression (산술) |
| `Expression` | RPN 토큰 리스트 — ExprToken[] (산술/비교/논리 연산자) |
| `Condition` | 조건 분기 — var/lhs_expr, op, compare_value/rhs_expr, true/false jumps |
| `SaveState` | 세이브 루트 — version, node, pc, finished, variables, call_stack, pending_choices |
| `SavedVar` | 저장된 변수 — name, ValueData, string_value |
| `SavedCallFrame` | 콜 스택 프레임 — node_name, pc |
| `SavedPendingChoice` | 대기 선택지 — text, target_node_name |

## Compiler Warnings

```
-Wall -pedantic -Wextra -Werror=unused-parameter -Wold-style-cast
-Wnon-virtual-dtor -Wzero-as-null-pointer-constant -faligned-new -Wextra-semi
```

## Dependencies

- **FlatBuffers** v24.3.25 (auto-fetched via CMake FetchContent)
- **Google Test** v1.14.0 (auto-fetched via CMake FetchContent)
- **godot-cpp** 4.3 branch (git submodule at `bindings/godot_extension/godot-cpp`)

## Testing

Google Test v1.14.0 기반 자동화 테스트 (109 tests):

```bash
# 유닛 테스트 실행
./build/src/tests/GyeolTests.exe

# CTest로 실행
cd build && ctest --output-on-failure
```

테스트 범위:
- **ParserTest** (39): 문법 요소별 파싱, 에스케이프, String Pool, voice_asset, global_vars, jump 검증, 표현식, 조건 표현식, 논리 연산자
- **ParserErrorTest** (9): 에러 케이스, 에러 복구, 다중 에러 수집, 잘못된 jump/choice/condition 타겟
- **RunnerTest** (48): VM 실행 흐름, 선택지, Jump/Call, 변수/조건, Command, 변수 API, 산술 표현식, 문자열 보간, 조건 표현식, 논리 연산자
- **StoryTest** (4): .gyb 로드/검증, 잘못된 파일 처리
- **SaveLoadTest** (9): 라운드트립, 선택지/변수/콜스택 저장복원, 에러 케이스

수동 파이프라인 테스트:
```bash
./build/src/gyeol_compiler/GyeolCompiler.exe test.gyeol           # .gyeol → story.gyb
./build/src/gyeol_compiler/GyeolCompiler.exe test.gyeol -o out.gyb # 출력 파일명 지정
./build/src/gyeol_core/GyeolTest.exe story.gyb                     # 콘솔에서 플레이
```

예제 스크립트: `test.gyeol` (기본), `test_advanced.gyeol` (변수/조건/명령/Call 테스트)

### Godot 데모

```bash
# 1. 테스트 스토리 컴파일
./build/src/gyeol_compiler/GyeolCompiler.exe test.gyeol -o demo/godot/test.gyb

# 2. GDExtension 빌드
cd bindings/godot_extension && scons platform=windows target=template_debug

# 3. Godot 에디터에서 demo/godot/ 프로젝트 열기
```
