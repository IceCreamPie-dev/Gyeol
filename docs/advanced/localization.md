# Localization

Gyeol supports multi-language stories with stable `line_id` keys and runtime locale overlays.

## Recommended Workflow (PO + JSON)

```
1) Write story  ->  2) Export POT  ->  3) Translate in PO  ->  4) Convert PO to JSON  ->  5) Load locale
```

1. Write `.gyeol` in your source language.
2. Export POT template from the compiled script metadata.
3. Translators work in PO (`msgctxt = line_id`).
4. Convert PO into runtime JSON locale.
5. Load JSON locale at runtime (`Runner` / `StoryPlayer`).

## Step 1: Export POT

```bash
GyeolCompiler story.gyeol --export-strings-po strings.pot
```

Each translatable entry is exported as:

```po
msgctxt "start:0:a3f2"
msgid "Hello, how are you?"
msgstr ""
```

## Step 2: Translate PO

Translators fill `msgstr` values.
Entries marked `#, fuzzy` are ignored by the converter in v1.

## Step 3: Convert PO to Runtime JSON

```bash
GyeolCompiler --po-to-json strings_ko.po -o ko.locale.json --locale ko
```

Output schema:

```json
{
  "version": 1,
  "locale": "ko",
  "entries": {
    "start:0:a3f2": "안녕하세요!"
  }
}
```

Rules:
- `version` is required and currently must be `1`.
- `entries` is required (`line_id -> translated text`).
- Empty translations are ignored.
- Unknown `line_id` values are ignored at runtime.

## Step 4: Load Locale at Runtime

### C++ (Runner)

```cpp
runner.loadLocale("ko.locale.json");
// ...
runner.clearLocale();
```

### GDScript (StoryPlayer)

```gdscript
story_player.load_locale("res://locales/ko.locale.json")
story_player.clear_locale()
```

## Backward Compatibility

CSV locale loading is still supported:

```bash
GyeolCompiler story.gyeol --export-strings strings.csv
```

`Runner::loadLocale(path)` auto-detects:
- `.json` -> JSON locale
- other extensions -> CSV locale
