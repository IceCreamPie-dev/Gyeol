# 로컬라이제이션

Gyeol은 자동 Line ID 시스템과 CSV 기반 로케일 오버레이를 통해 다국어 스토리를 지원합니다.

## 작동 방식

```
1. Write story  -->  2. Compile + export CSV  -->  3. Translate CSV  -->  4. Load locale at runtime
```

1. 기본 언어로 스토리를 작성합니다
2. 컴파일하고 자동 생성된 Line ID가 포함된 번역 CSV를 추출합니다
3. 번역가가 CSV에 번역을 채웁니다
4. 런타임에 로케일 CSV를 로드하여 번역을 오버레이합니다

## 1단계: 문자열 추출

```bash
GyeolCompiler story.gyeol --export-strings strings.csv
```

번역 가능한 모든 텍스트가 포함된 CSV 파일을 생성합니다:

```csv
id,text
start:0:a3f2,"Hello, how are you?"
start:1:b7c1,"Sure, let's go!"
start:2:c4d8,"No thanks."
shop:0:e1f3,"Welcome to the shop!"
```

### Line ID가 부여되는 항목

| 타입 | 번역 대상? | 예시 |
|------|:------------:|---------|
| 대사 텍스트 | 예 | `hero "Hello!"` |
| 선택지 텍스트 | 예 | `"Go left" -> cave` |
| 노드 이름 | 아니오 | `label start:` |
| 변수 이름 | 아니오 | `$ hp = 100` |
| 명령 타입 | 아니오 | `@ bg "forest.png"` |
| 명령 파라미터 | 아니오 | `@ sfx "sound.wav"` |
| 캐릭터 ID | 아니오 | `character hero:` |

### Line ID 형식

```
{node_name}:{instruction_index}:{hash4}
```

| 부분 | 설명 |
|------|-------------|
| `node_name` | 포함하는 label |
| `instruction_index` | 노드 내 0부터 시작하는 위치 |
| `hash4` | 텍스트의 FNV-1a 해시에서 생성된 4자리 16진수 |

해시는 인스트럭션 순서가 약간 변경되더라도 ID가 안정적으로 유지되도록 합니다.

## 2단계: 번역 작성

`strings.csv`를 복사하고 `text` 열을 번역합니다:

**strings_ko.csv** (한국어):
```csv
id,text
start:0:a3f2,"안녕하세요?"
start:1:b7c1,"좋아, 같이 가자!"
start:2:c4d8,"아니, 됐어."
shop:0:e1f3,"상점에 오신 것을 환영합니다!"
```

**strings_ja.csv** (일본어):
```csv
id,text
start:0:a3f2,"こんにちは、元気ですか？"
start:1:b7c1,"うん、行こう！"
start:2:c4d8,"いいえ、大丈夫。"
shop:0:e1f3,"お店へようこそ！"
```

### 부분 번역

모든 라인을 번역하지 않아도 됩니다. 번역이 없는 라인은 원본 텍스트로 폴백됩니다:

```csv
id,text
start:0:a3f2,"안녕하세요?"
# Lines not in the CSV use the original language
```

## 3단계: 런타임에 로드

### C++ (Runner)

```cpp
runner.loadLocale("strings_ko.csv");  // Load Korean
// ... play story with Korean text ...

runner.clearLocale();                  // Revert to original
runner.loadLocale("strings_ja.csv");  // Switch to Japanese
```

### GDScript (StoryPlayer)

```gdscript
# Load locale
story_player.load_locale("res://locales/ko.csv")

# Check current locale
var locale = story_player.get_locale()

# Clear (revert to original)
story_player.clear_locale()

# Switch language
story_player.load_locale("res://locales/ja.csv")
```

## 런타임 동작

로케일이 로드되면:
- `poolStr()`이 먼저 로케일 오버레이를 확인합니다
- 해당 Line ID에 번역이 있으면 사용됩니다
- 없으면 원본 텍스트를 사용합니다 (폴백)
- 문자열 보간(`{variable}`)은 번역된 텍스트에서도 동작합니다
- 인라인 조건(`{if ...}`)도 번역된 텍스트에서 동작합니다

## 실전 설정

### 폴더 구조

```
project/
  story.gyeol
  story.gyb
  locales/
    strings.csv        # Original (exported)
    ko.csv             # Korean
    ja.csv             # Japanese
    en.csv             # English (if original is different)
```

### Godot에서 언어 선택

```gdscript
var locale_map = {
    "ko": "res://locales/ko.csv",
    "ja": "res://locales/ja.csv",
    "en": "res://locales/en.csv",
}

func set_language(lang: String):
    story_player.clear_locale()
    if lang in locale_map:
        story_player.load_locale(locale_map[lang])

func _ready():
    # Use Godot's system locale
    var lang = OS.get_locale_language()
    set_language(lang)
```

## Yarn Spinner 호환성

Gyeol의 로컬라이제이션 시스템은 Yarn Spinner의 접근 방식에서 영감을 받았습니다:
- 자동 Line ID 생성 (수동 ID 할당 불필요)
- CSV 기반 번역 파일
- 런타임 오버레이 (재컴파일 불필요)
- 번역이 없는 라인은 원본 텍스트로 폴백
