# LSP Server

`GyeolLSP` is a Language Server Protocol server for `.gyeol` files. It provides editor features like autocompletion, diagnostics, and go-to-definition.

## Usage

The LSP server communicates via JSON-RPC over stdin/stdout. It is typically launched automatically by editor extensions.

```bash
GyeolLSP
```

## Capabilities

### Diagnostics

Real-time error reporting as you type. The LSP runs the Gyeol parser on every file change and reports errors:

- Syntax errors
- Unknown jump/call/choice targets
- Duplicate label names
- Import errors
- Parameter errors

### Completion

Context-aware autocompletion:

| Context | Suggestions |
|---------|------------|
| Keywords | `label`, `menu`, `jump`, `call`, `if`, `elif`, `else`, `random`, `return`, `import` |
| After `jump` / `call` | All label names in the project |
| After `->` | All label names |
| After `$` or `if` | All variable names |
| In expressions | `visit_count()`, `visited()`, `list_contains()`, `list_length()` |

### Go to Definition

Navigate to definitions with Ctrl+Click or F12:

| Element | Navigates To |
|---------|-------------|
| Label name (in jump/call/choice) | Label declaration line |
| Variable name | First assignment or global declaration |

### Hover

Hover over elements for documentation:

| Element | Information |
|---------|------------|
| Keywords | Brief description of syntax |
| Label name | Signature with parameters |
| Variable | Scope information (global/local) |

### Document Symbols

Outline view showing:

| Symbol Type | Elements |
|-------------|----------|
| Function | Label definitions |
| Variable | Variable declarations |

## VS Code Integration

The VS Code extension (`editors/vscode/`) automatically connects to `GyeolLSP`.

### Configuration

In VS Code settings (`settings.json`):

```json
{
    "gyeol.lsp.path": "path/to/GyeolLSP"
}
```

### Features in VS Code

- **Syntax highlighting** - `.gyeol` files are colored automatically
- **Error underlines** - Red squiggles for syntax errors
- **Autocomplete** - Triggered automatically or with Ctrl+Space
- **Go to Definition** - F12 or Ctrl+Click on labels/variables
- **Hover** - Mouse over keywords, labels, variables
- **Outline** - View all labels and variables in the sidebar
- **Breakpoints** - Click in the gutter to set breakpoints (for debugger)

## Protocol Details

The LSP server implements these methods:

| Method | Direction | Description |
|--------|-----------|-------------|
| `initialize` | Client -> Server | Negotiate capabilities |
| `shutdown` | Client -> Server | Prepare for exit |
| `exit` | Client -> Server | Terminate server |
| `textDocument/didOpen` | Client -> Server | File opened (triggers diagnostics) |
| `textDocument/didChange` | Client -> Server | File changed (triggers diagnostics) |
| `textDocument/didClose` | Client -> Server | File closed |
| `textDocument/publishDiagnostics` | Server -> Client | Send error/warning list |
| `textDocument/completion` | Client -> Server | Request completions |
| `textDocument/definition` | Client -> Server | Go to definition |
| `textDocument/hover` | Client -> Server | Hover information |
| `textDocument/documentSymbol` | Client -> Server | Document outline |

## Dependencies

- **nlohmann/json** v3.11.3 (fetched via CMake FetchContent)

## Build Location

After building with CMake:

```
build/src/gyeol_lsp/GyeolLSP
```
