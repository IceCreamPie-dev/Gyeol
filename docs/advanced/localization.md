# Localization

Gyeol supports multi-language stories through an automatic Line ID system and CSV-based locale overlays.

## How It Works

```
1. Write story  -->  2. Compile + export CSV  -->  3. Translate CSV  -->  4. Load locale at runtime
```

1. Write your story in the primary language
2. Compile and export a translation CSV with auto-generated Line IDs
3. Translators fill in translations in the CSV
4. Load the locale CSV at runtime to overlay translations

## Step 1: Export Strings

```bash
GyeolCompiler story.gyeol --export-strings strings.csv
```

This generates a CSV file with all translatable text:

```csv
id,text
start:0:a3f2,"Hello, how are you?"
start:1:b7c1,"Sure, let's go!"
start:2:c4d8,"No thanks."
shop:0:e1f3,"Welcome to the shop!"
```

### What Gets Line IDs

| Type | Translatable? | Example |
|------|:------------:|---------|
| Dialogue text | Yes | `hero "Hello!"` |
| Choice text | Yes | `"Go left" -> cave` |
| Node names | No | `label start:` |
| Variable names | No | `$ hp = 100` |
| Command types | No | `@ bg "forest.png"` |
| Command params | No | `@ sfx "sound.wav"` |
| Character IDs | No | `character hero:` |

### Line ID Format

```
{node_name}:{instruction_index}:{hash4}
```

| Part | Description |
|------|-------------|
| `node_name` | The containing label |
| `instruction_index` | 0-based position within the node |
| `hash4` | 4-digit hex from FNV-1a hash of the text |

The hash ensures IDs remain stable even if instruction order changes slightly.

## Step 2: Create Translations

Copy `strings.csv` and translate the `text` column:

**strings_ko.csv** (Korean):
```csv
id,text
start:0:a3f2,"안녕하세요?"
start:1:b7c1,"좋아, 같이 가자!"
start:2:c4d8,"아니, 됐어."
shop:0:e1f3,"상점에 오신 것을 환영합니다!"
```

**strings_ja.csv** (Japanese):
```csv
id,text
start:0:a3f2,"こんにちは、元気ですか？"
start:1:b7c1,"うん、行こう！"
start:2:c4d8,"いいえ、大丈夫。"
shop:0:e1f3,"お店へようこそ！"
```

### Partial Translation

You don't have to translate every line. Missing translations fall back to the original text:

```csv
id,text
start:0:a3f2,"안녕하세요?"
# Lines not in the CSV use the original language
```

## Step 3: Load at Runtime

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

## Runtime Behavior

When a locale is loaded:
- `poolStr()` checks the locale overlay first
- If a translation exists for the Line ID, it's used
- If not, the original text is used (fallback)
- String interpolation (`{variable}`) works on translated text
- Inline conditions (`{if ...}`) work on translated text

## Practical Setup

### Folder Structure

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

### Language Selection in Godot

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

## Yarn Spinner Compatibility

Gyeol's localization system is inspired by Yarn Spinner's approach:
- Automatic Line ID generation (no manual ID assignment)
- CSV-based translation files
- Runtime overlay (no recompilation needed)
- Fallback to original text for missing translations
