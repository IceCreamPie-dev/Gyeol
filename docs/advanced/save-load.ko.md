# 저장 및 로드

Gyeol은 게임 진행 상황의 저장과 로드를 위한 완전한 상태 직렬화를 제공합니다.

## 개요

세이브 파일(`.gys`)은 Runner의 전체 상태를 캡처하는 FlatBuffers 바이너리 파일입니다:

| 데이터 | 설명 |
|------|-------------|
| 현재 노드 + PC | 스토리에서 현재 위치 |
| 변수 | 값이 포함된 모든 런타임 변수 |
| 콜 스택 | 활성 함수 호출과 섀도된 변수 |
| 대기 중인 선택지 | 현재 표시된 메뉴 선택지 |
| 방문 횟수 | 각 노드가 방문된 횟수 |
| Once 선택지 추적 | 선택된 once 선택지 목록 |
| 종료 플래그 | 스토리 종료 여부 |

## 사용법

### C++ (Runner)

```cpp
// Save
runner.saveState("save1.gys");

// Load (story must already be started)
runner.loadState("save1.gys");
```

### GDScript (StoryPlayer)

```gdscript
# Save - supports res:// and user:// paths
story_player.save_state("user://saves/slot1.gys")

# Load - story must be loaded first
story_player.load_story("res://story.gyb")
story_player.load_state("user://saves/slot1.gys")
```

## 요구 사항

- **저장 시:** 스토리가 로드되어 있어야 합니다 (`hasStory() == true`)
- **로드 시:** 상태를 복원하기 전에 **동일한** 스토리가 로드되어 있어야 합니다
- `.gys` 파일 형식은 스토리 버전에 종속됩니다

## 저장되는 항목

### 변수

타입을 포함한 모든 런타임 변수:

```gyeol
$ hp = 100        # Saved as Int
$ name = "Hero"   # Saved as String
$ alive = true    # Saved as Bool
$ items += "key"  # Saved as List
```

### 콜 스택

함수 호출 중에 저장하면 전체 콜 스택이 보존됩니다:

```gyeol
label start:
    call helper       # If saved here...
    hero "Back!"      # ...this resumes after load

label helper:
    hero "Inside!"    # Save point
```

저장 내용:
- 각 프레임의 노드 이름과 프로그램 카운터
- 반환 변수 이름 (`$ x = call ...`용)
- 섀도된 변수 (전역 변수를 덮어쓴 매개변수)
- 매개변수 이름

### 방문 횟수

모든 노드의 방문 횟수가 직렬화됩니다:

```
# If shop was visited 3 times before save...
# After load, visit_count("shop") == 3
```

### Once 선택지 추적

이미 선택된 once 선택지는 로드 후에도 숨겨진 상태를 유지합니다:

```gyeol
menu:
    "One-time offer" -> deal #once    # If selected before save, stays hidden after load
    "Browse" -> browse
```

### 대기 중인 선택지

메뉴가 표시된 상태에서 저장하면 대기 중인 선택지(수식어 포함)가 보존됩니다.

## 세이브 파일 형식

`.gys` 파일은 `schemas/gyeol.fbs`의 `SaveState` 스키마를 따릅니다:

```
SaveState
  version: string              # Save format version
  story_version: string        # Original story version
  current_node_name: string    # Active node
  pc: uint32                   # Program counter
  finished: bool               # End flag
  variables: [SavedVar]        # All variables
  call_stack: [SavedCallFrame] # Function call frames
  pending_choices: [SavedPendingChoice]  # Active menu
  visit_counts: [SavedVisitCount]        # Node visit data
  chosen_once_choices: [string]          # Once-choice keys
```

## 하위 호환성

저장/로드 시스템은 누락된 필드를 우아하게 처리합니다:

- 이전 버전의 세이브(새로운 필드가 없는)를 로드해도 정상 동작합니다
- 누락된 필드는 기본값(빈 배열, 0 카운트 등)을 사용합니다
- 새로운 기능(once 선택지, 선택지 수식어, 함수 매개변수)은 우아하게 성능 저하됩니다

## 에러 처리

```cpp
// Returns false on error
if (!runner.saveState("invalid/path/save.gys")) {
    // Handle save error
}

if (!runner.loadState("missing.gys")) {
    // Handle load error
}
```

```gdscript
if not story_player.save_state("user://save.gys"):
    print("Save failed!")

if not story_player.load_state("user://save.gys"):
    print("Load failed!")
```

## 다중 세이브 슬롯

다른 파일 경로를 사용하여 다중 세이브 슬롯을 구현합니다:

```gdscript
func save_game(slot: int):
    var path = "user://saves/slot_%d.gys" % slot
    story_player.save_state(path)

func load_game(slot: int):
    var path = "user://saves/slot_%d.gys" % slot
    if FileAccess.file_exists(path):
        story_player.load_state(path)
```

## 자동 저장 패턴

```gdscript
func _on_dialogue_line(character, text, tags):
    # Auto-save at checkpoints
    if story_player.has_node_tag(get_current_node(), "checkpoint"):
        story_player.save_state("user://autosave.gys")
```
