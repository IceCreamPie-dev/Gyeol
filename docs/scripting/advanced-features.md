# Advanced Features

## Dialogue Tags

Attach metadata to dialogue lines using `#key:value` tags:

```
hero "I'm furious!" #mood:angry #pose:arms_crossed
hero "Listen carefully." #voice:hero_line42.wav
hero "This is important!" #important
```

### Tag Format

| Format | Example | Description |
|--------|---------|-------------|
| `#key:value` | `#mood:angry` | Key-value metadata |
| `#key` | `#important` | Boolean flag (value = empty string) |

Multiple tags are space-separated after the dialogue text.

### Accessing Tags

Tags are delivered as a Dictionary in the `dialogue_line` signal:

```gdscript
func _on_dialogue_line(character: String, text: String, tags: Dictionary):
    if tags.has("mood"):
        set_expression(character, tags["mood"])
    if tags.has("voice"):
        play_voice(tags["voice"])
    if tags.has("pose"):
        set_pose(character, tags["pose"])
```

### Voice Asset (Legacy)

The `#voice:filename` tag is the standard way to attach voice files:

```
hero "Hello!" #voice:hero_hello.wav
```

## Node Metadata Tags

Attach metadata to labels (nodes) for game-side logic:

```
label shop #repeatable #category=shop:
    merchant "Welcome!"

label boss_fight #difficulty=hard #checkpoint:
    hero "Let's do this!"

label helper(a, b) #pure:
    return a + b
```

### Node Tag Format

| Format | Example | Description |
|--------|---------|-------------|
| `#key=value` | `#difficulty=hard` | Key-value tag |
| `#key` | `#checkpoint` | Boolean flag (value = empty string) |

> **Note:** Node tags use `=` as separator (not `:`), to avoid ambiguity with the label's colon.

Tags appear between the name/parameters and the colon.

### Querying Node Tags

**C++ (Runner):**

```cpp
std::string diff = runner.getNodeTag("boss_fight", "difficulty"); // "hard"
bool isCp = runner.hasNodeTag("boss_fight", "checkpoint");        // true
auto tags = runner.getNodeTags("boss_fight");                     // vector of pairs
```

**GDScript (StoryPlayer):**

```gdscript
var diff = story_player.get_node_tag("boss_fight", "difficulty")
var is_cp = story_player.has_node_tag("boss_fight", "checkpoint")
var tags = story_player.get_node_tags("boss_fight")  # Dictionary
```

### Use Cases

- **Checkpoints:** `#checkpoint` - mark nodes as save points
- **Repeatable content:** `#repeatable` - allow revisiting
- **Categories:** `#category=shop` - group nodes by type
- **Difficulty:** `#difficulty=hard` - tag encounters
- **Pure functions:** `#pure` - mark side-effect-free functions

## Choice Modifiers

Control how menu choices appear and disappear over time:

```
menu:
    "Buy healing potion" -> buy_heal #once
    "Browse inventory" -> browse #sticky
    "Leave shop" -> exit #fallback
```

### Modifier Types

| Modifier | Keyword | Behavior |
|----------|---------|----------|
| Default | *(none)* | Always shown when condition passes |
| Once | `#once` | Hidden permanently after being selected |
| Sticky | `#sticky` | Always shown (explicit version of default) |
| Fallback | `#fallback` | Only shown when all other choices are hidden |

### Once Choices

```
menu:
    "Explore the cave" -> cave #once     # Disappears after first visit
    "Rest at camp" -> camp
    "Continue journey" -> journey
```

After selecting "Explore the cave", it won't appear in future visits to this menu.

### Fallback Choices

```
menu:
    "Buy sword (50g)" -> buy_sword if gold >= 50 #once
    "Buy shield (30g)" -> buy_shield if gold >= 30 #once
    "Nothing left to buy." -> leave #fallback
```

The fallback choice only appears when all other choices (sword + shield) are hidden (either by condition or once-selection).

### Combined Conditions + Modifiers

