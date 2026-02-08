# StoryPlayer

**상속:** [Node](https://docs.godotengine.org/en/stable/classes/class_node.html)

Godot 4.3에서 Gyeol 스토리를 재생하기 위한 GDExtension 노드입니다.

## 설명

`StoryPlayer`는 Godot에서 Gyeol 스토리를 실행하기 위한 주요 인터페이스입니다. 핵심 C++ Runner VM을 래핑하며, 대화, 선택지, 명령, 스토리 흐름을 위한 시그널 기반 API를 제공합니다.

씬에 `StoryPlayer` 노드를 추가하고, 컴파일된 `.gyb` 스토리 파일을 로드한 후, 시그널을 연결하고, `advance()`를 호출하여 스토리를 진행합니다.

## 튜토리얼

- [Godot 연동 가이드](../getting-started/godot-integration.md)
- [빠른 시작](../getting-started/quick-start.md)

---

## 시그널

### dialogue_line

```
dialogue_line(character: String, text: String, tags: Dictionary)
```

대사 라인을 만나면 발생합니다. `character`는 캐릭터 ID(나레이션의 경우 빈 문자열)입니다. `text`는 보간이 적용된 대사 텍스트입니다. `tags`는 `#key:value` 태그의 메타데이터를 포함합니다.

---

### choices_presented

```
choices_presented(choices: Array)
```

`menu:` 블록에 도달하면 발생합니다. `choices`는 각 선택지의 표시 텍스트를 담은 String Array입니다. [choose](#choose)를 호출하여 하나를 선택하고 계속 진행합니다.

---

### command_received

```
command_received(type: String, params: Array)
```

`@` 명령을 만나면 발생합니다. `type`은 명령 이름이고, `params`는 String Array입니다. 명령을 처리한 후 반드시 [advance](#advance)를 호출해야 합니다.

---

### story_ended

```
story_ended()
```

스토리가 끝에 도달하면 발생합니다.

---

## 메서드

### 핵심

| 반환 타입 | 메서드 |
|--------|--------|
| `bool` | [load_story](#load_story)`(path: String)` |
| `void` | [start](#start)`()` |
| `void` | [advance](#advance)`()` |
| `void` | [choose](#choose)`(index: int)` |
| `bool` | [is_finished](#is_finished)`()` |

### 저장 / 로드

| 반환 타입 | 메서드 |
|--------|--------|
| `bool` | [save_state](#save_state)`(path: String)` |
| `bool` | [load_state](#load_state)`(path: String)` |

### 변수

| 반환 타입 | 메서드 |
|--------|--------|
| `Variant` | [get_variable](#get_variable)`(name: String)` |
| `void` | [set_variable](#set_variable)`(name: String, value: Variant)` |
| `bool` | [has_variable](#has_variable)`(name: String)` |
| `PackedStringArray` | [get_variable_names](#get_variable_names)`()` |

### 로컬라이제이션

| 반환 타입 | 메서드 |
|--------|--------|
| `bool` | [load_locale](#load_locale)`(path: String)` |
| `void` | [clear_locale](#clear_locale)`()` |
| `String` | [get_locale](#get_locale)`()` |

### 방문 추적

| 반환 타입 | 메서드 |
|--------|--------|
| `int` | [get_visit_count](#get_visit_count)`(node_name: String)` |
| `bool` | [has_visited](#has_visited)`(node_name: String)` |

### 캐릭터

| 반환 타입 | 메서드 |
|--------|--------|
| `String` | [get_character_property](#get_character_property)`(character_id: String, key: String)` |
| `PackedStringArray` | [get_character_names](#get_character_names)`()` |
| `String` | [get_character_display_name](#get_character_display_name)`(character_id: String)` |

### 노드 태그

| 반환 타입 | 메서드 |
|--------|--------|
| `String` | [get_node_tag](#get_node_tag)`(node_name: String, key: String)` |
| `Dictionary` | [get_node_tags](#get_node_tags)`(node_name: String)` |
| `bool` | [has_node_tag](#has_node_tag)`(node_name: String, key: String)` |

### 테스트

| 반환 타입 | 메서드 |
|--------|--------|
| `void` | [set_seed](#set_seed)`(seed: int)` |

---

## 메서드 설명

### load_story

```
bool load_story(path: String)
```

지정된 경로에서 컴파일된 `.gyb` 스토리 파일을 로드합니다. `res://` 및 `user://` 경로를 지원합니다.

성공 시 `true`를 반환하고, 파일을 열 수 없으면 `false`를 반환합니다.

> **참고:** 이 메서드는 바이너리 데이터를 메모리에 로드하기만 합니다. Runner VM을 초기화하려면 [start](#start)를 호출하세요.

---

### start

```
void start()
```

Runner VM을 초기화하고 스토리의 시작 노드로 이동합니다. 전역 변수가 초기화되고 방문 횟수가 리셋됩니다.

[load_story](#load_story) 이후, [advance](#advance) 이전에 호출해야 합니다.

---

### advance

```
void advance()
```

스토리의 다음 인스트럭션을 실행합니다. 인스트럭션 타입에 따라 다음 시그널 중 하나가 발생합니다:

| 인스트럭션 | 시그널 |
|-------------|--------|
| Line (대사/나레이션) | [dialogue_line](#dialogue_line) |
| Menu (선택지) | [choices_presented](#choices_presented) |
| Command (`@`) | [command_received](#command_received) |
| 스토리 종료 | [story_ended](#story_ended) |

스토리가 끝났으면 [story_ended](#story_ended)를 발생시킵니다.

> **참고:** [command_received](#command_received) 수신 후에는 `advance()`를 다시 호출해야 합니다. [choices_presented](#choices_presented) 수신 후에는 [choose](#choose)를 호출하세요.

---

### choose

```
void choose(index: int)
```

0부터 시작하는 인덱스로 선택지를 선택하고 자동으로 스토리를 진행합니다. [choices_presented](#choices_presented) 시그널을 수신한 후 호출합니다.

---

### is_finished

```
bool is_finished()
```

스토리가 끝에 도달했으면 `true`를 반환합니다.

---

### save_state

```
bool save_state(path: String)
```

완전한 스토리 상태를 `.gys` 파일에 저장합니다. `res://` 및 `user://` 경로를 지원합니다.

저장 내용: 현재 위치, 모든 변수, 콜 스택, 대기 중인 선택지, 방문 횟수, once 선택지 추적 상태.

성공 시 `true`를 반환합니다.

> **참고:** 저장하기 전에 스토리가 로드되어 있어야 합니다.

---

### load_state

```
bool load_state(path: String)
```

`.gys` 파일에서 이전에 저장한 상태를 복원합니다. [load_story](#load_story)를 통해 동일한 스토리가 이미 로드되어 있어야 합니다.

성공 시 `true`를 반환합니다.

---

### get_variable

```
Variant get_variable(name: String)
```

지정된 이름의 변수의 현재 값을 반환합니다. 반환 타입은 변수의 타입에 따라 달라집니다:

| Gyeol 타입 | Godot 타입 |
|-----------|------------|
| Bool | `bool` |
| Int | `int` |
| Float | `float` |
| String | `String` |
| List | `Array[String]` |

변수가 존재하지 않으면 `null`을 반환합니다.

---

### set_variable

```
void set_variable(name: String, value: Variant)
```

GDScript에서 스토리 변수를 설정합니다. 지원하는 Godot 타입:

| Godot 타입 | Gyeol 타입 |
|-----------|-----------|
| `bool` | Bool |
| `int` | Int |
| `float` | Float |
| `String` | String |
| `Array` | List (요소가 String으로 변환됨) |

---

### has_variable

```
bool has_variable(name: String)
```

해당 이름의 변수가 스토리 상태에 존재하면 `true`를 반환합니다.

---

### get_variable_names

```
PackedStringArray get_variable_names()
```

현재 정의된 모든 변수의 이름을 담은 `PackedStringArray`를 반환합니다.

---

### load_locale

```
bool load_locale(path: String)
```

로케일 오버레이 CSV 파일을 로드합니다. 번역된 문자열이 런타임에 원본을 대체합니다. `res://` 및 `user://` 경로를 지원합니다.

성공 시 `true`를 반환합니다.

CSV 형식에 대한 자세한 내용은 [로컬라이제이션](../advanced/localization.md)을 참고하세요.

---

### clear_locale

```
void clear_locale()
```

로케일 오버레이를 제거하여 모든 텍스트를 원본 언어로 되돌립니다.

---

### get_locale

```
String get_locale()
```

현재 로드된 로케일 식별자를 반환하거나, 활성 로케일이 없으면 빈 문자열을 반환합니다.

---

### get_visit_count

```
int get_visit_count(node_name: String)
```

현재 플레이에서 지정된 노드에 진입한 횟수를 반환합니다.

---

### has_visited

```
bool has_visited(node_name: String)
```

지정된 노드를 최소 한 번 방문했으면 `true`를 반환합니다.

---

### get_character_property

```
String get_character_property(character_id: String, key: String)
```

캐릭터 속성의 값을 반환합니다(스크립트의 `character` 정의에서). 찾을 수 없으면 빈 문자열을 반환합니다.

---

### get_character_names

```
PackedStringArray get_character_names()
```

정의된 모든 캐릭터 ID를 담은 `PackedStringArray`를 반환합니다.

---

### get_character_display_name

```
String get_character_display_name(character_id: String)
```

캐릭터의 `displayName` 속성을 반환하는 편의 메서드입니다. `get_character_property(character_id, "displayName")`와 동일합니다.

---

### get_node_tag

```
String get_node_tag(node_name: String, key: String)
```

노드의 메타데이터 태그 값을 반환합니다. 태그가 존재하지 않으면 빈 문자열을 반환합니다.

```gdscript
var difficulty = story_player.get_node_tag("boss_fight", "difficulty")  # "hard"
```

---

### get_node_tags

```
Dictionary get_node_tags(node_name: String)
```

노드의 모든 메타데이터 태그를 Dictionary(String 키, String 값)로 반환합니다.

```gdscript
var tags = story_player.get_node_tags("boss_fight")
# {"difficulty": "hard", "checkpoint": ""}
```

---

### has_node_tag

```
bool has_node_tag(node_name: String, key: String)
```

지정된 노드에 해당 메타데이터 태그가 있으면 `true`를 반환합니다.

```gdscript
if story_player.has_node_tag("shop", "repeatable"):
    # Allow revisiting
    pass
```

---

### set_seed

```
void set_seed(seed: int)
```

결정적 랜덤 분기 선택을 위한 RNG 시드를 설정합니다. 테스트 및 리플레이 시스템에 유용합니다.

```gdscript
story_player.set_seed(42)  # Same seed = same random sequence every time
```
