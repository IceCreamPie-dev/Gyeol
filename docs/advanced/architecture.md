# 아키텍처

## 개요

Gyeol은 명확한 파이프라인을 가진 미들웨어 엔진입니다:

```
  JSON IR (story.json)
       |
  [JsonIrReader] ─ gyeol_json_ir_reader.cpp
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

## Ren'Py/Ink/Yarn 대비 선택 기준

아래 표는 "무엇이 더 좋다"가 아니라, 어떤 팀/요구사항에 어떤 선택이 맞는지 정리한 기준입니다.

| 비교 대상 | 중심 저작 모델 | Gyeol과의 구조 차이 | 이런 경우 Gyeol 선택 |
|------|------|------|------|
| Ren'Py | 엔진 종속 스크립트 + 엔진 기능 결합 | Gyeol은 엔진 독립 `JSON IR` + `Runner` VM으로 계약을 고정 | 엔진 교체/멀티 런타임(WASM, Godot 등)에서 동일 실행 규약을 유지해야 할 때 |
| Ink | DSL 중심 저작 + 스토리 흐름 표현 최적화 | Gyeol은 canonical IR 기반으로 `graph patch` 운영과 구조 편집 자동화가 쉬움 | 툴링/자동화 파이프라인에서 "텍스트 DSL"보다 "안정 JSON 계약"이 중요한 팀 |
| Yarn | 대화 시스템 중심 노드 저작 | Gyeol은 타입드 `Command` 인자, `locale catalog` 핫스위치, runtime contract를 함께 고정 | 대화 외에 엔진 명령 계약/로컬라이제이션 fallback까지 하나의 데이터 계약으로 묶고 싶을 때 |

요약하면, Gyeol은 "저작 문법의 간결함"보다 "실행 계약의 이식성/검증 가능성"이 우선인 프로젝트에서 강점을 가집니다.

## 핵심 컴포넌트

### JSON IR 리더 (JsonIrReader, `gyeol_json_ir_reader.cpp`)

`JsonIrReader`는 현재 기본 입력 어댑터입니다:

- **입력:** canonical JSON IR (`format: "gyeol-json-ir"`, `format_version: 2`)
- **출력:** FlatBuffers Object API 타입 (`StoryT`, `NodeT`, `InstructionT`)
- **역할:** `.gyb` 패킹 전 엄격 검증 + 역직렬화

### 레거시 Parser (`gyeol_parser.cpp`)

텍스트 Parser는 레거시/비공개 경로용으로 유지됩니다:

- 줄 단위, 들여쓰기 기반 처리
- 에러 복구(다중 에러 수집)
- 멀티 파일 import 병합 및 표현식 파싱

### 문자열 풀 (String Pool)

컴파일된 바이너리의 모든 텍스트는 중앙 String Pool에 저장됩니다:

```
string_pool: ["hero", "Hello!", "cave", "Go left", ...]
```

다른 구조체는 이 풀에 대한 정수 인덱스를 저장합니다:

```
Line { character_id: 0, text_id: 1 }  // hero가 "Hello!"를 말함
Choice { text_id: 3, target_node_name_id: 2 }  // "Go left" -> cave
```

장점:
- 컴파일 중 `unordered_map`을 통한 **중복 제거**
- 동일 인덱스에 번역 문자열을 오버레이하는 **로컬라이제이션**
- FlatBuffers zero-copy 접근을 통한 **메모리 효율**

### 실행 가상 머신 (Runner VM, `gyeol_runner.cpp`)

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

### 바이너리 포맷 계층 (FlatBuffers)

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
  |── 시그널:
  |     dialogue_line(character, text, tags)
  |     choices_presented(choices)
  |     command_received(type, args)
  |     story_ended()
```

주요 설계:
- 소스 파일이 확장 DLL에 직접 컴파일됨 (라이브러리로 링크하지 않음)
- FlatBuffers 헤더는 CMake 빌드 디렉토리에서 참조
- Godot 경로(`res://`, `user://`)는 `FileAccess`를 통해 시스템 경로로 변환
- 모든 Gyeol 타입은 Godot Variant로 매핑 (Int->int, String->String, List->Array)

### 외부 그래프 편집 계약 (v1 + v2)

Gyeol은 외부 Rust/Node 툴을 위한 구조 편집 계약을 제공합니다.

- `--export-graph-json`: `gyeol-graph-doc` 출력 (`format: "gyeol-graph-doc"`, `version: 1`)
- `--validate-graph-patch`: `gyeol-graph-patch` (`version: 1` 또는 `2`) 검증
- `--apply-graph-patch`: 패치를 원자적으로 적용한 뒤 canonical JSON IR 재출력
- `--apply-graph-patch --preserve-line-id`: `*.lineidmap.json` 사이드카 생성
- `--line-id-map <path>`: 재컴파일 시 line_id 복원

v1 지원 연산:

- `add_node`
- `rename_node` (모든 참조 자동 갱신)
- `delete_node` (`redirect_target` 필수)
- `retarget_edge` (stable `edge_id`)
- `set_start_node`

v2 연산:

- `update_line_text`
- `update_choice_text`
- `update_command`
- `update_expression`
- `insert_instruction`
- `delete_instruction`
- `move_instruction`

v2의 `instruction_id`는 patch 시작 스냅샷 기준으로 해석되며, 같은 patch에서 삽입된 instruction은 `instruction_id`로 재참조할 수 없습니다.

레이아웃 메타데이터는 Runner에서 읽지 않으며, `*.graph.layout.json` 같은 사이드카 파일로 관리합니다.

실행/이벤트/상태 보장 규약은 [Runtime Contract v1.1](runtime-contract.md) 문서에서 버전 고정합니다.

## 데이터 흐름

### 컴파일

```
story.json (JSON IR)
    |
  JsonIrReader::fromFile()
    |
  StoryT (Object API)
    |         \
  JsonIrReader::compileToBuffer()    LocaleTools export/convert
    |                      |
  .gyb binary          locale 산출물 (POT/locale JSON/catalog)
```

### 런타임

```
.gyb binary                  locale catalog (v2)
    |                            |
  Runner.start()            Runner.loadLocaleCatalog() + setLocale()
    |                            |
  [Zero-copy read]          [fallback overlay pool]
    |                            |
  Runner.step()  ←──── pool/character 조회 시 오버레이 우선
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

### CMake 타깃

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
