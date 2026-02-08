# 아키텍처

## 개요

Gyeol은 명확한 파이프라인을 가진 미들웨어 엔진입니다:

```
  .gyeol Script
       |
  [Parser] ──── gyeol_parser.cpp
       |
  FlatBuffers Object API (StoryT, NodeT, ...)
       |
  [Compiler] ── Pack() → .gyb binary
       |
  [Runner VM] ─ gyeol_runner.cpp (zero-copy read)
       |
  [Binding] ─── GDExtension / Unity / WASM
       |
  Game Engine
```

## 핵심 컴포넌트

### Parser (`gyeol_parser.cpp`)

Parser는 줄 단위, 들여쓰기 기반 텍스트 처리기입니다:

- **입력:** `.gyeol` 텍스트 파일
- **출력:** FlatBuffers Object API 타입 (`StoryT`, `NodeT`, `InstructionT`)
- **방식:** 단일 패스 + 사후 검증

주요 설계 결정:
- 중괄호 기반이 아닌 **들여쓰기 기반** (0/4/8 스페이스)
- **에러 복구** - 첫 에러에서 멈추지 않고 모든 에러 수집
- **멀티 파일 지원** - `import`로 순환 감지와 함께 파일 병합
- **표현식 파싱** - Shunting-yard 알고리즘으로 중위 표기를 RPN으로 변환

### String Pool

컴파일된 바이너리의 모든 텍스트는 중앙 String Pool에 저장됩니다:

```
string_pool: ["hero", "Hello!", "cave", "Go left", ...]
```

다른 구조체는 이 풀에 대한 정수 인덱스를 저장합니다:

```
Line { character_id: 0, text_id: 1 }  // hero says "Hello!"
Choice { text_id: 3, target_node_name_id: 2 }  // "Go left" -> cave
```

장점:
- 컴파일 중 `unordered_map`을 통한 **중복 제거**
- 동일 인덱스에 번역 문자열을 오버레이하는 **로컬라이제이션**
- FlatBuffers zero-copy 접근을 통한 **메모리 효율**

### Runner VM (`gyeol_runner.cpp`)

Runner는 컴파일된 스토리 데이터를 해석하는 가상 머신입니다:

- **입력:** `.gyb` FlatBuffers 바이너리 (zero-copy 읽기)
- **API:** 이벤트 기반 `step()` / `choose()`
- **상태:** 프로그램 카운터, 변수, 콜 스택, 방문 횟수

#### 실행 모델

```
           step()
             |
     [Read Instruction]
             |
    +--------+--------+--------+--------+
    |        |        |        |        |
   LINE   CHOICE   COMMAND    JUMP    OTHER
    |        |        |        |     (SetVar, Condition,
  emit    collect   emit    jump     Random, Return, ...)
  signal   choices  signal  to node    |
    |        |        |        |    [internal,
    |     emit       |        |     continue]
    |     signal     |        |
    v        v        v        v
  StepResult returned to caller
```

#### StepResult 타입

| 타입 | 의미 | 호출자 동작 |
|------|---------|---------------|
| `LINE` | 표시할 대사/나레이션 | 텍스트 표시 후 `step()` 호출 |
| `CHOICES` | 메뉴 표시 | 버튼 표시 후 `choose(index)` 호출 |
| `COMMAND` | 엔진 명령 | 명령 처리 후 `step()` 호출 |
| `END` | 스토리 종료 | 정지 |

내부 인스트럭션(SetVar, Jump, Condition, Random)은 조용히 실행되고 다음 가시 인스트럭션으로 계속 진행됩니다.

#### 선택지 필터링 파이프라인

메뉴를 만나면, 선택지는 다음 파이프라인을 통해 필터링됩니다:

```
Raw choices from bytecode
    |
[1. Condition filter] ── hide choices whose condition var is false
    |
[2. Once filter] ────── hide choices with once_key in chosenOnceChoices_
    |
[3. Modifier split] ─── separate into normal vs fallback
    |
[4. Fallback logic] ─── if normal.empty(), use fallback
    |
Final choices → StepResult
```

#### 표현식 평가

표현식은 스택 기반 RPN 평가기를 사용합니다:

```
Input:  hp * 2 + bonus
RPN:    [PushVar(hp), PushLiteral(2), Mul, PushVar(bonus), Add]

Stack trace:
  Push hp(100)     → [100]
  Push 2           → [100, 2]
  Mul              → [200]
  Push bonus(10)   → [200, 10]
  Add              → [210]
  Result: 210
```

