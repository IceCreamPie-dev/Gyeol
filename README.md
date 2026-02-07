# Gyeol (결)

**Write Once, Run Everywhere Story Engine**

[한국어](README.ko.md)

A high-performance interactive storytelling engine built in C++17. Write a single `.gyeol` script and play it on any game engine — Godot, Unity (planned), or WebAssembly (planned) — with zero modifications.

Combines the staging power of Ren'Py, the structural elegance of Ink, and the flexibility of Yarn Spinner into one portable middleware.

## Features

- **Script-driven narratives** — Ren'Py-style syntax with characters, dialogue, choices, and branching
- **Expression engine** — Variables, arithmetic (`$ hp = hp - 10`), inline conditions (`{if hp > 50}Strong{endif}`)
- **Function system** — Parameterized labels, call/return with values, local scope shadowing
- **Visit tracking** — Automatic node visit counts, `visit_count()` / `visited()` functions
- **Localization** — Auto-generated Line IDs, CSV-based locale overlays
- **Save/Load** — Full state serialization (variables, call stack, visit counts) via FlatBuffers
- **Random branching** — Weighted probability branches with deterministic seeding
- **Zero-copy binary format** — `.gyb` (FlatBuffers) for fast loading
- **Dev tools** — LSP server, CLI debugger, VS Code extension with syntax highlighting
- **Engine bindings** — Godot 4.3 GDExtension (signal-based StoryPlayer node)

## Quick Example

```
label start:
    hero "Hey, I'm Gyeol. Wanna go on an adventure?"
    menu:
        "Sure, let's go!" -> good
        "Nah, I'll pass." -> bad

label good:
    hero "Great! Let's head out together!"

label bad:
    hero "Okay... maybe next time."
```

## Building

### Prerequisites

- **CMake** 3.15+
- **Ninja** build system
- **C++17 compiler** — GCC 8+ / MinGW / Clang 7+
- **Git** (for submodules)

Dependencies (FlatBuffers, Google Test, nlohmann/json) are fetched automatically via CMake FetchContent.

### Build Core + Tools

```bash
git clone --recurse-submodules https://github.com/IceCreamPie-dev/Gyeol.git
cd Gyeol

cmake -B build -G Ninja
cmake --build build
```

This builds: `GyeolCompiler`, `GyeolLSP`, `GyeolDebugger`, `GyeolTest` (console player), and test suites.

### Build GDExtension (Godot)

Requires MSVC (Windows) and [SCons](https://scons.org/):

```bash
cd bindings/godot_extension
scons platform=windows target=template_debug
```

Output: `demo/godot/bin/libgyeol.windows.template_debug.x86_64.dll`

## Usage

### Compile a Script

```bash
# .gyeol text → .gyb binary
GyeolCompiler story.gyeol -o story.gyb

# Export translation CSV
GyeolCompiler story.gyeol --export-strings strings.csv
```

### Play in Console

```bash
GyeolTest story.gyb
```

### Debug Interactively

```bash
GyeolDebugger story.gyb
# Commands: step, continue, break node:pc, locals, print var, where, info node
```

### Godot Integration

1. Compile your story: `GyeolCompiler story.gyeol -o demo/godot/story.gyb`
2. Build the GDExtension (see above)
3. Open `demo/godot/` in Godot 4.3

The `StoryPlayer` node exposes:

| Method | Description |
|--------|-------------|
| `load_story(path)` | Load a `.gyb` file |
| `advance()` | Step forward |
| `select_choice(index)` | Pick a choice |
| `save_state(path)` / `load_state(path)` | Save/Load |
| `get_variable(name)` / `set_variable(name, value)` | Variable access |
| `load_locale(path)` / `clear_locale()` | Localization |

Signals: `dialogue_line`, `choices_presented`, `command_received`, `story_ended`

## Script Syntax

```
import "common.gyeol"            # Import other files

$ health = 100                   # Global variable

label greet(name, title):        # Function with parameters
    narrator "Hello, {name}!"    # String interpolation
    narrator "{if health > 50}You look strong{else}You look weak{endif}"
    @ play_sfx "hello.wav"       # Engine command
    menu:
        "Continue" -> next
        "Leave" -> exit if has_key    # Conditional choice
    if health > 80 -> strong
    elif health > 30 -> normal
    else -> weak
    random:                      # Weighted random branch
        50 -> common_path
        10 -> rare_path
    $ result = call some_func(1, 2)   # Call with return value
    return result + health       # Return from function

label next:
    narrator "Moving on..."
```

## Dev Tools

### VS Code Extension

Located in `editors/vscode/`. Provides:
- Syntax highlighting for `.gyeol` files
- LSP integration (completion, go-to-definition, hover, diagnostics)
- Debugger adapter

### LSP Server

JSON-RPC over stdin/stdout. Features: diagnostics, completion (keywords/labels/variables/built-in functions), go-to-definition, hover, document symbols.

## Testing

318 automated tests via Google Test:

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run directly
./build/src/tests/GyeolTests       # 259 tests (Core + Parser + Runner)
./build/src/tests/GyeolLSPTests    # 59 tests (LSP Analyzer + Server)
```

## Project Structure

```
schemas/gyeol.fbs           # FlatBuffers schema (all data structures)
src/
  gyeol_core/               # Core engine (Story loader, Runner VM)
  gyeol_compiler/           # .gyeol → .gyb compiler + parser
  gyeol_lsp/                # Language Server Protocol server
  gyeol_debugger/           # CLI interactive debugger
  tests/                    # Google Test suites
bindings/
  godot_extension/          # Godot 4.3 GDExtension
editors/
  vscode/                   # VS Code extension (gyeol-lang)
demo/
  godot/                    # Godot demo project
```

## License

[MIT](LICENSE) - IceCreamPie
