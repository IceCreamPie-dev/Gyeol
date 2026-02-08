# 고급 기능

## 대사 태그

`#key:value` 태그를 사용하여 대사 줄에 메타데이터를 첨부합니다:

```gyeol
hero "I'm furious!" #mood:angry #pose:arms_crossed
hero "Listen carefully." #voice:hero_line42.wav
hero "This is important!" #important
```

### 태그 형식

| 형식 | 예시 | 설명 |
|------|------|------|
| `#key:value` | `#mood:angry` | 키-값 메타데이터 |
| `#key` | `#important` | 불리언 플래그 (값 = 빈 문자열) |

여러 태그는 대사 텍스트 뒤에 공백으로 구분합니다.

### 태그 접근

태그는 `dialogue_line` 시그널에서 Dictionary로 전달됩니다:

```gdscript
func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    if tags.has("mood"):
        set_expression(character, tags["mood"])
    if tags.has("voice"):
        play_voice(tags["voice"])
    if tags.has("pose"):
        set_pose(character, tags["pose"])
```

### 보이스 에셋 (레거시)

`#voice:filename` 태그는 보이스 파일을 연결하는 표준 방법입니다:

```gyeol
hero "Hello!" #voice:hero_hello.wav
```

## 노드 메타데이터 태그

게임 측 로직을 위해 label(노드)에 메타데이터를 첨부합니다:

```gyeol
label shop #repeatable #category=shop:
    merchant "Welcome!"

label boss_fight #difficulty=hard #checkpoint:
    hero "Let's do this!"

label helper(a, b) #pure:
    return a + b
```

### 노드 태그 형식

| 형식 | 예시 | 설명 |
|------|------|------|
| `#key=value` | `#difficulty=hard` | 키-값 태그 |
| `#key` | `#checkpoint` | 불리언 플래그 (값 = 빈 문자열) |

> **참고:** 노드 태그는 구분자로 `=`를 사용합니다 (`:` 아님). label의 콜론과 혼동을 피하기 위함입니다.

태그는 이름/매개변수와 콜론 사이에 위치합니다.

### 노드 태그 조회

**C++ (Runner):**

```cpp
std::string diff = runner.getNodeTag("boss_fight", "difficulty"); // "hard"
bool isCp = runner.hasNodeTag("boss_fight", "checkpoint");        // true
auto tags = runner.getNodeTags("boss_fight");                     // vector of pairs
```

**GDScript (StoryPlayer):**

```gdscript
var diff = story_player.get_node_tag("boss_fight", "difficulty")
var is_cp = story_player.has_node_tag("boss_fight", "checkpoint")
var tags = story_player.get_node_tags("boss_fight")  # Dictionary
```

### 활용 사례

- **체크포인트:** `#checkpoint` - 노드를 세이브 포인트로 표시
- **반복 가능한 콘텐츠:** `#repeatable` - 재방문 허용
- **카테고리:** `#category=shop` - 유형별 노드 그룹화
- **난이도:** `#difficulty=hard` - 전투 태그 지정
- **순수 함수:** `#pure` - 부작용 없는 함수 표시

## 선택지 수식어

시간에 따라 메뉴 선택지가 나타나고 사라지는 방식을 제어합니다:

```gyeol
menu:
    "Buy healing potion" -> buy_heal #once
    "Browse inventory" -> browse #sticky
    "Leave shop" -> exit #fallback
```

### 수식어 유형

| 수식어 | 키워드 | 동작 |
|--------|--------|------|
| 기본값 | *(없음)* | 조건을 만족하면 항상 표시 |
| Once | `#once` | 선택 후 영구적으로 숨김 |
| Sticky | `#sticky` | 항상 표시 (기본값의 명시적 버전) |
| Fallback | `#fallback` | 다른 모든 선택지가 숨겨졌을 때만 표시 |

### Once 선택지

```gyeol
menu:
    "Explore the cave" -> cave #once     # 첫 방문 후 사라짐
    "Rest at camp" -> camp
    "Continue journey" -> journey
```

"Explore the cave"를 선택한 후에는 이 메뉴의 이후 방문에서 나타나지 않습니다.

### Fallback 선택지

```gyeol
menu:
    "Buy sword (50g)" -> buy_sword if gold >= 50 #once
    "Buy shield (30g)" -> buy_shield if gold >= 30 #once
    "Nothing left to buy." -> leave #fallback
```

Fallback 선택지는 다른 모든 선택지(검 + 방패)가 숨겨졌을 때만 나타납니다 (조건 불충족 또는 once 선택에 의해).

### 조건 + 수식어 결합

두 가지 순서 모두 유효합니다:

```gyeol
"VIP special" -> vip if is_vip #once
"VIP special" -> vip #once if is_vip
```

### 평가 순서