#### 콜 스택

함수 호출은 스코프 관리를 포함하는 콜 스택을 사용합니다:

```
CallFrame
  node: pointer to called node
  pc: return address (instruction after the call)
  returnVarName: variable for return value
  shadowedVars: saved global values overridden by parameters
  paramNames: parameter names for cleanup
```

함수가 반환할 때:
1. 콜 프레임을 팝
2. 섀도된 변수를 호출 전 값으로 복원
3. `returnVarName`이 설정되어 있으면 반환값 저장
4. 호출자 노드의 저장된 `pc`에서 재개

### FlatBuffers

Gyeol은 스토리 데이터와 세이브 파일 모두에 FlatBuffers를 사용합니다:

| API | 사용 시점 | 타입 |
|-----|------|-------|
| **Object API** | 빌드/쓰기 | `StoryT`, `NodeT`, `ChoiceT` 등 |
| **Zero-copy API** | 읽기/실행 | `Story*`, `Node*`, `GetRoot<>()` 사용 |

Object API는 생성을 위해 `T` 접미사 타입(뮤터블 구조체)을 사용합니다. Zero-copy API는 역직렬화 없이 바이너리 버퍼에서 직접 읽습니다.

### GDExtension 바인딩

Godot 바인딩은 C++ Runner를 `StoryPlayer` 노드로 래핑합니다:

```
StoryPlayer (Node)
  |
  |── Runner runner_
  |── vector<uint8_t> buffer_
  |
  |── load_story() ──→ FileAccess::open → buffer_
  |── start()      ──→ runner_.start()
  |── advance()    ──→ runner_.step() → emit_signal()
  |── choose()     ──→ runner_.choose() → advance()
  |
  |── Signals:
  |     dialogue_line(character, text, tags)
  |     choices_presented(choices)
  |     command_received(type, params)
  |     story_ended()
```

주요 설계:
- 소스 파일이 확장 DLL에 직접 컴파일됨 (라이브러리로 링크하지 않음)
- FlatBuffers 헤더는 CMake 빌드 디렉토리에서 참조
- Godot 경로(`res://`, `user://`)는 `FileAccess`를 통해 시스템 경로로 변환
- 모든 Gyeol 타입은 Godot Variant로 매핑 (Int->int, String->String, List->Array)

## 데이터 흐름

### 컴파일

```
.gyeol text
    |
  Parser.parse()
    |
  StoryT (Object API)
    |         \
  Parser.compile()    Parser.exportStrings()
    |                      |
  .gyb binary          strings.csv
```

### 런타임

```
.gyb binary                  .csv locale
    |                            |
  Runner.start()            Runner.loadLocale()
    |                            |
  [Zero-copy read]          [Overlay pool]
    |                            |
  Runner.step()  ←──── poolStr() checks overlay first
    |
  StepResult → Game Engine
```

### 저장/로드

```
Runner state ──→ SaveState (Object API) ──→ Pack() ──→ .gys binary
                                                           |
Runner state ←── Parse .gys ←── GetRoot<SaveState>() ←────┘
```

## 빌드 아키텍처

### CMake 타겟

```
GyeolCore ────── Core library (Story + Runner)
    |
GyeolParser ─── Parser library (shared by Compiler + Tests)
    |
    ├── GyeolCompiler ── CLI compiler
    ├── GyeolTest ────── Console player
    ├── GyeolTests ───── Unit tests
    ├── GyeolLSP ─────── LSP server
    └── GyeolDebugger ── CLI debugger

SCons (separate):
    libgyeol.dll ─────── GDExtension (includes GyeolCore sources directly)
```

### 의존성

```
FlatBuffers v24.3.25 ─── Binary serialization (FetchContent)
Google Test v1.14.0 ──── Unit testing (FetchContent)
nlohmann/json v3.11.3 ── LSP server only (FetchContent)
godot-cpp 4.3 ────────── GDExtension binding (git submodule)
```

## 설계 원칙

1. **엔진 독립적** - Runner는 어떤 게임 엔진에도 의존하지 않음
2. **이벤트 기반** - `step()`/`choose()` API, 프레임 기반이 아님
3. **Zero-copy** - 빠른 바이너리 로딩을 위한 FlatBuffers
4. **결정적** - 시드 설정 가능한 RNG, 재현 가능한 실행
5. **하위 호환** - 세이브 파일이 버전 간 호환
6. **에러 내성** - Parser가 첫 에러에서 멈추지 않고 모든 에러 수집
