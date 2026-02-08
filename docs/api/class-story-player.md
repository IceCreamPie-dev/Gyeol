# StoryPlayer

**Inherits:** [Node](https://docs.godotengine.org/en/stable/classes/class_node.html)

GDExtension node for playing Gyeol stories in Godot 4.3.

## Description

`StoryPlayer` is the main interface for running Gyeol stories in Godot. It wraps the core C++ Runner VM and exposes a signal-based API for dialogue, choices, commands, and story flow.

Add a `StoryPlayer` node to your scene, load a compiled `.gyb` story file, connect the signals, and call `advance()` to step through the story.

## Tutorials

- [Godot Integration Guide](../getting-started/godot-integration.md)
- [Quick Start](../getting-started/quick-start.md)

---

## Signals

### dialogue_line

```
dialogue_line(character: String, text: String, tags: Dictionary)
```

Emitted when a dialogue line is encountered. `character` is the character ID (empty string for narration). `text` is the dialogue text with interpolation already applied. `tags` contains metadata from `#key:value` tags.

---

### choices_presented

```
choices_presented(choices: Array)
```

Emitted when a `menu:` block is reached. `choices` is an Array of String containing the display text for each available option. Call [choose](#choose) to select one and continue.

---

### command_received

```
command_received(type: String, params: Array)
```

Emitted when an `@` command is encountered. `type` is the command name, `params` is an Array of String. You **must** call [advance](#advance) after handling the command.

---

### story_ended

```
story_ended()
```

Emitted when the story reaches its end.

---

## Methods

### Core

| Return | Method |
|--------|--------|
| `bool` | [load_story](#load_story)`(path: String)` |
| `void` | [start](#start)`()` |
| `void` | [advance](#advance)`()` |
| `void` | [choose](#choose)`(index: int)` |
| `bool` | [is_finished](#is_finished)`()` |

### Save / Load

| Return | Method |
|--------|--------|
| `bool` | [save_state](#save_state)`(path: String)` |
| `bool` | [load_state](#load_state)`(path: String)` |

### Variables

| Return | Method |
|--------|--------|
| `Variant` | [get_variable](#get_variable)`(name: String)` |
| `void` | [set_variable](#set_variable)`(name: String, value: Variant)` |
| `bool` | [has_variable](#has_variable)`(name: String)` |
| `PackedStringArray` | [get_variable_names](#get_variable_names)`()` |

### Localization

| Return | Method |
|--------|--------|
| `bool` | [load_locale](#load_locale)`(path: String)` |
| `void` | [clear_locale](#clear_locale)`()` |
| `String` | [get_locale](#get_locale)`()` |

### Visit Tracking

| Return | Method |
|--------|--------|
| `int` | [get_visit_count](#get_visit_count)`(node_name: String)` |
| `bool` | [has_visited](#has_visited)`(node_name: String)` |

### Characters

| Return | Method |
|--------|--------|
| `String` | [get_character_property](#get_character_property)`(character_id: String, key: String)` |
| `PackedStringArray` | [get_character_names](#get_character_names)`()` |
| `String` | [get_character_display_name](#get_character_display_name)`(character_id: String)` |

### Node Tags

| Return | Method |
|--------|--------|
| `String` | [get_node_tag](#get_node_tag)`(node_name: String, key: String)` |
| `Dictionary` | [get_node_tags](#get_node_tags)`(node_name: String)` |
| `bool` | [has_node_tag](#has_node_tag)`(node_name: String, key: String)` |

### Testing

| Return | Method |
|--------|--------|
| `void` | [set_seed](#set_seed)`(seed: int)` |

---

## Method Descriptions

### load_story

```
bool load_story(path: String)
```

Loads a compiled `.gyb` story file from the given path. Supports `res://` and `user://` paths.

Returns `true` on success, `false` if the file could not be opened.

> **Note:** This only loads the binary data into memory. Call [start](#start) to initialize the Runner VM.

---

### start

```
void start()
```

Initializes the Runner VM and jumps to the story's start node. Global variables are initialized and visit counts are reset.

Must be called after [load_story](#load_story) and before [advance](#advance).

---

### advance

```
void advance()
```

Executes the next instruction in the story. Depending on the instruction type, one of the following signals will be emitted:

| Instruction | Signal |
|-------------|--------|
| Line (dialogue/narration) | [dialogue_line](#dialogue_line) |
| Menu (choices) | [choices_presented](#choices_presented) |
| Command (`@`) | [command_received](#command_received) |
| End of story | [story_ended](#story_ended) |

If the story is finished, emits [story_ended](#story_ended).

> **Note:** After receiving [command_received](#command_received), you must call `advance()` again to continue. After [choices_presented](#choices_presented), call [choose](#choose) instead.

---

### choose

```
void choose(index: int)
```

Selects a choice by its 0-based index and automatically advances the story. Call this after receiving the [choices_presented](#choices_presented) signal.

---

### is_finished

```
bool is_finished()
```

Returns `true` if the story has reached its end.

---

### save_state

```
bool save_state(path: String)
```

Saves the complete story state to a `.gys` file. Supports `res://` and `user://` paths.

The save includes: current position, all variables, call stack, pending choices, visit counts, and once-choice tracking.

Returns `true` on success.

> **Note:** A story must be loaded before saving.

---

### load_state

```
bool load_state(path: String)
```

Restores a previously saved state from a `.gys` file. The same story must already be loaded via [load_story](#load_story).

Returns `true` on success.

---

### get_variable

```
Variant get_variable(name: String)
```

Returns the current value of the named variable. The return type depends on the variable's type:

| Gyeol Type | Godot Type |
|-----------|------------|
| Bool | `bool` |
| Int | `int` |
| Float | `float` |
| String | `String` |
| List | `Array[String]` |

Returns `null` if the variable does not exist.

---

### set_variable

```
void set_variable(name: String, value: Variant)
```

Sets a story variable from GDScript. Supported Godot types:

| Godot Type | Gyeol Type |
|-----------|-----------|
| `bool` | Bool |
| `int` | Int |
| `float` | Float |
| `String` | String |
| `Array` | List (elements converted to String) |

---

### has_variable

```
bool has_variable(name: String)
```

Returns `true` if the named variable exists in the story state.

---

### get_variable_names

```
PackedStringArray get_variable_names()
```

Returns a `PackedStringArray` containing the names of all currently defined variables.

---

### load_locale

```
bool load_locale(path: String)
```

Loads a locale overlay CSV file. Translated strings replace originals at runtime. Supports `res://` and `user://` paths.

Returns `true` on success.

See [Localization](../advanced/localization.md) for details on the CSV format.

---

### clear_locale

```
void clear_locale()
```

Removes the locale overlay, reverting all text to the original language.

---

### get_locale

```
String get_locale()
```

Returns the currently loaded locale identifier, or an empty string if no locale is active.

---

### get_visit_count

```
int get_visit_count(node_name: String)
```

Returns the number of times the specified node has been entered during this playthrough.

---

### has_visited

```
bool has_visited(node_name: String)
```

Returns `true` if the specified node has been visited at least once.

---

### get_character_property

```
String get_character_property(character_id: String, key: String)
```

Returns the value of a character property (from `character` definitions in the script). Returns an empty string if not found.

---

### get_character_names

```
PackedStringArray get_character_names()
```

Returns a `PackedStringArray` containing all defined character IDs.

---

### get_character_display_name

```
String get_character_display_name(character_id: String)
```

Convenience method that returns the `displayName` property of a character. Equivalent to `get_character_property(character_id, "displayName")`.

---

### get_node_tag

```
String get_node_tag(node_name: String, key: String)
```

Returns the value of a metadata tag on a node. Returns an empty string if the tag doesn't exist.

```gdscript
var difficulty = story_player.get_node_tag("boss_fight", "difficulty")  # "hard"
```

---

### get_node_tags

```
Dictionary get_node_tags(node_name: String)
```

Returns all metadata tags on a node as a Dictionary (String keys, String values).

```gdscript
var tags = story_player.get_node_tags("boss_fight")
# {"difficulty": "hard", "checkpoint": ""}
```

---

### has_node_tag

```
bool has_node_tag(node_name: String, key: String)
```

Returns `true` if the specified node has the given metadata tag.

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

Sets the RNG seed for deterministic random branch selection. Useful for testing and replay systems.

```gdscript
story_player.set_seed(42)  # Same seed = same random sequence every time
```
