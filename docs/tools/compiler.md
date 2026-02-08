# Compiler CLI

`GyeolCompiler` compiles `.gyeol` text scripts into `.gyb` binary files.

## Usage

```bash
GyeolCompiler <input.gyeol> [options]
```

## Options

| Option | Description |
|--------|-------------|
| `-o <path>` | Output `.gyb` file path (default: `story.gyb`) |
| `--export-strings <path>` | Export translatable strings to CSV |
| `--analyze [path]` | Run static analysis (print to stdout or file) |
| `-O` | Enable optimizations (constant folding, dead code removal) |
| `-h`, `--help` | Show help message |
| `--version` | Show version number |

## Examples

### Basic Compilation

```bash
# Compile to default output (story.gyb)
GyeolCompiler my_story.gyeol

# Specify output path
GyeolCompiler my_story.gyeol -o game/assets/story.gyb
```

### Export Translation Strings

```bash
GyeolCompiler my_story.gyeol --export-strings strings.csv
```

Generates a CSV file with Line IDs and original text for translation:

```csv
id,text
start:0:a3f2,"Hello, how are you?"
start:1:b7c1,"Sure, let's go!"
```

### Static Analysis

```bash
# Print analysis to stdout
GyeolCompiler my_story.gyeol --analyze

# Write analysis to file
GyeolCompiler my_story.gyeol --analyze report.txt
```

## Error Handling

The compiler collects **all** errors (doesn't stop at the first one):

```bash
$ GyeolCompiler broken.gyeol
Error: [line 5] Unknown jump target: 'missing_node'
Error: [line 12] Unknown choice target: 'also_missing'
Error: [line 18] Duplicate label name: 'start'
Compilation failed with 3 errors.
```

### Error Categories

| Category | Example |
|----------|---------|
| Syntax error | Malformed line, missing colon on label |
| Unknown target | Jump/call/choice referencing non-existent label |
| Duplicate label | Two labels with the same name |
| Import error | File not found, circular import |
| Parameter error | Duplicate parameter name, unclosed parentheses |

## Multi-file Compilation

When a script uses `import`, the compiler resolves all imports:

```gyeol
# main.gyeol
import "characters.gyeol"
import "chapter1/intro.gyeol"

label start:
    call intro
```

```bash
# Only specify the main file - imports are resolved automatically
GyeolCompiler main.gyeol -o game.gyb
```

- Import paths are relative to the importing file
- Circular imports are detected and reported
- All files are merged into a single `.gyb`
- The start node is the first label in the main file

## Output Format

The `.gyb` file is a FlatBuffers binary containing:

| Section | Description |
|---------|-------------|
| `version` | Schema version string |
| `string_pool` | All unique text strings (deduplicated) |
| `line_ids` | Translation Line IDs (parallel to string_pool) |
| `global_vars` | Initial variable declarations |
| `nodes` | All story nodes with instructions |
| `start_node_name` | Entry point node name |
| `characters` | Character definitions |

See [Binary Format](../advanced/binary-format.md) for the full schema.

## Line ID Format

Auto-generated Line IDs for localization:

```
{node_name}:{instruction_index}:{hash4}
```

- `node_name` - the containing label name
- `instruction_index` - 0-based instruction position
- `hash4` - 4-digit hex from FNV-1a hash of the text

Only translatable text (dialogue, choice text) gets Line IDs. Structural text (node names, variable names, commands) has empty Line IDs.

## Exit Codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Compilation error or file error |

## Build Location

After building with CMake:

```
build/src/gyeol_compiler/GyeolCompiler
```
