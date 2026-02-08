# Gyeol (결) Documentation

**Write Once, Run Everywhere Story Engine**

Gyeol is a high-performance interactive storytelling engine built in C++17. Write a single `.gyeol` script and play it on any game engine with zero modifications.

---

## Table of Contents

### Getting Started

- [Introduction](getting-started/introduction.md) - What is Gyeol?
- [Installation](getting-started/installation.md) - Build from source
- [Quick Start](getting-started/quick-start.md) - Your first story in 5 minutes
- [Godot Integration](getting-started/godot-integration.md) - Using Gyeol in Godot 4.3

### Scripting

- [Script Syntax](scripting/syntax.md) - Complete `.gyeol` language reference
- [Variables & Expressions](scripting/variables-and-expressions.md) - Variables, arithmetic, logic
- [Flow Control](scripting/flow-control.md) - Branches, conditions, menus, random
- [Functions](scripting/functions.md) - Parameters, call/return, local scope
- [Advanced Features](scripting/advanced-features.md) - Interpolation, inline conditions, tags, choice modifiers

### API Reference

- [StoryPlayer (Godot)](api/class-story-player.md) - GDExtension class reference
- [Runner (C++)](api/class-runner.md) - Core VM class reference
- [Variant](api/class-variant.md) - Variable type system

### Tools

- [Compiler CLI](tools/compiler.md) - `GyeolCompiler` command reference
- [Debugger](tools/debugger.md) - `GyeolDebugger` interactive debugger
- [LSP Server](tools/lsp.md) - Language Server for editors
- [VS Code Extension](tools/vscode.md) - Editor integration

### Advanced

- [Architecture](advanced/architecture.md) - Engine internals overview
- [Save & Load](advanced/save-load.md) - State serialization
- [Localization](advanced/localization.md) - Multi-language support
- [Binary Format](advanced/binary-format.md) - `.gyb` / `.gys` FlatBuffers schema
