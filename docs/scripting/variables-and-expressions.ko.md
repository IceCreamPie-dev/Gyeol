# 변수와 표현식

## 변수 타입

Gyeol은 다섯 가지 변수 타입을 지원합니다:

| 타입 | 예시 | 설명 |
|------|------|------|
| **Bool** | `true`, `false` | 불리언 |
| **Int** | `42`, `-10`, `0` | 32비트 부호 있는 정수 |
| **Float** | `3.14`, `-0.5` | 32비트 부동 소수점 |
| **String** | `"hello"` | UTF-8 문자열 |
| **List** | *(런타임 전용)* | 문자열 리스트 |

## 선언과 대입

### 단순 대입

```
$ hp = 100
$ name = "Hero"
$ is_alive = true
$ speed = 2.5
```

### 산술 표현식

```
$ hp = hp - 10
$ damage = attack * 2 + bonus
$ total = (a + b) * c
$ remainder = score % 10
```

### 지원되는 연산자

| 연산자 | 설명 | 타입 |
|--------|------|------|
| `+` | 덧셈 | Int, Float |
| `-` | 뺄셈 | Int, Float |
| `*` | 곱셈 | Int, Float |
| `/` | 나눗셈 | Int, Float |
| `%` | 나머지 | Int |
| `-` (단항) | 부정 | Int, Float |

연산자 우선순위는 표준 수학 규칙을 따릅니다: `*`, `/`, `%`가 `+`, `-`보다 먼저 계산됩니다. 괄호를 사용하여 우선순위를 변경할 수 있습니다.

### 타입 변환

Int와 Float를 혼합한 산술 연산 시:
- Int + Float = Float
- Int * Float = Float
- 두 Int의 나눗셈은 Int를 생성합니다 (정수 나눗셈)

## 전역 변수

첫 번째 `label` 앞에 선언된 변수는 전역 변수이며, 스토리 시작 시 초기화됩니다:

```
$ max_hp = 100
$ gold = 0
$ difficulty = "normal"

label start:
    hero "Starting with {gold} gold."
```

## 리스트 연산

리스트는 문자열 값에 `+=`와 `-=`를 사용하면 암묵적으로 생성됩니다:

```
$ inventory += "sword"       # inventory 리스트에 "sword" 추가
$ inventory += "potion"      # "potion" 추가
$ inventory -= "potion"      # 리스트에서 "potion" 제거
```

### 표현식에서의 리스트 함수

| 함수 | 반환 타입 | 설명 |
|------|-----------|------|
| `list_contains(var, "item")` | Bool | 리스트에 항목이 포함되어 있는지 확인 |
| `list_length(var)` | Int | 리스트 크기 반환 |

```
if list_contains(inventory, "key") -> has_key_path
$ count = list_length(inventory)
```

## 내장 함수

### 방문 추적

| 함수 | 반환 타입 | 설명 |
|------|-----------|------|
| `visit_count("node_name")` | Int | 노드에 진입한 횟수 |
| `visited("node_name")` | Bool | 노드를 방문한 적이 있는지 여부 |

```
$ times = visit_count("shop")
$ been_there = visited("cave")

# 따옴표가 있든 없든 모두 사용 가능
$ times = visit_count(shop)
```

함수는 표현식이 허용되는 모든 곳에서 사용할 수 있습니다:

```
# 대입에서
$ count = visit_count("shop")

# 조건문에서
if visit_count("shop") > 3 -> regular_customer
if visited("secret") -> unlock_bonus

# 산술 연산에서
$ bonus = visit_count("shop") * 10

# 문자열 보간에서
hero "You've visited the shop {visit_count(shop)} times."

# 인라인 조건에서
hero "{if visited(shop)}Welcome back!{else}First time here?{endif}"
```

## 문자열 보간

`{...}`를 사용하여 대사와 선택지 텍스트에 변수를 삽입할 수 있습니다:

```
hero "Hello, {name}! You have {gold} gold."
```

### 변수 치환

```
hero "HP: {hp}/{max_hp}"
hero "Level {level} warrior"
```

`{...}` 안의 변수 이름은 런타임에 현재 값으로 치환됩니다. 모든 타입은 자동으로 문자열로 변환됩니다:
- Bool: `"true"` 또는 `"false"`
- Int: 10진수
- Float: 소수점이 있는 10진수
- String: 그대로 출력
- List: 쉼표로 구분된 항목

### 인라인 조건

```
hero "{if hp > 50}You look strong!{else}You look weak.{endif}"
```

지원되는 조건 유형:
- **참/거짓 판정:** `{if variable}` - 변수가 참이면 (0이 아니거나 비어 있지 않으면) 참
- **비교:** `{if hp > 50}`, `{if name == "Hero"}`
- **함수:** `{if visited(cave)}`, `{if visit_count(shop) > 2}`

`{else}` 절은 선택 사항입니다:

```
hero "{if has_key}You have a key.{endif}"
```

인라인 조건 안에서 다른 보간을 중첩할 수 있습니다:

```
hero "{if gold > 0}You have {gold} coins.{else}You're broke!{endif}"
```

## 비교 연산자

`if` 조건문과 인라인 조건에서 사용됩니다:

| 연산자 | 설명 |
|--------|------|
| `==` | 같음 |
| `!=` | 같지 않음 |
| `>` | 큼 |
| `<` | 작음 |
| `>=` | 크거나 같음 |
| `<=` | 작거나 같음 |

## 논리 연산자

논리 연산자로 조건을 결합합니다:

```
if hp > 0 and has_weapon == true -> can_fight
if is_tired or is_hungry -> need_rest
if not is_dead -> still_alive
```

| 연산자 | 설명 |
|--------|------|
| `and` | 두 조건 모두 참이어야 함 |
| `or` | 조건 중 하나 이상 참이어야 함 |
| `not` | 조건을 부정 |

## 변수 API

Runner VM과 GDExtension은 변수 접근을 제공합니다:

### C++ (Runner)

```cpp
runner.setVariable("hp", Gyeol::Variant::Int(100));
Gyeol::Variant hp = runner.getVariable("hp");
bool exists = runner.hasVariable("hp");
auto names = runner.getVariableNames();
```

### GDScript (StoryPlayer)

```gdscript
story_player.set_variable("hp", 100)
var hp = story_player.get_variable("hp")
var exists = story_player.has_variable("hp")
var names = story_player.get_variable_names()
```
