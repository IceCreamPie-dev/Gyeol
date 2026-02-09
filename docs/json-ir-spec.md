# Gyeol JSON IR Specification

> Version: 1 (`format_version: 1`)

Gyeol 컴파일러가 출력하는 JSON 중간 표현(Intermediate Representation) 포맷 스펙.
`.gyb` FlatBuffers 바이너리와 1:1 대응하며, 모든 String Pool 인덱스가 실제 문자열로 해석되어 있어 외부 도구에서 바로 사용 가능.

## 사용법

### CLI

```bash
# JSON IR 출력
GyeolCompiler story.gyeol --format json -o story.json

# 기본 바이너리 출력 (기존 동작)
GyeolCompiler story.gyeol -o story.gyb

# 최적화 + JSON
GyeolCompiler story.gyeol -O --format json -o story.json
```

### WASM (브라우저)

```javascript
const engine = new GyeolEngine();
const result = engine.compileToJson(sourceCode);
if (result.success) {
    const story = JSON.parse(result.json);
    // story.nodes, story.characters, etc.
}
```

### C++ API

```cpp
#include "gyeol_json_export.h"

Gyeol::Parser parser;
parser.parse("story.gyeol");

// JSON object (nlohmann::json)
auto j = Gyeol::JsonExport::toJson(parser.getStory());

// Pretty-printed string
std::string jsonStr = Gyeol::JsonExport::toJsonString(parser.getStory());
```

---

## 루트 구조