1. 메뉴의 모든 선택지를 수집
2. 조건으로 필터링 (조건이 거짓인 선택지 숨김)
3. Once 선택지 필터링 (이전에 선택된 once 선택지 숨김)
4. 나머지를 **일반**과 **fallback** 그룹으로 분리
5. 일반 선택지가 존재하면: 일반만 표시
6. 모든 일반이 숨겨지면: fallback 선택지 표시
7. 아무것도 남지 않으면: 빈 배열 반환

## 문자열 보간

### 기본 변수 치환

```gyeol
hero "Hello {name}, you have {gold} gold."
```

모든 변수 타입은 자동으로 문자열로 변환됩니다.

### 인라인 조건 텍스트

런타임 조건을 텍스트에 직접 삽입합니다:

```gyeol
hero "{if hp > 50}You look strong!{else}You're injured.{endif}"
```

#### 지원되는 조건 유형

| 조건 | 예시 |
|------|------|
| 참/거짓 판정 | `{if has_key}You have a key.{endif}` |
| 비교 | `{if hp > 50}strong{else}weak{endif}` |
| 방문 확인 | `{if visited(cave)}been there{endif}` |
| 방문 횟수 | `{if visit_count(shop) > 2}regular{endif}` |

#### 중첩

조건 분기 안에서 변수를 보간할 수 있습니다:

```gyeol
hero "{if gold > 0}You have {gold} coins.{else}You're broke!{endif}"
```

## 멀티 파일 프로젝트

### Import 시스템

대규모 스토리를 여러 파일로 분할합니다:

```gyeol
# main.gyeol
import "characters.gyeol"
import "chapter1/intro.gyeol"
import "chapter1/battles.gyeol"

label start:
    call intro_sequence
```

```gyeol
# characters.gyeol
character hero:
    displayName: "Hero"
    color: "#4CAF50"

character villain:
    displayName: "Dark Lord"
```

```gyeol
# chapter1/intro.gyeol
label intro_sequence:
    @ bg "castle.png"
    hero "Our journey begins!"
```

### Import 규칙

- 경로는 가져오는 파일에 대한 **상대 경로**입니다
- **순환 import**는 감지되어 에러로 보고됩니다
- **자기 참조 import**도 감지되어 보고됩니다
- 파일 간 **중복 label 이름**은 에러입니다
- **시작 노드**는 항상 **메인 파일**의 첫 번째 label입니다
- 모든 파일이 하나의 **String Pool**을 공유합니다 (중복 제거)
- 모든 파일의 전역 변수가 병합됩니다

## 방문 추적

### 자동 카운팅

노드에 진입할 때마다 (`jump`, `call`, `choose`, `start`를 통해) 방문 횟수가 증가합니다:

```gyeol
label shop:
    hero "Welcome to the shop!"
    hero "This is visit #{visit_count(shop)}."
```

### 방문 데이터 조회

| 컨텍스트 | visit_count | visited |
|----------|-------------|---------|
| 표현식 | `$ n = visit_count("shop")` | `$ v = visited("shop")` |
| 조건문 | `if visit_count("shop") > 3 -> regular` | `if visited("cave") -> knows_secret` |
| 보간 | `"Visited {visit_count(shop)} times"` | 해당 없음 |
| 인라인 조건 | `{if visit_count(shop) > 2}regular{endif}` | `{if visited(cave)}yes{endif}` |

따옴표가 있든 없든 모두 사용할 수 있습니다:
```gyeol
visit_count("shop")    # 따옴표 있음
visit_count(shop)      # 따옴표 없음 (동일한 결과)
```

## 캐릭터 정의

게임 엔진에서 사용할 캐릭터 메타데이터를 정의합니다:

```gyeol
character hero:
    displayName: "Brave Hero"
    color: "#4CAF50"
    voice: "hero_voice_pack"
    portrait: "hero_portrait.png"

character merchant:
    displayName: "Shopkeeper"
    color: "#FFC107"
```

### 캐릭터 데이터 조회

**C++:**
```cpp
auto names = runner.getCharacterNames();           // ["hero", "merchant"]
auto display = runner.getCharacterDisplayName("hero"); // "Brave Hero"
auto color = runner.getCharacterProperty("hero", "color"); // "#4CAF50"
```

**GDScript:**
```gdscript
var names = story_player.get_character_names()
var display = story_player.get_character_display_name("hero")
var color = story_player.get_character_property("hero", "color")
```

## 결정적 랜덤

재현 가능한 랜덤 분기를 위해 RNG 시드를 설정합니다:

```cpp
runner.setSeed(42);  // 같은 시드 = 같은 랜덤 시퀀스
```

```gdscript
story_player.set_seed(42)
```

테스트와 리플레이 시스템에 유용합니다.
