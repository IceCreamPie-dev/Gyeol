# Flow Control

## Jump

A one-way transfer to another node. Execution continues from the target node and does **not** return.

```gyeol
jump target_node
```

### Example

```gyeol
label start:
    hero "Let's go!"
    jump next_scene

label next_scene:
    hero "We've arrived."
```

## Call / Return

Call pushes the current position onto a **call stack** and jumps to the target node. When the target node ends (or executes `return`), execution returns to the instruction after the `call`.

### Basic Call

```gyeol
label start:
    call greeting
    hero "Back from greeting."

label greeting:
    hero "Hello there!"
    # Automatically returns when the node ends
```

### Explicit Return

```gyeol
label helper:
    hero "Doing something..."
    return      # Explicitly return (skips remaining instructions)
```

### Call with Return Value

```gyeol
label start:
    $ result = call compute(10, 20)
    hero "The answer is {result}."

label compute(a, b):
    return a + b
```

The `return` expression is evaluated and stored in the variable specified by `$ var = call ...`.

## Conditions

### Simple Condition

```gyeol
if hp > 50 -> strong_path else weak_path
```

- **left operand** - variable name or expression
- **operator** - `==`, `!=`, `>`, `<`, `>=`, `<=`
- **right operand** - literal value or expression
- **true branch** - jump target when condition is true
- **false branch** (optional) - jump target when condition is false

### Truthiness Check

```gyeol
if has_key -> unlock_door
```

When there's no operator, the variable is checked for "truthiness":
- Bool: true/false
- Int: non-zero is true
- Float: non-zero is true
- String: non-empty is true

### Expression Conditions

Both sides can be full expressions:

```gyeol
if hp - 10 > 0 -> survive else die
if attack * 2 + bonus >= defense -> hit
```

### Logical Operators

```gyeol
if hp > 0 and has_weapon == true -> fight
if is_tired or is_hungry -> rest
if not is_dead -> alive
```

Logical operators can combine any conditions:

```gyeol
if hp > 50 and gold > 100 and visited("shop") -> wealthy_warrior
if not has_key or not has_torch -> cannot_enter
```

### Elif / Else Chains

```gyeol
if score >= 90 -> grade_a
elif score >= 80 -> grade_b
elif score >= 70 -> grade_c
else -> grade_f
```

- `elif` must follow `if` or another `elif`
- `else` must follow `if` or `elif`
- Conditions are evaluated top-to-bottom; first match wins
- Compiled as chained Condition + Jump instructions (no schema changes needed)

### Visit-based Conditions

```gyeol
if visited("secret_room") -> knows_secret
if visit_count("shop") > 5 -> loyal_customer
```

## Menu (Choices)

Present choices to the player. Execution pauses until the player selects one.

```gyeol
menu:
    "Choice A" -> node_a
    "Choice B" -> node_b
    "Choice C" -> node_c
```

### Conditional Choices

```gyeol
menu:
    "Use key" -> unlock if has_key
    "Force the door" -> force if strength > 10
    "Walk away" -> leave
```

Only choices whose conditions are met are shown to the player.

### Choice Modifiers

```gyeol
menu:
    "Buy healing potion" -> buy_heal #once
    "Buy mana potion" -> buy_mana #once
    "Browse wares" -> browse #sticky
    "Leave" -> exit #fallback
```

| Modifier | Behavior |
|----------|----------|
| *(default)* | Shown whenever its condition passes |
| `#once` | Hidden permanently after being selected once |
| `#sticky` | Always shown when condition passes (explicit default) |
| `#fallback` | Only shown when ALL other choices in the same menu are hidden |

**Combined with conditions:**

```gyeol
menu:
    "VIP offer" -> vip_deal if is_vip #once
    "Regular deal" -> regular #once
    "Leave" -> exit #fallback
```

Both orderings are valid: `if condition #modifier` and `#modifier if condition`.

**Fallback behavior:**
1. Evaluate all choices: apply conditions + once filtering
2. Separate into normal and fallback groups
3. If any normal choices remain visible: show only normal choices
4. If all normal choices hidden: show fallback choices
5. If no choices at all: return empty array (game engine handles)

**Once tracking:**
- Each once-choice is tracked by a unique key (`nodeName:pc`)
- Once-tracking state persists through save/load
- Reset when the story is restarted with `start()`

## Random Branching

Weighted random selection:

```gyeol
random:
    50 -> common_path       # 50/(50+30+10+1) = ~55% chance
    30 -> uncommon_path     # 30/91 = ~33% chance
    10 -> rare_path         # 10/91 = ~11% chance
    -> ultra_rare           # 1/91 = ~1% chance (default weight = 1)
```

- Each branch has a **weight** (positive integer, default 1)
- Probability = weight / total_weights
- Weight 0 means never selected
- Uses `std::mt19937` RNG; set seed with `setSeed()` for deterministic testing

## Flow Control Summary

| Instruction | Syntax | Returns? | Stack? |
|-------------|--------|----------|--------|
| Jump | `jump node` | No | No |
| Call | `call node` | Yes | Push |
| Call (return value) | `$ v = call node` | Yes | Push |
| Return | `return [expr]` | N/A | Pop |
| Condition | `if cond -> node [else node]` | No | No |
| Elif | `elif cond -> node` | No | No |
| Else | `else -> node` | No | No |
| Menu | `menu: choices...` | Pauses | No |
| Random | `random: branches...` | No | No |
