# Godot 연동

Gyeol은 Godot 4.3용 GDExtension을 제공하며, 시그널 기반 API를 가진 `StoryPlayer` 노드를 사용할 수 있습니다.

## 설정

### 1. 로컬 툴체인 준비 (Windows 기본)

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
```

`bootstrap-toolchains.ps1`는 고정 버전 Emscripten 설치를 먼저 시도하고, prebuilt SDK가 없으면 source-build fallback으로 자동 전환합니다(Windows ARM64에서 일반적).

### 2. GDExtension 빌드

```powershell
.\tools\dev\build-godot.ps1
```

산출물: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

ARM64 빌드가 필요하면:

```powershell
.\tools\dev\build-godot.ps1 -Arch arm64
```

직접 빌드(선택):

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

### 3. 스토리 컴파일

```bash
GyeolCompiler story.gyeol -o project/story.gyb
```

### 4. Extension 설정

Godot 프로젝트 `bin/` 폴더에 아래 파일을 배치합니다.

- `libgyeol.windows.template_debug.x86_64.dll`
- `gyeol.gdextension`

`.gdextension` 예시:

```ini
[configuration]
entry_symbol = "gyeol_library_init"
compatibility_minimum = "4.3"

[libraries]
windows.debug.x86_64 = "res://bin/libgyeol.windows.template_debug.x86_64.dll"
```

### 5. Scene에 StoryPlayer 추가

1. Godot에서 Scene 열기
2. `StoryPlayer` 노드 추가 (`Node` 상속)
3. 시그널 연결

## 기본 사용 예시

```gdscript
extends Control

@onready var story_player: StoryPlayer = $StoryPlayer
@onready var dialogue_label: RichTextLabel = $DialogueLabel
@onready var choices_container: VBoxContainer = $ChoicesContainer

func _ready():
    story_player.dialogue_line.connect(_on_dialogue_line)
    story_player.choices_presented.connect(_on_choices_presented)
    story_player.command_received.connect(_on_command_received)
    story_player.wait_requested.connect(_on_wait_requested)
    story_player.yield_emitted.connect(_on_yield_emitted)
    story_player.story_ended.connect(_on_story_ended)

    if story_player.load_story("res://story.gyb"):
        story_player.start()
        story_player.advance()

func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    if character.is_empty():
        dialogue_label.text = text
    else:
        dialogue_label.text = "[b]%s[/b]: %s" % [character, text]

func _on_choices_presented(choices: Array):
    for i in choices.size():
        var btn = Button.new()
        btn.text = choices[i]
        btn.pressed.connect(_on_choice_selected.bind(i))
        choices_container.add_child(btn)

func _on_command_received(type: String, args: Array):
    # args 항목 형태: { "kind": "String|Int|Float|Bool|Identifier", "value": ... }
    # 커맨드 처리 후 계속 진행
    story_player.advance()

func _on_wait_requested(tag: String):
    # 비동기 연출/컷신 완료 시점에 resume()
    story_player.resume()

func _on_yield_emitted():
    # 1틱 양보 이벤트, 다시 advance()
    story_player.advance()

func _on_story_ended():
    dialogue_label.text = "[i]--- END ---[/i]"

func _on_choice_selected(index: int):
    for child in choices_container.get_children():
        child.queue_free()
    story_player.choose(index)
```

## 시그널

| 시그널 | 파라미터 | 의미 |
|---|---|---|
| `dialogue_line` | `character: String, text: String, tags: Dictionary` | 대사/내레이션 라인 표시 |
| `choices_presented` | `choices: Array[String]` | 선택지 메뉴 표시 |
| `command_received` | `type: String, args: Array[Dictionary]` | `@` 커맨드 수신 |
| `wait_requested` | `tag: String` | `WAIT` 이벤트 발생, `resume()` 필요 |
| `yield_emitted` | *(없음)* | `YIELD` 이벤트 발생, `advance()` 재호출 |
| `story_ended` | *(없음)* | 스토리 종료 |

## Save/Load

```gdscript
story_player.save_state("user://save1.gys")
story_player.load_state("user://save1.gys")
```

## 로컬라이징

```gdscript
story_player.load_locale("res://locales/ko.locale.json")
story_player.clear_locale()
```

PO/JSON 워크플로는 [로컬라이징](../advanced/localization.md)을 참고하세요.

## 전체 API

전체 메서드 목록은 [StoryPlayer API](../api/class-story-player.md)에서 확인할 수 있습니다.
