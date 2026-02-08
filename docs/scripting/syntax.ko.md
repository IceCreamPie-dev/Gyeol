# 스크립트 문법 레퍼런스

`.gyeol` 스크립팅 언어의 완전한 문법 참조 문서입니다.

## 파일 구조

`.gyeol` 파일은 다음과 같은 최상위 요소로 구성됩니다:

```gyeol
import "other_file.gyeol"     # 1. Import (선택 사항)
character hero:                # 2. 캐릭터 정의 (선택 사항)
    displayName: "Hero"
$ global_var = 100             # 3. 전역 변수 (선택 사항)
label start:                   # 4. 노드 정의
    hero "Hello!"
```

**들여쓰기**는 의미를 가집니다:
- **0칸** - 최상위: `label`, `character`, `import`, 전역 `$`
- **4칸** - label 내부: 대사, 변수, 흐름 제어
- **8칸** - 블록 내부: menu 선택지, random 분기

## 주석

```gyeol
# 이것은 주석입니다
hero "Hello!"  # 인라인 주석은 지원되지 않습니다
```

주석은 줄의 시작(선택적 공백 뒤)에서 `#`으로 시작합니다.

## Import

```gyeol
import "common.gyeol"
import "characters/hero.gyeol"
```

- 가져온 파일의 모든 label을 현재 컴파일에 병합합니다
- 경로는 가져오는 파일 기준 상대 경로입니다
- 순환 import는 감지되어 에러로 보고됩니다
- 파일 간 중복 label 이름은 에러로 보고됩니다
- **시작 노드**는 항상 메인 파일의 첫 번째 label입니다

## 캐릭터 정의

```gyeol
character hero:
    displayName: "Hero"
    color: "#4CAF50"
    voice: "hero_voice"

character villain:
    displayName: "Dark Lord"
    color: "#F44336"
```

- 캐릭터 ID는 `character` 뒤의 식별자입니다
- 속성은 `key: value` 쌍입니다 (4칸 들여쓰기)
- 값은 따옴표가 있거나 없을 수 있습니다
- 속성은 컴파일된 바이너리에서 `Tag` 키-값 쌍으로 저장됩니다
- API를 통해 조회: `getCharacterProperty(id, key)`, `getCharacterDisplayName(id)`

## Label (노드)

```gyeol
label start:
    # 내용 작성

label helper(a, b):
    # 매개변수가 있는 함수
```

