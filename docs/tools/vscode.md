# VS Code Extension

The `gyeol-lang` extension provides full editor support for `.gyeol` files in Visual Studio Code.

## Installation

The extension is located in `editors/vscode/`. Install it by:

1. Open VS Code
2. Go to Extensions sidebar
3. Click "..." menu -> "Install from VSIX..." (if packaged)
4. Or open the `editors/vscode/` folder as a workspace for development

## Features

### Syntax Highlighting

Full TextMate grammar for `.gyeol` files:

- **Keywords** - `label`, `menu`, `jump`, `call`, `if`, `elif`, `else`, `return`, `random`, `import`
- **Strings** - Double-quoted text with escape sequences
- **Comments** - Lines starting with `#`
- **Variables** - `$ variable = value` declarations
- **Commands** - `@` command lines
- **Characters** - Character definitions and dialogue
- **Numbers** - Integer and float literals
- **Operators** - `==`, `!=`, `>`, `<`, `>=`, `<=`, `and`, `or`, `not`

### LSP Integration

Real-time language features powered by `GyeolLSP`:

- **Diagnostics** - Errors and warnings shown as squiggly underlines
- **Autocompletion** - Keywords, labels, variables, built-in functions
- **Go to Definition** - F12 on label names and variables
- **Hover** - Documentation for keywords, labels, parameters
- **Document Symbols** - Outline view with labels and variables

### Debugger Integration

Debug adapter support for `GyeolDebugger`:

- **Breakpoints** - Click in the gutter to set breakpoints
- **Launch configuration** - Run `.gyb` files from the editor

## Configuration

### Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `gyeol.lsp.path` | `string` | `""` | Path to `GyeolLSP` executable |
| `gyeol.debugger.path` | `string` | `""` | Path to `GyeolDebugger` executable |

### Language Configuration

The extension configures:

- **Line comments:** `#`
- **Brackets:** `()`, `""`
- **Auto-closing pairs:** `()`, `""`
- **Indentation rules:**
  - Increase after: `label ... :`, `menu:`, `random:`, `if/elif ... ->`, `else ->`
  - Decrease at: `elif`, `else` lines
- **Folding:** Indent-based

## File Associations

| Extension | Language ID |
|-----------|------------|
| `.gyeol` | `gyeol` |

## Development

To develop the extension:

```bash
cd editors/vscode
npm install
```

The extension source is in `src/extension.ts` and activates when `.gyeol` files are opened. It launches the LSP server as a child process.

## Extension Structure

```
editors/vscode/
  package.json                    # Extension manifest
  language-configuration.json     # Language rules
  syntaxes/
    gyeol.tmLanguage.json         # TextMate grammar
  src/
    extension.ts                  # LSP client connection
```
