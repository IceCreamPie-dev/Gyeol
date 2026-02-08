# Introduction

## What is Gyeol?

Gyeol (결) is an interactive storytelling middleware engine built in C++17. It provides a complete pipeline for writing, compiling, and running branching narratives in games.

The name "결" means "texture" or "grain" in Korean, reflecting the engine's goal of weaving rich narrative threads into any game.

### Design Philosophy

Gyeol combines the best of three popular narrative tools:

| Feature | Inspired By |
|---------|-------------|
| Character-driven dialogue syntax | Ren'Py |
| Structural node/graph system | Ink |
| Engine-agnostic middleware design | Yarn Spinner |

### Key Features

- **Single-script authoring** - Write `.gyeol` text files with Ren'Py-style syntax
- **Binary compilation** - Compile to `.gyb` FlatBuffers format for zero-copy loading
- **Engine-agnostic VM** - The Runner VM is pure C++ with no engine dependencies
- **Expression engine** - Full arithmetic, logic, string interpolation, inline conditions
- **Function system** - Labels as functions with parameters, return values, local scope
- **Visit tracking** - Automatic node visit counting with `visit_count()` / `visited()`
- **Localization** - Auto-generated Line IDs, CSV-based locale overlays
- **Save/Load** - Complete state serialization (variables, call stack, visit counts)
- **Random branching** - Weighted probability branches with deterministic seeding
- **Dev tools** - LSP server, CLI debugger, VS Code extension

### Supported Platforms

| Platform | Status | Binding |
|----------|--------|---------|
| Godot 4.3 | Available | GDExtension (`StoryPlayer` node) |
| Unity | Planned | Native Plugin |
| WebAssembly | Planned | Emscripten build |
| Console (CLI) | Available | `GyeolTest` player |

### How It Works

```
.gyeol script  -->  GyeolCompiler  -->  .gyb binary  -->  Runner VM  -->  Game Engine
  (text)              (parser)          (FlatBuffers)      (C++ VM)        (Godot/Unity/...)
```

1. **Write** a `.gyeol` script with dialogue, choices, variables, and logic
2. **Compile** it to a `.gyb` binary using the CLI compiler
3. **Load** the binary into the Runner VM via your game engine's binding
4. **Play** the story using the event-driven `step()`/`choose()` API

### Next Steps

- [Installation](installation.md) - Build Gyeol from source
- [Quick Start](quick-start.md) - Write your first story
- [Script Syntax](../scripting/syntax.md) - Full language reference
