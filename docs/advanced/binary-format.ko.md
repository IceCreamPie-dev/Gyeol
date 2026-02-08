# 바이너리 형식

Gyeol은 바이너리 형식에 [FlatBuffers](https://google.github.io/flatbuffers/)를 사용합니다.

## 파일 타입

| 확장자 | 형식 | 루트 타입 | 설명 |
|-----------|--------|-----------|-------------|
| `.gyb` | Gyeol Binary | `Story` | 컴파일된 스토리 데이터 |
| `.gys` | Gyeol Save | `SaveState` | 저장된 게임 상태 |

두 형식 모두 빠른 로딩을 위한 zero-copy FlatBuffers 바이너리입니다.

## 스키마 위치

```
schemas/gyeol.fbs
```

스키마는 `.gyb`와 `.gys` 파일의 모든 데이터 타입을 정의합니다. FlatBuffers C++ 헤더는 CMake 빌드 중에 자동 생성됩니다.

## 스토리 형식 (.gyb)

루트 타입: `Story`

```
Story
  version: string                 # Schema version
  string_pool: [string]           # All unique text strings
  line_ids: [string]              # Translation Line IDs (parallel to string_pool)
  global_vars: [SetVar]           # Initial variable declarations
  nodes: [Node]                   # Story nodes
  start_node_name: string         # Entry point node name
  characters: [CharacterDef]      # Character definitions
```

### String Pool

스토리의 모든 텍스트는 중앙 String Pool에 저장됩니다. 다른 구조체는 이 풀에 대한 정수 인덱스로 텍스트를 참조합니다. 장점:
- **중복 제거** - 동일한 문자열은 한 번만 저장
- **로컬라이제이션** - 로케일 오버레이가 인덱스 기반으로 문자열 대체
- **메모리 효율** - FlatBuffers zero-copy 접근

### Node

```
Node
  name: string (key)              # Node name (unique identifier)
  lines: [Instruction]            # Instructions to execute
  param_ids: [int]                # Parameter name indices (for functions)
  tags: [Tag]                     # Metadata tags (#key=value)
```

### 인스트럭션 타입 (OpData union)

| 타입 | 필드 | 설명 |
|------|--------|-------------|
| `Line` | character_id, text_id, voice_asset_id, tags | 대사 또는 나레이션 |
| `Choice` | text_id, target_node_name_id, condition_var_id, choice_modifier | 메뉴 선택지 |
| `Jump` | target_node_name_id, is_call, arg_exprs | 흐름 이동 |
| `Command` | type_id, params | 엔진 명령 |
| `SetVar` | var_name_id, value, expr, assign_op | 변수 할당 |
| `Condition` | var_name_id, op, compare_value, true/false jumps, expressions | 조건 분기 |
| `Random` | branches | 가중치 랜덤 선택 |
| `Return` | expr, value | 함수 반환 |
| `CallWithReturn` | target_node_name_id, return_var_name_id, arg_exprs | 호출 + 결과 저장 |

### 값 타입 (ValueData union)

| 타입 | 테이블 | 필드 |
|------|-------|-------|
| Bool | `BoolValue` | `val: bool` |
| Int | `IntValue` | `val: int` |
| Float | `FloatValue` | `val: float` |
| String | `StringRef` | `index: int` (String Pool 인덱스) |
| List | `ListValue` | `items: [int]` (String Pool 인덱스) |

### 표현식 시스템

표현식은 RPN (역폴란드 표기법) 토큰 리스트로 저장됩니다:

```
Expression
  tokens: [ExprToken]

ExprToken
  op: ExprOp                      # Operation type
  literal_value: ValueData        # For PushLiteral
  var_name_id: int                # For PushVar, PushVisitCount, etc.
```

ExprOp 타입:

| 카테고리 | 연산 |
|----------|-----------|
| 스택 | `PushLiteral`, `PushVar` |
| 산술 | `Add`, `Sub`, `Mul`, `Div`, `Mod`, `Negate` |
| 비교 | `CmpEq`, `CmpNe`, `CmpGt`, `CmpLt`, `CmpGe`, `CmpLe` |
| 논리 | `And`, `Or`, `Not` |
| 함수 | `PushVisitCount`, `PushVisited` |
| 리스트 | `ListContains`, `ListLength` |

### Tag

```
Tag
  key_id: int                     # String pool index for key
  value_id: int                   # String pool index for value
```

대사 태그(`#mood:angry`)와 노드 태그(`#difficulty=hard`) 모두에 사용됩니다.

### 캐릭터 정의

```
CharacterDef
  name_id: int                    # Character ID (string pool index)
  properties: [Tag]               # Key-value properties
```

### 선택지 수식어

```
enum ChoiceModifier : byte {
    Default = 0,
    Once = 1,
    Sticky = 2,
    Fallback = 3
}
```

## 세이브 형식 (.gys)

루트 타입: `SaveState`

```
SaveState
  version: string                 # Save format version
  story_version: string           # Original story version
  current_node_name: string       # Active node
  pc: uint32                      # Program counter
  finished: bool                  # End flag
  variables: [SavedVar]           # Runtime variables
  call_stack: [SavedCallFrame]    # Function call frames
  pending_choices: [SavedPendingChoice]  # Active menu
  visit_counts: [SavedVisitCount]        # Visit data
  chosen_once_choices: [string]          # Once-choice keys
```

### SavedVar

```
SavedVar
  name: string                    # Variable name
  value: ValueData                # Type + value
  string_value: string            # String type actual value
  list_items: [string]            # List type items
```

### SavedCallFrame

```
SavedCallFrame
  node_name: string               # Frame's node
  pc: uint32                      # Return address
  return_var_name: string         # Variable for return value
  shadowed_vars: [SavedShadowedVar]  # Saved parameter overrides
  param_names: [string]           # Parameter names
```

### SavedPendingChoice

```
SavedPendingChoice
  text: string                    # Choice display text
  target_node_name: string        # Jump target
  choice_modifier: ChoiceModifier # Modifier type
```

### SavedVisitCount

```
SavedVisitCount
  node_name: string               # Node name
  count: uint32                   # Visit count
```

## 스키마에서 빌드

FlatBuffers 헤더는 CMake 빌드 중 FetchContent를 통해 자동으로 생성됩니다. 수동으로 재생성하려면:

```bash
flatc --cpp schemas/gyeol.fbs -o src/gyeol_core/include/generated/
```

## FlatBuffers Object API

코드베이스는 두 가지 API를 사용합니다:

| API | 용도 | 패턴 |
|-----|-------|---------|
| **Object API** (`*T` 타입) | 빌드/쓰기 | `StoryT`, `NodeT`, `InstructionT` |
| **Zero-copy API** | 읽기 | `GetRoot<Story>()`, 포인터 접근 |

Parser는 Object API 타입으로 빌드한 후 `Pack()`을 호출하여 직렬화합니다. Runner는 최대 성능을 위해 zero-copy 포인터로 읽습니다.
