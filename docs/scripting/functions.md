# Functions

Gyeol labels can act as functions with parameters, return values, and local scope.

## Defining Functions

A function is a label with parameters:

```gyeol
label greet(name, title):
    hero "Hello, {title} {name}!"
```

- Parameters are declared in parentheses after the label name
- Parameters behave as local variables within the function
- Multiple parameters are comma-separated

## Calling Functions

### Simple Call

```gyeol
call greet("Hero", "Sir")
```

Arguments are evaluated as expressions at call time.

### Call with Return Value

```gyeol
$ result = call compute(10, 20)
hero "Result: {result}"
```

The return value is stored in the specified variable.

### Expression Arguments

Arguments can be any expression:

```gyeol
$ dmg = call calculate_damage(attack * 2, defense - 5)
```

## Return Values

### Return with Expression

```gyeol
label add(a, b):
    return a + b
```

### Return with Literal

```gyeol
label get_greeting():
    return "Hello!"
```

### Return without Value

```gyeol
label do_something():
    hero "Processing..."
    return     # Early exit, no return value
```

### Implicit Return

When a function reaches its last instruction without an explicit `return`, it returns automatically (with no value).

```gyeol
label side_effect():
    $ counter = counter + 1
    # Implicit return here
```

## Parameter Scope

### Local Scope (Shadowing)

Parameters create a local scope that **shadows** any existing global variable with the same name:

```gyeol
$ name = "Global"

label start:
    hero "Before: {name}"          # "Global"
    call set_name("Local")
    hero "After: {name}"           # "Global" (restored!)

label set_name(name):
    hero "Inside: {name}"          # "Local"
    # When this function returns, 'name' reverts to "Global"
```

How shadowing works:
1. Before the call, the current value of `name` is saved in the call frame
2. The parameter value (`"Local"`) is assigned to `name`
3. The function body runs with the parameter value
4. On return, the saved value (`"Global"`) is restored

### Default Values

Missing arguments default to `Int(0)`:

```gyeol
label damage(amount, multiplier):
    # If called as: call damage(10)
    # amount = 10, multiplier = 0 (default)
    return amount * multiplier

label start:
    $ d = call damage(10)      # multiplier defaults to 0
    $ d = call damage(10, 2)   # Both provided
```

Extra arguments beyond the parameter count are silently ignored.

## Nested Calls

Functions can call other functions:

```gyeol
label start:
    $ msg = call format_greeting("Hero", 100)
    hero "{msg}"

label format_greeting(name, hp):
    $ status = call get_status(hp)
    return "{name}: {status}"

label get_status(hp):
    if hp > 50 -> high_status else low_status

label high_status:
    return "Healthy"

label low_status:
    return "Weak"
```

The call stack tracks each nested call, properly restoring state on each return.

## Jump vs Call

| Feature | `jump` | `call` |
|---------|--------|--------|
| Returns to caller? | No | Yes |
| Uses call stack? | No | Yes |
| Supports parameters? | No | Yes |
| Supports return value? | No | Yes |
| Use case | Scene transitions | Reusable logic |

```gyeol
# Use jump for scene flow
jump next_chapter

# Use call for reusable functions
$ greeting = call format_name("Hero", "Sir")
call play_cutscene
```

> **Note:** `jump` does not support arguments. Use `call` for parameterized invocations.

## Save/Load Compatibility

The call stack, including shadowed variables and parameter names, is fully serialized:
- Saved in `.gys` files (FlatBuffers binary)
- Restored on load with proper scope recovery
- Backward compatible with saves from older versions (no parameters)

## Practical Patterns

### Utility Functions

```gyeol
label clamp_hp(value):
    if value > 100 -> cap_high
    if value < 0 -> cap_low
    return value

label cap_high:
    return 100

label cap_low:
    return 0

label start:
    $ hp = call clamp_hp(hp + heal_amount)
```

### Dialog Helpers

```gyeol
label say_with_effect(character, text):
    @ shake 0.2
    # Note: character/text as variables won't dynamically dispatch
    # This is a simplified pattern
    hero "{text}"
    return
```

### State Machines

```gyeol
label start:
    call game_loop

label game_loop:
    menu:
        "Fight" -> fight_action
        "Heal" -> heal_action
        "Flee" -> flee_action

label fight_action:
    $ hp = hp - 10
    hero "Took 10 damage! HP: {hp}"
    if hp <= 0 -> game_over
    jump game_loop

label heal_action:
    $ hp = hp + 20
    hero "Healed! HP: {hp}"
    jump game_loop

label flee_action:
    hero "You ran away!"
```