Both orderings are valid:

```
"VIP special" -> vip if is_vip #once
"VIP special" -> vip #once if is_vip
```

### Evaluation Order

1. Collect all choices in the menu
2. Filter by conditions (hide choices whose conditions are false)
3. Filter once-choices (hide previously selected once-choices)
4. Separate remaining into **normal** and **fallback** groups
5. If normal choices exist: show only normal
6. If all normal hidden: show fallback choices
7. If nothing left: return empty array

## String Interpolation

### Basic Variable Substitution

```
hero "Hello {name}, you have {gold} gold."
```

All variable types are automatically converted to strings.

### Inline Conditional Text

Embed runtime conditions directly in text:

```
hero "{if hp > 50}You look strong!{else}You're injured.{endif}"
```

#### Supported Condition Types

| Condition | Example |
|-----------|---------|
| Truthiness | `{if has_key}You have a key.{endif}` |
| Comparison | `{if hp > 50}strong{else}weak{endif}` |
| Visit check | `{if visited(cave)}been there{endif}` |
| Visit count | `{if visit_count(shop) > 2}regular{endif}` |

#### Nesting

Variables can be interpolated within conditional branches:

```
hero "{if gold > 0}You have {gold} coins.{else}You're broke!{endif}"
```

## Multi-file Projects

### Import System

Split large stories across multiple files:

```
# main.gyeol
import "characters.gyeol"
import "chapter1/intro.gyeol"
import "chapter1/battles.gyeol"

label start:
    call intro_sequence
```

```
# characters.gyeol
character hero:
    displayName: "Hero"
    color: "#4CAF50"

character villain:
    displayName: "Dark Lord"
```

```
# chapter1/intro.gyeol
label intro_sequence:
    @ bg "castle.png"
    hero "Our journey begins!"
```

### Import Rules

- Paths are **relative** to the importing file
- **Circular imports** are detected and reported as errors
- **Self-imports** are detected and reported
- **Duplicate label names** across files are errors
- The **start node** is always the first label in the **main file**
- All files share a single **string pool** (deduplication)
- Global variables from all files are merged

## Visit Tracking

### Automatic Counting

Every time a node is entered (via `jump`, `call`, `choose`, or `start`), its visit count increments:

```
label shop:
    hero "Welcome to the shop!"
    hero "This is visit #{visit_count(shop)}."
```

### Querying Visit Data

| Context | visit_count | visited |
|---------|-------------|---------|
| Expression | `$ n = visit_count("shop")` | `$ v = visited("shop")` |
| Condition | `if visit_count("shop") > 3 -> regular` | `if visited("cave") -> knows_secret` |
| Interpolation | `"Visited {visit_count(shop)} times"` | N/A |
| Inline condition | `{if visit_count(shop) > 2}regular{endif}` | `{if visited(cave)}yes{endif}` |

Both quoted and unquoted node names are accepted:
```
visit_count("shop")    # Quoted
visit_count(shop)      # Unquoted (same result)
```

## Character Definitions

Define character metadata for your game engine to use:

```
character hero:
    displayName: "Brave Hero"
    color: "#4CAF50"
    voice: "hero_voice_pack"
    portrait: "hero_portrait.png"

character merchant:
    displayName: "Shopkeeper"
    color: "#FFC107"
```

### Querying Character Data

**C++:**
```cpp
auto names = runner.getCharacterNames();           // ["hero", "merchant"]
auto display = runner.getCharacterDisplayName("hero"); // "Brave Hero"
auto color = runner.getCharacterProperty("hero", "color"); // "#4CAF50"
```

**GDScript:**
```gdscript
var names = story_player.get_character_names()
var display = story_player.get_character_display_name("hero")
var color = story_player.get_character_property("hero", "color")
```

## Deterministic Random

Set the RNG seed for reproducible random branches:

```cpp
runner.setSeed(42);  // Same seed = same random sequence
```

```gdscript
story_player.set_seed(42)
```

Useful for testing and replay systems.
