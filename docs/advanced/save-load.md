# Save & Load

Gyeol provides complete state serialization for saving and loading game progress.

## Overview

Save files (`.gys`) are FlatBuffers binary files that capture the entire runner state:

| Data | Description |
|------|-------------|
| Current node + PC | Where in the story we are |
| Variables | All runtime variables with values |
| Call stack | Active function calls and shadowed variables |
| Pending choices | Currently displayed menu choices |
| Visit counts | How many times each node was visited |
| Once-choice tracking | Which once-choices have been selected |
| Finished flag | Whether the story has ended |

## Usage

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

## Requirements

- **For saving:** A story must be loaded (`hasStory() == true`)
- **For loading:** The **same** story must be loaded before restoring state
- The `.gys` file format is tied to the story version

## What Gets Saved

### Variables

All runtime variables including their types:

```gyeol
$ hp = 100        # Saved as Int
$ name = "Hero"   # Saved as String
$ alive = true    # Saved as Bool
$ items += "key"  # Saved as List
```

### Call Stack

If the save happens during a function call, the entire call stack is preserved:

```gyeol
label start:
    call helper       # If saved here...
    hero "Back!"      # ...this resumes after load

label helper:
    hero "Inside!"    # Save point
```

The save captures:
- Each frame's node name and program counter
- Return variable name (for `$ x = call ...`)
- Shadowed variables (parameters that override globals)
- Parameter names

### Visit Counts

Every node's visit count is serialized:

```
# If shop was visited 3 times before save...
# After load, visit_count("shop") == 3
```

### Once-Choice Tracking

Once-choices that were already selected remain hidden after load:

```gyeol
menu:
    "One-time offer" -> deal #once    # If selected before save, stays hidden after load
    "Browse" -> browse
```

### Pending Choices

If the save happens while a menu is displayed, the pending choices (including their modifiers) are preserved.

## Save File Format

The `.gys` file follows the `SaveState` schema in `schemas/gyeol.fbs`:

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

## Backward Compatibility

The save/load system handles missing fields gracefully:

- Loading saves from older versions (without newer fields) works correctly
- Missing fields use their default values (empty arrays, zero counts, etc.)
- New features (once-choices, choice modifiers, function parameters) degrade gracefully

## Error Handling

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

## Multiple Save Slots

Implement multiple save slots by using different file paths:

```gdscript
func save_game(slot: int):
    var path = "user://saves/slot_%d.gys" % slot
    story_player.save_state(path)

func load_game(slot: int):
    var path = "user://saves/slot_%d.gys" % slot
    if FileAccess.file_exists(path):
        story_player.load_state(path)
```

## Auto-save Pattern

```gdscript
func _on_dialogue_line(character, text, tags):
    # Auto-save at checkpoints
    if story_player.has_node_tag(get_current_node(), "checkpoint"):
        story_player.save_state("user://autosave.gys")
```
