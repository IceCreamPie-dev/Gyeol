# Godot 연동

Gyeol은 Godot 4.3용 GDExtension을 제공하며, Signal 기반 API를 갖춘 `StoryPlayer` 노드를 사용할 수 있습니다.

## 설정

### 1. GDExtension 빌드

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

빌드 결과: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

### 2. 스토리 컴파일

```bash
GyeolCompiler story.gyeol -o project/story.gyb
```

### 3. Extension 설정

다음 파일들을 Godot 프로젝트의 `bin/` 디렉토리에 복사합니다:

- `libgyeol.windows.template_debug.x86_64.dll`
- `gyeol.gdextension`

`.gdextension` 파일은 Godot에 플러그인 로드 방법을 알려줍니다:

```ini
[configuration]
entry_symbol = "gyeol_library_init"
compatibility_minimum = "4.3"

[libraries]
windows.debug.x86_64 = "res://bin/libgyeol.windows.template_debug.x86_64.dll"
```

### 4. 씬에 StoryPlayer 추가

1. Godot에서 씬을 엽니다
2. `StoryPlayer` 노드를 추가합니다 (`Node`를 상속합니다)
3. 에디터 또는 코드에서 Signal을 연결합니다

## 기본 사용법

```gdscript
extends Control

@onready var story_player: StoryPlayer = $StoryPlayer
@onready var dialogue_label: RichTextLabel = $DialogueLabel
@onready var choices_container: VBoxContainer = $ChoicesContainer

func _ready():
    # Signal 연결
    story_player.dialogue_line.connect(_on_dialogue_line)
    story_player.choices_presented.connect(_on_choices_presented)
    story_player.command_received.connect(_on_command_received)
    story_player.story_ended.connect(_on_story_ended)

    # 로드 및 시작
    if story_player.load_story("res://story.gyb"):
        story_player.start()
        story_player.advance()

func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    if character.is_empty():
        dialogue_label.text = text                              # 나레이션
    else:
        dialogue_label.text = "[b]%s[/b]: %s" % [character, text]  # 캐릭터 대사

func _on_choices_presented(choices: Array):
    for i in choices.size():
        var btn = Button.new()
        btn.text = choices[i]
        btn.pressed.connect(_on_choice_selected.bind(i))
        choices_container.add_child(btn)

func _on_command_received(type: String, params: Array):
    match type:
        "bg":
            # 배경 변경
            pass
        "sfx":
            # 효과음 재생
            pass
    # 명령 처리 후 자동 진행
    story_player.advance()

func _on_story_ended():
    dialogue_label.text = "[i]--- END ---[/i]"

func _on_choice_selected(index: int):
    # 선택지 버튼 제거
    for child in choices_container.get_children():
        child.queue_free()
    story_player.choose(index)
```

## Signal

| Signal | 매개변수 | 발생 시점 |
|--------|---------|----------|
| `dialogue_line` | `character: String, text: String, tags: Dictionary` | 대사 한 줄이 표시될 준비가 되었을 때 |
| `choices_presented` | `choices: Array[String]` | 선택지 메뉴가 제시될 때 |
| `command_received` | `type: String, params: Array[String]` | `@` 명령이 발견되었을 때 |
| `story_ended` | *(없음)* | 스토리가 종료되었을 때 |

### dialogue_line

모든 `Line` 인스트럭션에서 발생합니다. 나레이션의 경우 `character` 매개변수가 빈 문자열입니다. `tags` 딕셔너리에는 해당 대사의 `#key:value` 태그에서 가져온 메타데이터가 들어 있습니다.

```gdscript
func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    # mood 태그 확인
    if tags.has("mood"):
        update_character_expression(character, tags["mood"])

    # voice 태그 확인
    if tags.has("voice"):
        play_voice(tags["voice"])
```

### choices_presented

`menu:` 블록에 도달하면 발생합니다. 스토리를 계속 진행하려면 반드시 `choose(index)`를 호출해야 합니다. choices 배열에는 각 선택지의 표시 텍스트가 포함됩니다.

### command_received

`@` 명령에 대해 발생합니다. 스토리를 계속 진행하려면 명령 처리 후 반드시 `advance()`를 호출**해야 합니다**.

### story_ended

스토리가 끝에 도달했을 때 (더 이상 실행할 인스트럭션이 없을 때) 발생합니다.

## 변수 다루기

```gdscript
# 변수 가져오기
var hp = story_player.get_variable("hp")        # Variant 반환 (int, float, bool, String, Array)

# 변수 설정하기
story_player.set_variable("player_name", "Hero")
story_player.set_variable("hp", 100)
story_player.set_variable("has_key", true)

# 존재 여부 확인
if story_player.has_variable("gold"):
    var gold = story_player.get_variable("gold")

# 모든 변수 목록
var names: PackedStringArray = story_player.get_variable_names()
for name in names:
    print("%s = %s" % [name, story_player.get_variable(name)])
```

## 저장과 불러오기

```gdscript
# 현재 상태 저장
story_player.save_state("user://save1.gys")

# 저장된 상태 불러오기 (스토리가 이미 로드되어 있어야 합니다)
story_player.load_state("user://save1.gys")
```

`res://` 및 `user://` 경로를 모두 지원합니다. `.gys` 파일에는 다음이 저장됩니다:
- 현재 위치 (노드 + 프로그램 카운터)
- 모든 변수
- 콜 스택
- 대기 중인 선택지
- 방문 횟수
- Once 선택지 추적 정보

## 다국어 지원

```gdscript
# 번역된 로케일 CSV 로드
story_player.load_locale("res://locales/en.csv")

# 현재 로케일 확인
var locale = story_player.get_locale()

# 로케일 초기화 (원문으로 복원)
story_player.clear_locale()
```

CSV 포맷에 대한 자세한 내용은 [다국어 지원](../advanced/localization.md)을 참조하세요.

## 방문 추적

```gdscript
# 노드 방문 횟수 확인
var count = story_player.get_visit_count("shop")

# 방문한 적이 있는지 확인
if story_player.has_visited("secret_room"):
    # 무언가 해금
    pass
```

## 캐릭터 API

```gdscript
# 정의된 모든 캐릭터 ID 가져오기
var characters: PackedStringArray = story_player.get_character_names()

# 표시 이름 가져오기
var display_name = story_player.get_character_display_name("hero")

# 캐릭터 속성 가져오기
var color = story_player.get_character_property("hero", "color")
```

## 노드 태그

```gdscript
# 노드에 태그가 있는지 확인
if story_player.has_node_tag("boss_fight", "difficulty"):
    var diff = story_player.get_node_tag("boss_fight", "difficulty")

# 노드의 모든 태그 가져오기
var tags: Dictionary = story_player.get_node_tags("shop")
for key in tags:
    print("%s = %s" % [key, tags[key]])
```

## 결정적 테스트

```gdscript
# 재현 가능한 랜덤 분기를 위한 RNG 시드 설정
story_player.set_seed(42)
```

## 전체 API 레퍼런스

전체 API는 [StoryPlayer 클래스 레퍼런스](../api/class-story-player.md)를 참조하세요.