```json
{
    "format": "gyeol-json-ir",
    "format_version": 1,
    "version": "0.2.0",
    "start_node_name": "start",
    "string_pool": ["start", "hero", "Hello", ...],
    "line_ids": ["start:0:a1b2", "", ...],
    "characters": [...],
    "global_vars": [...],
    "nodes": [...]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `format` | `string` | 항상 `"gyeol-json-ir"` |
| `format_version` | `int` | 포맷 버전 (현재 `1`) |
| `version` | `string` | 스토리 버전 (컴파일러 버전) |
| `start_node_name` | `string` | 시작 노드 이름 |
| `string_pool` | `string[]` | 전체 문자열 풀 (인덱스 참조가 필요한 도구용) |
| `line_ids` | `string[]` | 번역용 Line ID (빈 문자열 = 구조적, 비어있으면 생략) |
| `characters` | `CharacterDef[]` | 캐릭터 정의 목록 (없으면 생략) |
| `global_vars` | `SetVar[]` | 전역 변수 초기값 (없으면 생략) |
| `nodes` | `Node[]` | 스토리 노드 배열 |

---

## Node

```json
{
    "name": "start",
    "params": ["a", "b"],
    "tags": [
        {"key": "repeatable", "value": ""},
        {"key": "difficulty", "value": "hard"}
    ],
    "instructions": [...]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `name` | `string` | 노드 이름 |
| `params` | `string[]` | 함수 매개변수 이름 (없으면 생략) |
| `tags` | `Tag[]` | 노드 메타데이터 태그 (없으면 생략) |
| `instructions` | `Instruction[]` | 명령어 배열 |

---

## Instruction 타입

모든 명령어는 `"type"` 필드로 구분.

### Line (대사)

```json
{
    "type": "Line",
    "character": "hero",
    "text": "Hello {name}!",
    "voice_asset": "hero_greeting.wav",
    "tags": [
        {"key": "mood", "value": "happy"},
        {"key": "pose", "value": "standing"}
    ]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Line"` | |
| `character` | `string \| null` | 캐릭터 이름 (`null` = 나레이션) |
| `text` | `string` | 대사 텍스트 (`{변수}` 보간 포함 가능) |
| `voice_asset` | `string` | 보이스 에셋 경로 (없으면 생략) |
| `tags` | `Tag[]` | 메타데이터 태그 (없으면 생략) |

### Choice (선택지)

```json
{
    "type": "Choice",
    "text": "Go left",
    "target_node": "left_path",
    "condition_var": "has_key",
    "choice_modifier": "Once"
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Choice"` | |
| `text` | `string` | 선택지 텍스트 |
| `target_node` | `string` | 선택 시 이동할 노드 |
| `condition_var` | `string` | 조건 변수 (없으면 생략, truthy일 때만 표시) |
| `choice_modifier` | `string` | `"Once"`, `"Sticky"`, `"Fallback"` (Default면 생략) |

### Jump (이동)

```json
{
    "type": "Jump",
    "target_node": "next_scene",
    "is_call": false,
    "arg_exprs": [...]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Jump"` | |
| `target_node` | `string` | 이동할 노드 |
| `is_call` | `bool` | `true` = 서브루틴 호출 (콜스택 push) |
| `arg_exprs` | `Expression[]` | 함수 호출 인자 표현식 (없으면 생략) |

### Command (엔진 명령)

```json
{
    "type": "Command",
    "command_type": "bg",
    "params": ["forest.png"]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Command"` | |
| `command_type` | `string` | 명령 종류 (`bg`, `sfx`, `music`, 등 자유 정의) |
| `params` | `string[]` | 파라미터 배열 |

> **참고**: `@` 명령어는 사용자 정의. Gyeol이 강제하는 목록은 없으며, 게임 엔진에서 자유롭게 해석.

### SetVar (변수 설정)

```json
{
    "type": "SetVar",
    "var_name": "hp",
    "assign_op": "Assign",
    "value": {"type": "Int", "val": 100},
    "expr": null
}
```

표현식이 있는 경우:

```json
{
    "type": "SetVar",
    "var_name": "hp",
    "assign_op": "Assign",
    "value": null,
    "expr": {
        "tokens": [
            {"op": "PushVar", "var_name": "hp"},
            {"op": "PushLiteral", "value": {"type": "Int", "val": 5}},
            {"op": "Add"}
        ]
    }
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"SetVar"` | |
| `var_name` | `string` | 변수 이름 |
| `assign_op` | `string` | `"Assign"` (=), `"Append"` (+=), `"Remove"` (-=) |
| `value` | `Value \| null` | 리터럴 값 (`expr`이 있으면 `null`) |
| `expr` | `Expression \| null` | 복합 표현식 (`value`보다 우선) |

### Condition (조건 분기)

단순 비교:

```json
{
    "type": "Condition",
    "op": "Greater",
    "var_name": "hp",
    "compare_value": {"type": "Int", "val": 0},
    "true_jump_node": "alive",
    "false_jump_node": "dead"
}
```

표현식 조건:

```json
{
    "type": "Condition",
    "op": "Greater",
    "lhs_expr": {"tokens": [...]},
    "rhs_expr": {"tokens": [...]},
    "true_jump_node": "alive",
    "false_jump_node": "dead"
}
```

논리 연산자 (전체 boolean 표현식):

```json
{
    "type": "Condition",
    "op": "Equal",
    "cond_expr": {
        "tokens": [
            {"op": "PushVar", "var_name": "hp"},
            {"op": "PushLiteral", "value": {"type": "Int", "val": 0}},
            {"op": "CmpGt"},
            {"op": "PushVar", "var_name": "has_key"},
            {"op": "And"}
        ]
    },
    "true_jump_node": "pass",
    "false_jump_node": "fail"
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Condition"` | |
| `op` | `string` | 비교 연산자 (하위 호환, `cond_expr` 있으면 무시) |
| `var_name` | `string` | 단순 비교 좌변 변수 (하위 호환) |
| `compare_value` | `Value` | 단순 비교 우변 값 (하위 호환) |
| `lhs_expr` | `Expression` | 좌변 표현식 |
| `rhs_expr` | `Expression` | 우변 표현식 |
| `cond_expr` | `Expression` | 전체 boolean 표현식 (있으면 위 필드 무시) |
| `true_jump_node` | `string` | 참일 때 이동할 노드 |
| `false_jump_node` | `string` | 거짓일 때 이동할 노드 |

### Random (랜덤 분기)

```json
{
    "type": "Random",
    "branches": [
        {"target_node": "path_a", "weight": 50},
        {"target_node": "path_b", "weight": 30},
        {"target_node": "path_c", "weight": 1}
    ]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Random"` | |
| `branches` | `RandomBranch[]` | 가중치 기반 분기 (확률 = weight / 총합) |

### Return (반환)

```json
{
    "type": "Return",
    "expr": {"tokens": [...]},
    "value": {"type": "Int", "val": 42}
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"Return"` | |
| `expr` | `Expression \| null` | 반환 표현식 (있으면 `value`보다 우선) |
| `value` | `Value` | 단순 리터럴 반환값 |

### CallWithReturn (반환값 저장 호출)

```json
{
    "type": "CallWithReturn",
    "target_node": "calculate",
    "return_var": "result",
    "arg_exprs": [
        {"tokens": [{"op": "PushLiteral", "value": {"type": "Int", "val": 1}}]},
        {"tokens": [{"op": "PushLiteral", "value": {"type": "Int", "val": 2}}]}
    ]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `type` | `"CallWithReturn"` | |
| `target_node` | `string` | 호출할 노드 |
| `return_var` | `string` | 반환값을 저장할 변수 이름 |
| `arg_exprs` | `Expression[]` | 함수 호출 인자 (없으면 생략) |

---

## 공통 타입

### Value (값)

```json
{"type": "Bool", "val": true}
{"type": "Int", "val": 42}
{"type": "Float", "val": 3.14}
{"type": "String", "val": "hello"}
{"type": "List", "val": ["item1", "item2"]}
```

| type | val 타입 | 설명 |
|------|----------|------|
| `"Bool"` | `boolean` | 참/거짓 |
| `"Int"` | `integer` | 정수 |
| `"Float"` | `number` | 실수 |
| `"String"` | `string` | 문자열 (String Pool에서 해석됨) |
| `"List"` | `string[]` | 문자열 리스트 |

### Expression (표현식)

RPN(역폴란드 표기법) 토큰 리스트. Shunting-yard 알고리즘으로 변환된 중위 표현식.

```json
{
    "tokens": [
        {"op": "PushVar", "var_name": "hp"},
        {"op": "PushLiteral", "value": {"type": "Int", "val": 10}},
        {"op": "Sub"},
        {"op": "PushLiteral", "value": {"type": "Int", "val": 0}},
        {"op": "CmpGt"}
    ]
}
```

### ExprToken (표현식 토큰)

| 필드 | 타입 | 설명 |
|------|------|------|
| `op` | `string` | 연산자 (아래 목록) |
| `value` | `Value` | `PushLiteral`일 때 리터럴 값 |
| `var_name` | `string` | `PushVar`, `PushVisitCount`, `PushVisited`, `ListLength`일 때 변수/노드 이름 |

### ExprOp (연산자 목록)

| 연산자 | 스택 동작 | 설명 |
|--------|-----------|------|
| `PushLiteral` | → val | 리터럴 값 push |
| `PushVar` | → val | 변수 값 push |
| `Add` | a, b → a+b | 덧셈 |
| `Sub` | a, b → a-b | 뺄셈 |
| `Mul` | a, b → a*b | 곱셈 |
| `Div` | a, b → a/b | 나눗셈 |
| `Mod` | a, b → a%b | 나머지 |
| `Negate` | a → -a | 부호 반전 |
| `CmpEq` | a, b → a==b | 같음 |
| `CmpNe` | a, b → a!=b | 다름 |
| `CmpGt` | a, b → a>b | 초과 |
| `CmpLt` | a, b → a<b | 미만 |
| `CmpGe` | a, b → a>=b | 이상 |
| `CmpLe` | a, b → a<=b | 이하 |
| `And` | a, b → a&&b | 논리 AND |
| `Or` | a, b → a\|\|b | 논리 OR |
| `Not` | a → !a | 논리 NOT |
| `PushVisitCount` | → int | 노드 방문 횟수 (`var_name` = 노드명) |
| `PushVisited` | → bool | 노드 방문 여부 (`var_name` = 노드명) |
| `ListContains` | list, str → bool | 리스트 포함 여부 |
| `ListLength` | → int | 리스트 크기 (`var_name` = 변수명) |

### Tag (메타데이터)

```json
{"key": "mood", "value": "happy"}
{"key": "repeatable", "value": ""}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `key` | `string` | 태그 키 |
| `value` | `string` | 태그 값 (boolean 플래그면 빈 문자열) |

### CharacterDef (캐릭터 정의)

```json
{
    "name": "hero",
    "properties": [
        {"key": "name", "value": "The Hero"},
        {"key": "color", "value": "#FF0000"},
        {"key": "voice", "value": "hero_voice"}
    ]
}
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `name` | `string` | 캐릭터 식별자 |
| `properties` | `Tag[]` | key-value 속성 (name, color, voice 등 자유 정의) |

---

## 비교 연산자 (Operator)

| 값 | 의미 |
|----|------|
| `"Equal"` | `==` |
| `"NotEqual"` | `!=` |
| `"Greater"` | `>` |
| `"Less"` | `<` |
| `"GreaterOrEqual"` | `>=` |
| `"LessOrEqual"` | `<=` |

## 할당 연산자 (AssignOp)

| 값 | 의미 |
|----|------|
| `"Assign"` | `=` (기본) |
| `"Append"` | `+=` (리스트에 추가) |
| `"Remove"` | `-=` (리스트에서 제거) |

## 선택지 수식어 (ChoiceModifier)

| 값 | 의미 |
|----|------|
| (생략) | Default — 조건 통과 시 항상 표시 |
| `"Once"` | 한번 선택 후 다시 표시 안 됨 |
| `"Sticky"` | 항상 표시 (Default와 동일, 의도 명시) |
| `"Fallback"` | 다른 모든 비-fallback 선택지가 숨겨졌을 때만 표시 |

---

## 런타임 동작 참고

### 문자열 보간

`text` 필드의 `{변수명}` 패턴은 런타임에서 변수 값으로 치환됨.

```
"Hello {name}!"  →  "Hello Alice!"  (name = "Alice")
```

### 인라인 조건 텍스트

`text` 필드의 `{if 조건}참{else}거짓{endif}` 패턴은 런타임에서 조건 평가.

```
"{if hp > 50}Strong{else}Weak{endif} warrior"
```

### 매개변수 바인딩

`label func(a, b):` + `call func(1, x+2)` 사용 시:
- 호출 시점에 `arg_exprs`의 각 표현식을 평가
- 결과를 `params` 이름에 바인딩 (기존 변수는 섀도잉)
- return/노드 끝에서 섀도된 변수 복원
- 인자 부족 시 `Int(0)` 기본값, 초과 인자 무시

### Visit Count

- `PushVisitCount`/`PushVisited`의 `var_name`은 노드 이름
- 노드 진입 시 자동 카운트 증가 (start/jump/call/choose 모두)
- 따옴표/맨문자 모두 허용: `visit_count("shop")` = `visit_count(shop)`

---

## 전체 예제

입력 (.gyeol):

```
character npc:
    name: "NPC"
    color: "#00FF00"

$ score = 0

label start:
    npc "Welcome!" #mood:friendly
    $ score = score + 10
    menu:
        "Fight" -> battle
        "Talk" -> dialogue

label battle:
    @ sfx sword.wav
    "You fight"
    jump ending

label dialogue:
    npc "Let's talk"
    jump ending

label ending:
    "The end"
```

출력 (JSON IR, 핵심 부분 발췌):

```json
{
    "format": "gyeol-json-ir",
    "format_version": 1,
    "version": "",
    "start_node_name": "start",
    "string_pool": ["score", "0", "start", "npc", "Welcome!", ...],
    "characters": [
        {
            "name": "npc",
            "properties": [
                {"key": "name", "value": "NPC"},
                {"key": "color", "value": "#00FF00"}
            ]
        }
    ],
    "global_vars": [
        {
            "type": "SetVar",
            "var_name": "score",
            "assign_op": "Assign",
            "value": {"type": "Int", "val": 0},
            "expr": null
        }
    ],
    "nodes": [
        {
            "name": "start",
            "instructions": [
                {
                    "type": "Line",
                    "character": "npc",
                    "text": "Welcome!",
                    "tags": [{"key": "mood", "value": "friendly"}]
                },
                {
                    "type": "SetVar",
                    "var_name": "score",
                    "assign_op": "Assign",
                    "value": null,
                    "expr": {
                        "tokens": [
                            {"op": "PushVar", "var_name": "score"},
                            {"op": "PushLiteral", "value": {"type": "Int", "val": 10}},
                            {"op": "Add"}
                        ]
                    }
                },
                {
                    "type": "Choice",
                    "text": "Fight",
                    "target_node": "battle"
                },
                {
                    "type": "Choice",
                    "text": "Talk",
                    "target_node": "dialogue"
                }
            ]
        },
        {
            "name": "battle",
            "instructions": [
                {
                    "type": "Command",
                    "command_type": "sfx",
                    "params": ["sword.wav"]
                },
                {
                    "type": "Line",
                    "character": null,
                    "text": "You fight"
                },
                {
                    "type": "Jump",
                    "target_node": "ending",
                    "is_call": false
                }
            ]
        }
    ]
}
```
