# Debugger

`GyeolDebugger` is a CLI interactive debugger for stepping through compiled stories instruction by instruction.

## Usage

```bash
GyeolDebugger <story.gyb> [--help] [--version]
```

## Getting Started

```bash
$ GyeolDebugger story.gyb
[Gyeol Debugger] Loaded story.gyb
[start:0] Line  hero "Hello!"
(gyeol) step
[hero] Hello!
[start:1] Choice  "Yes" -> good
(gyeol)
```

The debugger starts in **step mode** (pauses after each instruction). Type commands at the `(gyeol)` prompt.

## Commands

### Execution Control

| Command | Aliases | Arguments | Description |
|---------|---------|-----------|-------------|
| `step` | `s` | - | Execute one instruction |
| `continue` | `c` | - | Run until breakpoint or END |
| `choose` | `ch` | `INDEX` | Select a choice (0-based) |
| `restart` | `r` | - | Restart from start node |

### Breakpoints

| Command | Aliases | Arguments | Description |
|---------|---------|-----------|-------------|
| `break` | `b` | `NODE [PC]` | Set breakpoint (PC defaults to 0) |
| `delete` | `d` | `[NODE [PC]]` | Remove breakpoint, or clear all |
| `breakpoints` | `bp` | - | List all breakpoints |

### Inspection

| Command | Aliases | Arguments | Description |
|---------|---------|-----------|-------------|
| `locals` | `l` | - | Show all variables with values |
| `print` | `p` | `VARIABLE` | Print a single variable |
| `set` | - | `VARIABLE VALUE` | Set variable (int/float/bool/"string") |
| `where` | `w` | - | Show location + call stack + visit counts |

### Node Information

| Command | Aliases | Arguments | Description |
|---------|---------|-----------|-------------|
| `nodes` | `n` | - | List all nodes with instruction counts |
| `info` | `i` | `NODE` | Show all instructions in a node |

### Other

| Command | Aliases | Arguments | Description |
|---------|---------|-----------|-------------|
| `help` | `h` | - | Show command help |
| `quit` | `q`, `exit` | - | Exit debugger |

## Breakpoints

### Setting Breakpoints

```
(gyeol) break start         # Break at start:0
(gyeol) break encounter 3   # Break at encounter:3
```

### Listing Breakpoints

```
(gyeol) breakpoints
Breakpoints:
  [1] start:0
  [2] encounter:3
```

### Removing Breakpoints

```
(gyeol) delete start 0      # Remove specific breakpoint
(gyeol) delete              # Clear all breakpoints
```

### Hitting Breakpoints

```
(gyeol) continue
... (running)
[Breakpoint hit] encounter:3
[encounter:3] Condition  hp > 0 -> brave else coward
(gyeol)
```

## Step Mode

The debugger starts with step mode enabled. Each `step` command executes exactly one instruction:

```
(gyeol) step
[start:0] Line  hero "Hello!"

(gyeol) step
[hero] Hello!
[start:1] Jump  -> encounter
```

Use `continue` to run freely until a breakpoint or END:

```
(gyeol) continue
... (executing multiple instructions)
--- END ---
```

## Variable Inspection

### View All Variables

```
(gyeol) locals
Variables:
  hp = 100 (Int)
  name = "Hero" (String)
  has_key = true (Bool)
  speed = 3.14 (Float)
```

### View Single Variable

```
(gyeol) print hp
hp = 100 (Int)
```

### Modify Variables

```
(gyeol) set hp 50
hp = 50

(gyeol) set name "Warrior"
name = "Warrior"

(gyeol) set has_key false
has_key = false
```

## Location & Call Stack

### Current Location

```
(gyeol) where
Location: encounter:3 [Condition]
Call stack:
  [0] start:5 (call)
  [1] encounter:3 (current)
Visit counts:
  start: 1
  encounter: 1
```

### Node Inspection

```
(gyeol) nodes
Nodes:
  start (5 instructions)
  encounter (4 instructions)
  brave (2 instructions)
  coward (2 instructions)
```

```
(gyeol) info encounter
encounter (4 instructions):
  [0] Line  "A giant wolf appears!"
  [1] Command  sfx "wolf_growl.wav"
  [2] Line  hero "What do we do?"
  [3] Condition  courage == 1 -> brave else coward
```

## Workflow Example

```bash
$ GyeolDebugger story.gyb

# Set a breakpoint at the encounter
(gyeol) break encounter

# Run to the breakpoint
(gyeol) continue
[Breakpoint hit] encounter:0

# Inspect state
(gyeol) locals
  courage = 1 (Int)

# Step through
(gyeol) step
[encounter:0] Line  "A giant wolf appears!"

(gyeol) step
"A giant wolf appears!"
[encounter:1] Condition  courage == 1 -> brave else coward

# Modify variable to test different branch
(gyeol) set courage 0
(gyeol) step
[coward:0] Line  hero "Help!"

# Check where we are
(gyeol) where
Location: coward:0 [Line]
```

## ANSI Colors

The debugger uses colored output:

| Color | Usage |
|-------|-------|
| Red | Errors |
| Green | Success messages |
| Cyan | Node names and labels |
| Yellow | Emphasis and values |
| Dim | Status information |

## Build Location

After building with CMake:

```
build/src/gyeol_debugger/GyeolDebugger
```