Label은 스토리 구조의 기본 단위입니다 (Ren'Py의 `label`, Ink의 `knot`, Yarn의 `node`에 해당).

- 메인 파일의 첫 번째 label이 **시작 노드**가 됩니다
- Label에 **매개변수**를 선언하여 함수 스타일 호출이 가능합니다
- Label에 **메타데이터 태그**를 붙일 수 있습니다 ([고급 기능](advanced-features.md) 참조)

### 노드 메타데이터 태그

```gyeol
label shop #repeatable #category=shop:
    hero "Welcome to the shop!"

label boss_fight #difficulty=hard #checkpoint:
    hero "Prepare yourself!"
```

- 태그는 이름/매개변수 뒤, 콜론 앞에 위치합니다
- `#key` - 불리언 플래그 태그 (값은 빈 문자열)
- `#key=value` - 키-값 태그 (`:` 대신 `=` 사용)
- API를 통해 조회: `getNodeTag(name, key)`, `getNodeTags(name)`, `hasNodeTag(name, key)`

## 대사

### 캐릭터 대사

```gyeol
hero "Hello, how are you?"
villain "Surrender now!"
```

형식: `<character_id> "<text>"`

### 나레이션

```gyeol
"The wind howled through the empty streets."
"A door creaked open in the distance."
```

나레이션 줄에는 캐릭터 ID가 없습니다 (`character_id = -1`로 저장).

### 문자열 보간

```gyeol
hero "Hello, {player_name}!"
hero "You have {gold} gold coins."
hero "HP: {hp}/{max_hp}"
```

`{...}` 안의 변수는 런타임에 현재 값으로 치환됩니다.

### 인라인 조건 텍스트

```gyeol
hero "{if hp > 50}You look strong!{else}You look tired.{endif}"
hero "You have {if gold > 0}{gold} coins{else}no money{endif}."
hero "{if visited(shop)}Welcome back!{else}First time here?{endif}"
```

인라인 조건은 다음을 지원합니다:
- 단순 참/거짓 판정: `{if variable}...{endif}`
- 비교: `{if hp > 50}...{endif}`
- 함수: `{if visited(node)}...{endif}`, `{if visit_count(node) > 3}...{endif}`
- 선택적 `{else}` 절
- 분기 내부에서 중첩 보간

### 이스케이프 시퀀스

| 시퀀스 | 결과 |
|--------|------|
| `\\n` | 줄 바꿈 |
| `\\t` | 탭 |
| `\\"` | 큰따옴표 |
| `\\\\` | 백슬래시 |

### 대사 태그 (메타데이터)

```gyeol
hero "I'm angry!" #mood:angry #pose:arms_crossed
hero "Listen to this." #voice:hero_line42.wav
hero "Important line!" #important
```

- 태그는 대사 텍스트 뒤에 공백을 두고 붙입니다
- 형식: `#key:value` 또는 `#key` (값 없음)
- 여러 태그는 공백으로 구분합니다
- 게임 엔진에는 `dialogue_line` 시그널의 `tags` Dictionary로 전달됩니다

## Menu (선택지)

```gyeol
menu:
    "Go left" -> cave
    "Go right" -> forest
    "Stay here" -> camp
```

### 조건부 선택지

```gyeol
menu:
    "Open the door" -> locked_room if has_key
    "Walk away" -> hallway
```

조건부 선택지는 변수가 참으로 평가될 때만 표시됩니다.

### 선택지 수식어

```gyeol
menu:
    "Buy potion" -> buy_potion #once
    "Browse wares" -> browse #sticky
    "Leave" -> exit #fallback
    "Special offer" -> special if vip #once
```

| 수식어 | 동작 |
|--------|------|
| *(없음)* | **기본값** - 조건을 만족하면 항상 표시 |
| `#once` | 한 번 선택하면 영구적으로 숨김 |
| `#sticky` | 항상 표시 (명시적 기본값, 수식어 없음과 동일) |
| `#fallback` | 다른 모든 선택지가 숨겨졌을 때만 표시 |

- 수식어와 조건을 함께 사용할 수 있습니다: `"text" -> node if var #once` 또는 `"text" -> node #once if var`
- Once 추적은 Save/Load를 통해 유지됩니다
- 모든 비-fallback 선택지가 숨겨지고 fallback도 없으면 빈 선택지 배열이 반환됩니다

## 변수

### 선언

```gyeol
$ hp = 100                  # 정수
$ name = "Hero"             # 문자열
$ is_ready = true           # 불리언
$ speed = 3.14              # 실수
```

### 전역 변수

첫 번째 label 앞에 선언된 변수는 **전역 변수**이며 스토리 시작 시 초기화됩니다:

```gyeol
$ max_hp = 100
$ gold = 0

label start:
    hero "Starting adventure with {gold} gold."
```

### 표현식을 사용한 대입

```gyeol
$ hp = hp - 10
$ damage = attack * 2 + bonus
$ total = (a + b) * c
$ count = visit_count("shop")
$ been_there = visited("cave")
```

### 리스트 변수

```gyeol
$ inventory += "sword"       # 리스트에 추가
$ inventory -= "potion"      # 리스트에서 제거
```

완전한 표현식 참조는 [변수와 표현식](variables-and-expressions.md)을 참조하세요.

## 흐름 제어

### Jump

```gyeol
jump next_scene              # 단방향 이동 (복귀 없음)
```

### Call / Return

```gyeol
call helper_function         # 호출 후 완료되면 복귀
$ result = call compute(10, 20)  # 반환값이 있는 호출
```

### 조건문

```gyeol
if hp > 50 -> strong_path
elif hp > 20 -> weak_path
else -> critical_path
```

```gyeol
if hp > 0 -> alive else dead
if has_key == true -> open_door else locked
if courage >= 10 and wisdom >= 5 -> hero_path
```

### 랜덤 분기

```gyeol
random:
    50 -> common_event       # 가중치 50
    30 -> uncommon_event     # 가중치 30
    10 -> rare_event         # 가중치 10
    -> ultra_rare            # 가중치 1 (기본값)
```

자세한 내용은 [흐름 제어](flow-control.md)를 참조하세요.

## 함수

```gyeol
label greet(name, title):
    hero "Hello, {title} {name}!"
    return name

label start:
    $ result = call greet("Hero", "Sir")
```

자세한 내용은 [함수](functions.md)를 참조하세요.

## 커맨드

```gyeol
@ bg "forest.png"
@ sfx "sword_clash.wav"
@ bgm "battle_theme.ogg" loop
@ shake 0.5
```

커맨드는 `command_received` 시그널을 통해 게임 엔진으로 전달됩니다. 어떤 커맨드를 지원하는지는 엔진에서 정의합니다.

- `@` 뒤의 첫 번째 단어가 커맨드 `type`입니다
- 나머지 단어들은 `params`입니다 (공백으로 구분, 공백이 포함된 문자열은 따옴표 사용)

## 전체 예제

```gyeol
import "characters.gyeol"

$ gold = 50
$ has_sword = false

label start:
    @ bg "town_square.png"
    @ bgm "town_theme.ogg"
    "The sun rises over the quiet town square."
    hero "Time to prepare for the journey." #mood:determined
    menu:
        "Visit the shop" -> shop
        "Head to the gate" -> gate if has_sword
        "Rest at the inn" -> inn #once
        "Just wait" -> start #fallback

label shop #repeatable #category=shop:
    merchant "Welcome! What can I do for you?"
    if gold >= 30 -> can_buy else too_poor

label can_buy:
    merchant "I have a fine sword for 30 gold."
    menu:
        "Buy the sword (30g)" -> buy_sword #once
        "Leave" -> start

label buy_sword:
    $ gold = gold - 30
    $ has_sword = true
    merchant "A fine choice! Here you go."
    @ sfx "purchase.wav"
    jump start

label too_poor:
    merchant "Come back when you have more gold."
    jump start

label inn:
    "You rest at the inn and feel refreshed."
    jump start

label gate:
    hero "With sword in hand, I'm ready!"
    "And so the adventure begins..."
```
