# Script Syntax Reference

Complete reference for the `.gyeol` scripting language.

## File Structure

A `.gyeol` file consists of these top-level elements:

```gyeol
import "other_file.gyeol"     # 1. Imports (optional)
character hero:                # 2. Character definitions (optional)
    displayName: "Hero"
$ global_var = 100             # 3. Global variables (optional)
label start:                   # 4. Node definitions
    hero "Hello!"
```

**Indentation** is significant:
- **0 spaces** - Top-level: `label`, `character`, `import`, global `$`
- **4 spaces** - Inside labels: dialogue, variables, flow control
- **8 spaces** - Inside blocks: menu choices, random branches

## Comments

```gyeol
# This is a comment
hero "Hello!"  # Inline comments are NOT supported
```

Comments start with `#` at the beginning of a line (after optional whitespace).

## Imports

```gyeol
import "common.gyeol"
import "characters/hero.gyeol"
```

- Merges all labels from the imported file into the current compilation
- Paths are relative to the importing file
- Circular imports are detected and reported as errors
- Duplicate label names across files are reported as errors
- The **start node** is always the first label in the main file

## Character Definitions

```gyeol
character hero:
    displayName: "Hero"
    color: "#4CAF50"
    voice: "hero_voice"

character villain:
    displayName: "Dark Lord"
    color: "#F44336"
```

- Character ID is the identifier after `character`
- Properties are `key: value` pairs (4-space indent)
- Values can be quoted or unquoted
- Properties are stored as `Tag` key-value pairs in the compiled binary
- Query via API: `getCharacterProperty(id, key)`, `getCharacterDisplayName(id)`

## Labels (Nodes)

```gyeol
label start:
    # Content here

label helper(a, b):
    # Function with parameters
```

Labels are the fundamental unit of story structure (equivalent to Ren'Py's `label`, Ink's `knot`, or Yarn's `node`).

- The first label in the main file becomes the **start node**
- Labels can have **parameters** for function-style calls
- Labels can have **metadata tags** (see [Advanced Features](advanced-features.md))

### Node Metadata Tags

```gyeol
label shop #repeatable #category=shop:
    hero "Welcome to the shop!"

label boss_fight #difficulty=hard #checkpoint:
    hero "Prepare yourself!"
```

- Tags appear after the name/parameters, before the colon
- `#key` - Boolean flag tag (value is empty string)
- `#key=value` - Key-value tag (use `=`, not `:`)
- Query via API: `getNodeTag(name, key)`, `getNodeTags(name)`, `hasNodeTag(name, key)`

## Dialogue

### Character Dialogue

```gyeol
hero "Hello, how are you?"
villain "Surrender now!"
```

Format: `<character_id> "<text>"`

### Narration

```gyeol
"The wind howled through the empty streets."
"A door creaked open in the distance."
```

Narration lines have no character ID (stored as `character_id = -1`).

### String Interpolation

```gyeol
hero "Hello, {player_name}!"
hero "You have {gold} gold coins."
hero "HP: {hp}/{max_hp}"
```

Variables inside `{...}` are replaced at runtime with their current values.

### Inline Conditional Text

```gyeol
hero "{if hp > 50}You look strong!{else}You look tired.{endif}"
hero "You have {if gold > 0}{gold} coins{else}no money{endif}."
hero "{if visited(shop)}Welcome back!{else}First time here?{endif}"
```

Inline conditions support:
- Simple truthiness: `{if variable}...{endif}`
- Comparison: `{if hp > 50}...{endif}`
- Functions: `{if visited(node)}...{endif}`, `{if visit_count(node) > 3}...{endif}`
- Optional `{else}` clause
- Nested interpolation within branches

### Escape Sequences

| Sequence | Result |
|----------|--------|
| `\\n` | Newline |
| `\\t` | Tab |
| `\\"` | Double quote |
| `\\\\` | Backslash |

### Dialogue Tags (Metadata)

```gyeol
hero "I'm angry!" #mood:angry #pose:arms_crossed
hero "Listen to this." #voice:hero_line42.wav
hero "Important line!" #important
```

- Tags follow the dialogue text after a space
- Format: `#key:value` or `#key` (valueless)
- Multiple tags separated by spaces
- Exposed to the game engine via the `tags` Dictionary in `dialogue_line` signal

## Menu (Choices)

