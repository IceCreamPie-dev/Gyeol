# Quick Start

Write and run your first Gyeol story in 5 minutes.

## Step 1: Write a Script

Create a file called `hello.gyeol`:

```gyeol
label start:
    hero "Hey there! I'm Gyeol."
    hero "Want to go on an adventure?"
    menu:
        "Sure, let's go!" -> accept
        "No thanks." -> decline

label accept:
    hero "Great! Let's head out together!"

label decline:
    hero "Okay... maybe next time."
```

## Step 2: Compile

```bash
GyeolCompiler hello.gyeol -o hello.gyb
```

## Step 3: Play

```bash
GyeolTest hello.gyb
```

You'll see interactive dialogue in the console:

```
[hero] Hey there! I'm Gyeol.
> (press Enter)
[hero] Want to go on an adventure?
> (press Enter)
[1] Sure, let's go!
[2] No thanks.
> 1
[hero] Great! Let's head out together!
--- END ---
```

## Adding Variables

```gyeol
$ courage = 0

label start:
    "You wake up in a dark forest."
    hero "Where am I...?"
    menu:
        "Look around." -> explore
        "Run away." -> flee

label explore:
    hero "I'll be brave and explore."
    $ courage = 1
    jump encounter

label flee:
    hero "Too scary!"
    jump encounter

label encounter:
    "A giant wolf appears!"
    if courage == 1 -> brave else coward

label brave:
    hero "I won't back down!"
    "The wolf, impressed by your courage, steps aside."

label coward:
    hero "Help! Someone save me!"
    "The wolf takes pity and walks away."
```

## Adding Characters

Define characters with metadata for your game engine:

```gyeol
character hero:
    displayName: "Hero"
    color: "#4CAF50"

character villain:
    displayName: "Dark Lord"
    color: "#F44336"

label start:
    hero "I've come to stop you!"
    villain "You dare challenge me?"
```

## Adding Engine Commands

Use `@` to send commands to your game engine:

```gyeol
label start:
    @ bg "forest.png"
    @ bgm "ambient_forest.ogg"
    hero "What a peaceful place."
    @ sfx "footsteps.wav"
    hero "Wait, I hear something..."
```

## Adding Tags to Dialogue

Attach metadata to dialogue lines for animation, expressions, etc.:

```gyeol
label start:
    hero "Hello there!" #mood:happy #pose:wave
    hero "This is serious." #mood:angry #pose:arms_crossed
    hero "Goodbye." #mood:sad
```

## Using Functions

```gyeol
label start:
    $ greeting = call greet("Hero", 100)
    hero "{greeting}"

label greet(name, hp):
    $ msg = "{if hp > 50}Welcome, strong {name}!{else}Rest well, {name}.{endif}"
    return msg
```

## Next Steps

- [Script Syntax](../scripting/syntax.md) - Complete language reference
- [Variables & Expressions](../scripting/variables-and-expressions.md) - All about variables
- [Godot Integration](godot-integration.md) - Use in Godot 4.3
- [Debugger](../tools/debugger.md) - Debug your stories
