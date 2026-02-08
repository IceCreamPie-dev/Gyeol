# Godot Integration

Gyeol provides a GDExtension for Godot 4.3 that exposes a `StoryPlayer` node with a signal-based API.

## Setup

### 1. Build the GDExtension

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

This produces `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`.

### 2. Compile Your Story

```bash
GyeolCompiler story.gyeol -o project/story.gyb
```

### 3. Configure the Extension

Copy the following files to your Godot project's `bin/` directory:

- `libgyeol.windows.template_debug.x86_64.dll`
- `gyeol.gdextension`

The `.gdextension` file tells Godot how to load the plugin:

```ini
[configuration]
entry_symbol = "gyeol_library_init"
compatibility_minimum = "4.3"

[libraries]
windows.debug.x86_64 = "res://bin/libgyeol.windows.template_debug.x86_64.dll"
```

### 4. Add StoryPlayer to Your Scene

1. Open your scene in Godot
2. Add a new `StoryPlayer` node (it inherits from `Node`)
3. Connect its signals in the editor or via code

## Basic Usage

```gdscript
extends Control

@onready var story_player: StoryPlayer = $StoryPlayer
@onready var dialogue_label: RichTextLabel = $DialogueLabel
@onready var choices_container: VBoxContainer = $ChoicesContainer

func _ready():
    # Connect signals
    story_player.dialogue_line.connect(_on_dialogue_line)
    story_player.choices_presented.connect(_on_choices_presented)
    story_player.command_received.connect(_on_command_received)
    story_player.story_ended.connect(_on_story_ended)

    # Load and start
    if story_player.load_story("res://story.gyb"):
        story_player.start()
        story_player.advance()

func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    if character.is_empty():
        dialogue_label.text = text                              # Narration
    else:
        dialogue_label.text = "[b]%s[/b]: %s" % [character, text]  # Character dialogue

func _on_choices_presented(choices: Array):
    for i in choices.size():
        var btn = Button.new()
        btn.text = choices[i]
        btn.pressed.connect(_on_choice_selected.bind(i))
        choices_container.add_child(btn)

func _on_command_received(type: String, params: Array):
    match type:
        "bg":
            # Change background
            pass
        "sfx":
            # Play sound effect
            pass
    # Auto-advance after handling command
    story_player.advance()

func _on_story_ended():
    dialogue_label.text = "[i]--- END ---[/i]"

func _on_choice_selected(index: int):
    # Clear choice buttons
    for child in choices_container.get_children():
        child.queue_free()
    story_player.choose(index)
```

## Signals

| Signal | Parameters | When |
|--------|-----------|------|
| `dialogue_line` | `character: String, text: String, tags: Dictionary` | A dialogue line is ready to display |
| `choices_presented` | `choices: Array[String]` | A menu with choices is presented |
| `command_received` | `type: String, params: Array[String]` | An `@` command is encountered |
| `story_ended` | *(none)* | The story has finished |

### dialogue_line

Emitted for every `Line` instruction. The `character` parameter is empty for narration lines. The `tags` dictionary contains metadata from `#key:value` tags on the line.

```gdscript
func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    # Check for mood tag
    if tags.has("mood"):
        update_character_expression(character, tags["mood"])

    # Check for voice tag
    if tags.has("voice"):
        play_voice(tags["voice"])
```

### choices_presented

Emitted when a `menu:` block is reached. You must call `choose(index)` to continue. The choices array contains the display text for each option.

### command_received

Emitted for `@` commands. You **must** call `advance()` after handling the command to continue the story.

### story_ended

Emitted when the story reaches its end (no more instructions to execute).

## Working with Variables

```gdscript
# Get a variable
var hp = story_player.get_variable("hp")        # Returns Variant (int, float, bool, String, Array)

# Set a variable
story_player.set_variable("player_name", "Hero")
story_player.set_variable("hp", 100)
story_player.set_variable("has_key", true)

# Check existence
if story_player.has_variable("gold"):
    var gold = story_player.get_variable("gold")

# List all variables
var names: PackedStringArray = story_player.get_variable_names()
for name in names:
    print("%s = %s" % [name, story_player.get_variable(name)])
```

## Save and Load

```gdscript
# Save current state
story_player.save_state("user://save1.gys")

# Load saved state (story must already be loaded)
story_player.load_state("user://save1.gys")
```

Both `res://` and `user://` paths are supported. The `.gys` file stores:
- Current position (node + program counter)
- All variables
- Call stack
- Pending choices
- Visit counts
- Once-choice tracking

## Localization

```gdscript
# Load a translated locale CSV
story_player.load_locale("res://locales/en.csv")

# Check current locale
var locale = story_player.get_locale()

# Clear locale (revert to original)
story_player.clear_locale()
```

See [Localization](../advanced/localization.md) for details on the CSV format.

## Visit Tracking

```gdscript
# Check how many times a node was visited
var count = story_player.get_visit_count("shop")

# Check if ever visited
if story_player.has_visited("secret_room"):
    # Unlock something
    pass
```

## Character API

```gdscript
# Get all defined character IDs
var characters: PackedStringArray = story_player.get_character_names()

# Get display name
var display_name = story_player.get_character_display_name("hero")

# Get any character property
var color = story_player.get_character_property("hero", "color")
```

## Node Tags

```gdscript
# Check if a node has a tag
if story_player.has_node_tag("boss_fight", "difficulty"):
    var diff = story_player.get_node_tag("boss_fight", "difficulty")

# Get all tags on a node
var tags: Dictionary = story_player.get_node_tags("shop")
for key in tags:
    print("%s = %s" % [key, tags[key]])
```

## Deterministic Testing

```gdscript
# Set RNG seed for reproducible random branches
story_player.set_seed(42)
```

## Full API Reference

See [StoryPlayer Class Reference](../api/class-story-player.md) for the complete API.
