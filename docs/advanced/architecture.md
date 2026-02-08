# Architecture

## Overview

Gyeol is a middleware engine with a clear pipeline:

```
  .gyeol Script
       |
  [Parser] ──── gyeol_parser.cpp
       |
  FlatBuffers Object API (StoryT, NodeT, ...)
       |
  [Compiler] ── Pack() → .gyb binary
       |
  [Runner VM] ─ gyeol_runner.cpp (zero-copy read)
       |
  [Binding] ─── GDExtension / Unity / WASM
       |
  Game Engine
```

## Core Components

### Parser (`gyeol_parser.cpp`)

The parser is a line-by-line, indent-based text processor:

- **Input:** `.gyeol` text files
- **Output:** FlatBuffers Object API types (`StoryT`, `NodeT`, `InstructionT`)
- **Approach:** Single-pass with post-validation

Key design decisions:
- **Indent-based** (0/4/8 spaces) rather than brace-based
- **Error recovery** - collects all errors instead of stopping at the first
- **Multi-file support** - `import` merges files with cycle detection
- **Expression parsing** - Shunting-yard algorithm converts infix to RPN

### String Pool

All text in the compiled binary lives in a central string pool:

```
string_pool: ["hero", "Hello!", "cave", "Go left", ...]
```

Other structures store integer indices into this pool:

```
Line { character_id: 0, text_id: 1 }  // hero says "Hello!"
Choice { text_id: 3, target_node_name_id: 2 }  // "Go left" -> cave
```

Benefits:
- **Deduplication** via `unordered_map` during compilation
- **Localization** by overlaying translated strings at the same indices
- **Memory efficiency** with zero-copy FlatBuffers access

### Runner VM (`gyeol_runner.cpp`)

The Runner is a virtual machine that interprets compiled story data:

- **Input:** `.gyb` FlatBuffers binary (zero-copy read)
- **API:** Event-driven `step()` / `choose()`
- **State:** Program counter, variables, call stack, visit counts

#### Execution Model

```
           step()
             |
     [Read Instruction]
             |
    +--------+--------+--------+--------+
    |        |        |        |        |
   LINE   CHOICE   COMMAND    JUMP    OTHER
    |        |        |        |     (SetVar, Condition,
  emit    collect   emit    jump     Random, Return, ...)
  signal   choices  signal  to node    |
    |        |        |        |    [internal,
    |     emit       |        |     continue]
    |     signal     |        |
    v        v        v        v
  StepResult returned to caller
```

#### StepResult Types

| Type | Meaning | Caller Action |
|------|---------|---------------|
| `LINE` | Dialogue/narration to display | Show text, call `step()` |
| `CHOICES` | Menu presented | Show buttons, call `choose(index)` |
| `COMMAND` | Engine command | Handle command, call `step()` |
| `END` | Story finished | Stop |

Internal instructions (SetVar, Jump, Condition, Random) execute silently and continue to the next visible instruction.

#### Choice Filtering Pipeline

When a menu is encountered, choices are filtered through this pipeline:

```
Raw choices from bytecode
    |
[1. Condition filter] ── hide choices whose condition var is false
    |
[2. Once filter] ────── hide choices with once_key in chosenOnceChoices_
    |
[3. Modifier split] ─── separate into normal vs fallback
    |
[4. Fallback logic] ─── if normal.empty(), use fallback
    |
Final choices → StepResult
```

#### Expression Evaluation

Expressions use a stack-based RPN evaluator:

```
Input:  hp * 2 + bonus
RPN:    [PushVar(hp), PushLiteral(2), Mul, PushVar(bonus), Add]

Stack trace:
  Push hp(100)     → [100]
  Push 2           → [100, 2]
  Mul              → [200]
  Push bonus(10)   → [200, 10]
  Add              → [210]
  Result: 210
```

#### Call Stack

Function calls use a call stack with scope management:

```
CallFrame
  node: pointer to called node
  pc: return address (instruction after the call)
  returnVarName: variable for return value
  shadowedVars: saved global values overridden by parameters
  paramNames: parameter names for cleanup
```

When a function returns:
1. Pop the call frame
2. Restore shadowed variables to their pre-call values
3. If `returnVarName` is set, store the return value
4. Resume at the saved `pc` in the caller's node

### FlatBuffers

Gyeol uses FlatBuffers for both story data and save files:

| API | When | Types |
|-----|------|-------|
| **Object API** | Building/writing | `StoryT`, `NodeT`, `ChoiceT`, etc. |
| **Zero-copy API** | Reading/running | `Story*`, `Node*`, via `GetRoot<>()` |

The Object API uses `T`-suffix types (mutable structs) for construction. The zero-copy API reads directly from the binary buffer without deserialization.

### GDExtension Binding

The Godot binding wraps the C++ Runner in a `StoryPlayer` node:

```
StoryPlayer (Node)
  |
  |── Runner runner_
  |── vector<uint8_t> buffer_
  |
  |── load_story() ──→ FileAccess::open → buffer_
  |── start()      ──→ runner_.start()
  |── advance()    ──→ runner_.step() → emit_signal()
  |── choose()     ──→ runner_.choose() → advance()
  |
  |── Signals:
  |     dialogue_line(character, text, tags)
  |     choices_presented(choices)
  |     command_received(type, params)
  |     story_ended()
```

Key design:
- Source files compiled directly into the extension DLL (not linked as library)
- FlatBuffers headers referenced from CMake build directory
- Godot paths (`res://`, `user://`) converted to system paths via `FileAccess`
- All Gyeol types map to Godot Variants (Int→int, String→String, List→Array)

## Data Flow

### Compilation

```
.gyeol text
    |
  Parser.parse()
    |
  StoryT (Object API)
    |         \
  Parser.compile()    Parser.exportStrings()
    |                      |
  .gyb binary          strings.csv
```

### Runtime

```
.gyb binary                  .csv locale
    |                            |
  Runner.start()            Runner.loadLocale()
    |                            |
  [Zero-copy read]          [Overlay pool]
    |                            |
  Runner.step()  ←──── poolStr() checks overlay first
    |
  StepResult → Game Engine
```

### Save/Load

```
Runner state ──→ SaveState (Object API) ──→ Pack() ──→ .gys binary
                                                           |
Runner state ←── Parse .gys ←── GetRoot<SaveState>() ←────┘
```

## Build Architecture

### CMake Targets

```
GyeolCore ────── Core library (Story + Runner)
    |
GyeolParser ─── Parser library (shared by Compiler + Tests)
    |
    ├── GyeolCompiler ── CLI compiler
    ├── GyeolTest ────── Console player
    ├── GyeolTests ───── Unit tests
    ├── GyeolLSP ─────── LSP server
    └── GyeolDebugger ── CLI debugger

SCons (separate):
    libgyeol.dll ─────── GDExtension (includes GyeolCore sources directly)
```

### Dependencies

```
FlatBuffers v24.3.25 ─── Binary serialization (FetchContent)
Google Test v1.14.0 ──── Unit testing (FetchContent)
nlohmann/json v3.11.3 ── LSP server only (FetchContent)
godot-cpp 4.3 ────────── GDExtension binding (git submodule)
```

## Design Principles

1. **Engine-agnostic** - The Runner has zero dependencies on any game engine
2. **Event-driven** - `step()`/`choose()` API, not frame-based
3. **Zero-copy** - FlatBuffers for fast binary loading
4. **Deterministic** - Seedable RNG, reproducible execution
5. **Backward compatible** - Save files work across versions
6. **Error-resilient** - Parser collects all errors, doesn't stop at first
