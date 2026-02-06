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
- `GyeolCompiler` — .gyeol → .gyb compiler (CMake)
- `GyeolTest` — Console interactive player (CMake)
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
  tests/                 # (Planned)
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
- **Union types** — `OpData` (Line/Choice/Jump/Command/SetVar/Condition), `ValueData` (Bool/Int/Float/String)
- **Node graph** — 노드 이름 기반 참조, Jump으로 분기, is_call로 스택 Call/Return 지원
- **Binary format** — `.gyb` files (Gyeol Binary), zero-copy loading via FlatBuffers
- **Runner VM** — 이벤트 기반 step()/choose() API, 엔진 독립적 설계
  - `StepResult` 반환: LINE (대사), CHOICES (선택지), COMMAND (엔진 명령), END
  - `Variant` 타입으로 변수 저장 (Bool/Int/Float/String)
  - Call stack으로 Jump is_call=true 지원 (서브루틴 호출/복귀)
- **Parser** — Ren'Py 스타일 순수 C++ line-by-line 파서, 외부 의존성 없음
- **GDExtension** — StoryPlayer 노드, Signal 기반 (dialogue_line, choices_presented, command_received, story_ended)

## .gyeol 스크립트 문법

```
label 노드이름:                     # 노드 선언 (첫 label = start_node)
    캐릭터 "대사"                   # Line (캐릭터 대사)
    "나레이션 텍스트"                # Line (character_id = -1)
    menu:                          # 선택지 블록
        "선택지 텍스트" -> 노드      # Choice
        "선택지" -> 노드 if 변수     # 조건부 Choice
    jump 노드이름                   # Jump (is_call=false)
    call 노드이름                   # Jump (is_call=true, 서브루틴)
    $ 변수 = 값                     # SetVar (true/false/정수/실수/"문자열")
    if 변수 op 값 -> 참 else 거짓    # Condition (==, !=, >, <, >=, <=)
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
| `SetVar` | 변수 설정 — var_name_id, ValueData union |
| `Condition` | 조건 분기 — var, op, compare_value, true/false jumps |

## Compiler Warnings

```
-Wall -pedantic -Wextra -Werror=unused-parameter -Wold-style-cast
-Wnon-virtual-dtor -Wzero-as-null-pointer-constant -faligned-new -Wextra-semi
```

## Dependencies

- **FlatBuffers** v24.3.25 (auto-fetched via CMake FetchContent)
- **godot-cpp** 4.3 branch (git submodule at `bindings/godot_extension/godot-cpp`)

## Testing

No test framework integrated yet. 전체 파이프라인 테스트:
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