```gyeol
menu:
    "Go left" -> cave
    "Go right" -> forest
    "Stay here" -> camp
```

### Conditional Choices

```gyeol
menu:
    "Open the door" -> locked_room if has_key
    "Walk away" -> hallway
```

A conditional choice is only displayed if the variable evaluates to true.

### Choice Modifiers

```gyeol
menu:
    "Buy potion" -> buy_potion #once
    "Browse wares" -> browse #sticky
    "Leave" -> exit #fallback
    "Special offer" -> special if vip #once
```

| Modifier | Behavior |
|----------|----------|
| *(none)* | **Default** - shown whenever condition passes |
| `#once` | Shown until selected once, then hidden permanently |
| `#sticky` | Always shown (explicit default, same as no modifier) |
| `#fallback` | Only shown when all other choices are hidden |

- Modifiers and conditions can be combined: `"text" -> node if var #once` or `"text" -> node #once if var`
- Once-tracking persists across save/load
- If all non-fallback choices are hidden and no fallback exists, an empty choices array is returned

## Variables

### Declaration

```gyeol
$ hp = 100                  # Integer
$ name = "Hero"             # String
$ is_ready = true           # Boolean
$ speed = 3.14              # Float
```

### Global Variables

Variables declared before the first label are **global** and initialized at story start:

```gyeol
$ max_hp = 100
$ gold = 0

label start:
    hero "Starting adventure with {gold} gold."
```

### Assignment with Expressions

```gyeol
$ hp = hp - 10
$ damage = attack * 2 + bonus
$ total = (a + b) * c
$ count = visit_count("shop")
$ been_there = visited("cave")
```

### List Variables

```gyeol
$ inventory += "sword"       # Append to list
$ inventory -= "potion"      # Remove from list
```

See [Variables & Expressions](variables-and-expressions.md) for the complete expression reference.

## Flow Control

### Jump

```gyeol
jump next_scene              # One-way jump (no return)
```

### Call / Return

```gyeol
call helper_function         # Call and return when done
$ result = call compute(10, 20)  # Call with return value
```

### Conditions

```gyeol
if hp > 50 -> strong_path
elif hp > 20 -> weak_path
else -> critical_path
```

```gyeol
if hp > 0 -> alive else dead
if has_key == true -> open_door else locked
if courage >= 10 and wisdom >= 5 -> hero_path
```

### Random Branching

```gyeol
random:
    50 -> common_event       # Weight 50
    30 -> uncommon_event     # Weight 30
    10 -> rare_event         # Weight 10
    -> ultra_rare            # Weight 1 (default)
```

See [Flow Control](flow-control.md) for details.

## Functions

```gyeol
label greet(name, title):
    hero "Hello, {title} {name}!"
    return name

label start:
    $ result = call greet("Hero", "Sir")
```

See [Functions](functions.md) for details.

## Commands

```gyeol
@ bg "forest.png"
@ sfx "sword_clash.wav"
@ bgm "battle_theme.ogg" loop
@ shake 0.5
```

Commands are passed through to the game engine via the `command_received` signal. The engine defines what commands are supported.

- First word after `@` is the command `type`
- Remaining words are `params` (space-separated, quotes for strings with spaces)

## Complete Example

```gyeol
import "characters.gyeol"

$ gold = 50
$ has_sword = false

label start:
    @ bg "town_square.png"
    @ bgm "town_theme.ogg"
    "The sun rises over the quiet town square."
    hero "Time to prepare for the journey." #mood:determined
    menu:
        "Visit the shop" -> shop
        "Head to the gate" -> gate if has_sword
        "Rest at the inn" -> inn #once
        "Just wait" -> start #fallback

label shop #repeatable #category=shop:
    merchant "Welcome! What can I do for you?"
    if gold >= 30 -> can_buy else too_poor

label can_buy:
    merchant "I have a fine sword for 30 gold."
    menu:
        "Buy the sword (30g)" -> buy_sword #once
        "Leave" -> start

label buy_sword:
    $ gold = gold - 30
    $ has_sword = true
    merchant "A fine choice! Here you go."
    @ sfx "purchase.wav"
    jump start

label too_poor:
    merchant "Come back when you have more gold."
    jump start

label inn:
    "You rest at the inn and feel refreshed."
    jump start

label gate:
    hero "With sword in hand, I'm ready!"
    "And so the adventure begins..."
```
