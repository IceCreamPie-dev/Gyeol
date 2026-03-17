# Compiler CLI

`GyeolCompiler` compiles `.gyeol` text scripts into `.gyb` binary files.

## Usage

```bash
GyeolCompiler <input.gyeol> [options]
GyeolCompiler <input.gyeol> --export-graph-json <output.graph.json>
GyeolCompiler <input.gyeol> --validate-graph-patch <patch.json>
GyeolCompiler <input.gyeol> --apply-graph-patch <patch.json> -o <output.gyeol>
```

## Options

| Option | Description |
|--------|-------------|
| `-o <path>` | Output `.gyb` file path (default: `story.gyb`) |
| `--export-strings <path>` | Export translatable strings to CSV |
| `--export-strings-po <path>` | Export translatable strings to POT (`msgctxt = line_id`) |
| `--po-to-json <path>` | Convert PO translations to runtime locale JSON |
| `--locale <code>` | Locale code override for `--po-to-json` |
| `--export-graph-json <path>` | Export graph contract JSON (`gyeol-graph-doc`, v1) |
| `--validate-graph-patch <path>` | Validate graph patch JSON (`gyeol-graph-patch`, v1/v2) |
| `--apply-graph-patch <path>` | Apply graph patch (v1/v2) and emit canonical `.gyeol` |
| `--preserve-line-id` | With `--apply-graph-patch`, emit `<output>.lineidmap.json` |
| `--line-id-map <path>` | Apply `gyeol-line-id-map` v1 during compile |
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

```bash
GyeolCompiler my_story.gyeol --export-strings-po strings.pot
```

Generates a POT template where each entry uses `msgctxt` as Gyeol `line_id`.

### PO to Runtime JSON

```bash
GyeolCompiler --po-to-json strings_ko.po -o ko.locale.json --locale ko
```

Output JSON schema:

```json
{
  "version": 1,
  "locale": "ko",
  "entries": {
    "start:0:a3f2": "안녕하세요!"
  }
}
```

### Static Analysis

```bash
# Print analysis to stdout
GyeolCompiler my_story.gyeol --analyze

# Write analysis to file
GyeolCompiler my_story.gyeol --analyze report.txt
```

### Graph Contract / Patch Workflow

```bash
# Export graph doc
GyeolCompiler my_story.gyeol --export-graph-json story.graph.json

# Validate patch
GyeolCompiler my_story.gyeol --validate-graph-patch patch.json

# Apply patch (atomic: no output written on failure)
GyeolCompiler my_story.gyeol --apply-graph-patch patch.json -o story.patched.gyeol

# Apply patch + preserve existing line_id for unchanged instructions
GyeolCompiler my_story.gyeol --apply-graph-patch patch.json --preserve-line-id -o story.patched.gyeol

# Recompile patched script with preserved IDs
GyeolCompiler story.patched.gyeol --line-id-map story.patched.gyeol.lineidmap.json -o story.patched.gyb
```

Graph doc header:

```json
{
  "format": "gyeol-graph-doc",
  "version": 1,
  "start_node": "start"
}
```

`gyeol-graph-doc` includes deterministic `edge_id` and per-node `instructions[]` entries with stable snapshot IDs (`instruction_id`, e.g. `n0:i3`) for v2 patching.

Graph patch header:

```json
{
  "format": "gyeol-graph-patch",
  "version": 2,
  "ops": []
}
```

Supported patch ops in v1:
- `add_node`
- `rename_node` (updates all references automatically)
- `delete_node` (`redirect_target` required)
- `retarget_edge` (`edge_id` required)
- `set_start_node`

Additional ops in v2:
- `update_line_text`
- `update_choice_text`
- `update_command`
- `update_expression`
- `insert_instruction`
- `delete_instruction`
- `move_instruction`

Instruction reference rule in v2:
- `instruction_id` is resolved against the patch-start snapshot.
- Instructions inserted by the same patch cannot be referenced by `instruction_id` in that patch.

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
