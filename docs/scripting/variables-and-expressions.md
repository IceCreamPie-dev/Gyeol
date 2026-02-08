# Variables & Expressions

## Variable Types

Gyeol supports five variable types:

| Type | Example | Description |
|------|---------|-------------|
| **Bool** | `true`, `false` | Boolean |
| **Int** | `42`, `-10`, `0` | 32-bit signed integer |
| **Float** | `3.14`, `-0.5` | 32-bit floating point |
| **String** | `"hello"` | UTF-8 string |
| **List** | *(runtime only)* | List of strings |

## Declaration & Assignment

### Simple Assignment

```
$ hp = 100
$ name = "Hero"
$ is_alive = true
$ speed = 2.5
```

### Arithmetic Expressions

```
$ hp = hp - 10
$ damage = attack * 2 + bonus
$ total = (a + b) * c
$ remainder = score % 10
```

### Supported Operators

| Operator | Description | Types |
|----------|------------|-------|
| `+` | Addition | Int, Float |
| `-` | Subtraction | Int, Float |
| `*` | Multiplication | Int, Float |
| `/` | Division | Int, Float |
| `%` | Modulo | Int |
| `-` (unary) | Negation | Int, Float |

Operator precedence follows standard math rules: `*`, `/`, `%` before `+`, `-`. Use parentheses to override.

### Type Coercion

When mixing Int and Float in arithmetic:
- Int + Float = Float
- Int * Float = Float
- Division of two Ints produces Int (integer division)

## Global Variables

Variables declared before the first `label` are globals, initialized when the story starts:

```
$ max_hp = 100
$ gold = 0
$ difficulty = "normal"

label start:
    hero "Starting with {gold} gold."
```

## List Operations

Lists are created implicitly with `+=` and `-=` on string values:

```
$ inventory += "sword"       # Append "sword" to inventory list
$ inventory += "potion"      # Append "potion"
$ inventory -= "potion"      # Remove "potion" from list
```

### List Functions in Expressions

| Function | Returns | Description |
|----------|---------|-------------|
| `list_contains(var, "item")` | Bool | Check if list contains item |
| `list_length(var)` | Int | Get list size |

```
if list_contains(inventory, "key") -> has_key_path
$ count = list_length(inventory)
```

## Built-in Functions

### Visit Tracking

| Function | Returns | Description |
|----------|---------|-------------|
| `visit_count("node_name")` | Int | Number of times a node was entered |
| `visited("node_name")` | Bool | Whether a node was ever visited |

```
$ times = visit_count("shop")
$ been_there = visited("cave")

# Both quoted and unquoted node names work
$ times = visit_count(shop)
```

Functions work everywhere expressions are allowed:

```
# In assignments
$ count = visit_count("shop")

# In conditions
if visit_count("shop") > 3 -> regular_customer
if visited("secret") -> unlock_bonus

# In arithmetic
$ bonus = visit_count("shop") * 10

# In string interpolation
hero "You've visited the shop {visit_count(shop)} times."

# In inline conditions
hero "{if visited(shop)}Welcome back!{else}First time here?{endif}"
```

## String Interpolation

Variables can be embedded in dialogue and choice text using `{...}`:

```
hero "Hello, {name}! You have {gold} gold."
```

### Variable Substitution

```
hero "HP: {hp}/{max_hp}"
hero "Level {level} warrior"
```

Any variable name inside `{...}` is replaced with its current value at runtime. All types are converted to string automatically:
- Bool: `"true"` or `"false"`
- Int: decimal number
- Float: decimal with fractional part
- String: as-is
- List: comma-separated items

### Inline Conditions

```
hero "{if hp > 50}You look strong!{else}You look weak.{endif}"
```

Supported condition types:
- **Truthiness:** `{if variable}` - true if variable is truthy (non-zero, non-empty)
- **Comparison:** `{if hp > 50}`, `{if name == "Hero"}`
- **Functions:** `{if visited(cave)}`, `{if visit_count(shop) > 2}`

The `{else}` clause is optional:

```
hero "{if has_key}You have a key.{endif}"
```

Inline conditions can be nested with other interpolation:

```
hero "{if gold > 0}You have {gold} coins.{else}You're broke!{endif}"
```

## Comparison Operators

Used in `if` conditions and inline conditions:

| Operator | Description |
|----------|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `>` | Greater than |
| `<` | Less than |
| `>=` | Greater or equal |
| `<=` | Less or equal |

## Logical Operators

Combine conditions with logical operators:

```
if hp > 0 and has_weapon == true -> can_fight
if is_tired or is_hungry -> need_rest
if not is_dead -> still_alive
```

| Operator | Description |
|----------|-------------|
| `and` | Both conditions must be true |
| `or` | At least one condition must be true |
| `not` | Negate the condition |

## Variable API

The Runner VM and GDExtension expose variable access:

### C++ (Runner)

```cpp
runner.setVariable("hp", Gyeol::Variant::Int(100));
Gyeol::Variant hp = runner.getVariable("hp");
bool exists = runner.hasVariable("hp");
auto names = runner.getVariableNames();
```

### GDScript (StoryPlayer)

```gdscript
story_player.set_variable("hp", 100)
var hp = story_player.get_variable("hp")
var exists = story_player.has_variable("hp")
var names = story_player.get_variable_names()
```
