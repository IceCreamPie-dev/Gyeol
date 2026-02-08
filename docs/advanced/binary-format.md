# Binary Format

Gyeol uses [FlatBuffers](https://google.github.io/flatbuffers/) for its binary formats.

## File Types

| Extension | Format | Root Type | Description |
|-----------|--------|-----------|-------------|
| `.gyb` | Gyeol Binary | `Story` | Compiled story data |
| `.gys` | Gyeol Save | `SaveState` | Saved game state |

Both formats are zero-copy FlatBuffers binaries for fast loading.

## Schema Location

```
schemas/gyeol.fbs
```

The schema defines all data types for both `.gyb` and `.gys` files. FlatBuffers C++ headers are auto-generated during the CMake build.

## Story Format (.gyb)

Root type: `Story`

```
Story
  version: string                 # Schema version
  string_pool: [string]           # All unique text strings
  line_ids: [string]              # Translation Line IDs (parallel to string_pool)
  global_vars: [SetVar]           # Initial variable declarations
  nodes: [Node]                   # Story nodes
  start_node_name: string         # Entry point node name
  characters: [CharacterDef]      # Character definitions
```

### String Pool

All text in the story is stored in a central string pool. Other structures reference text by integer index into this pool. Benefits:
- **Deduplication** - identical strings stored once
- **Localization** - locale overlay replaces strings by index
- **Memory efficiency** - zero-copy access via FlatBuffers

### Node

```
Node
  name: string (key)              # Node name (unique identifier)
  lines: [Instruction]            # Instructions to execute
  param_ids: [int]                # Parameter name indices (for functions)
  tags: [Tag]                     # Metadata tags (#key=value)
```

### Instruction Types (OpData union)

| Type | Fields | Description |
|------|--------|-------------|
| `Line` | character_id, text_id, voice_asset_id, tags | Dialogue or narration |
| `Choice` | text_id, target_node_name_id, condition_var_id, choice_modifier | Menu option |
| `Jump` | target_node_name_id, is_call, arg_exprs | Flow transfer |
| `Command` | type_id, params | Engine command |
| `SetVar` | var_name_id, value, expr, assign_op | Variable assignment |
| `Condition` | var_name_id, op, compare_value, true/false jumps, expressions | Branch |
| `Random` | branches | Weighted random selection |
| `Return` | expr, value | Function return |
| `CallWithReturn` | target_node_name_id, return_var_name_id, arg_exprs | Call + store result |

### Value Types (ValueData union)

| Type | Table | Field |
|------|-------|-------|
| Bool | `BoolValue` | `val: bool` |
| Int | `IntValue` | `val: int` |
| Float | `FloatValue` | `val: float` |
| String | `StringRef` | `index: int` (string pool index) |
| List | `ListValue` | `items: [int]` (string pool indices) |

### Expression System

Expressions are stored as RPN (Reverse Polish Notation) token lists:

```
Expression
  tokens: [ExprToken]

ExprToken
  op: ExprOp                      # Operation type
  literal_value: ValueData        # For PushLiteral
  var_name_id: int                # For PushVar, PushVisitCount, etc.
```

ExprOp types:

| Category | Operations |
|----------|-----------|
| Stack | `PushLiteral`, `PushVar` |
| Arithmetic | `Add`, `Sub`, `Mul`, `Div`, `Mod`, `Negate` |
| Comparison | `CmpEq`, `CmpNe`, `CmpGt`, `CmpLt`, `CmpGe`, `CmpLe` |
| Logic | `And`, `Or`, `Not` |
| Functions | `PushVisitCount`, `PushVisited` |
| List | `ListContains`, `ListLength` |

### Tag

```
Tag
  key_id: int                     # String pool index for key
  value_id: int                   # String pool index for value
```

Used for both dialogue tags (`#mood:angry`) and node tags (`#difficulty=hard`).

### Character Definition

```
CharacterDef
  name_id: int                    # Character ID (string pool index)
  properties: [Tag]               # Key-value properties
```

### Choice Modifier

```
enum ChoiceModifier : byte {
    Default = 0,
    Once = 1,
    Sticky = 2,
    Fallback = 3
}
```

## Save Format (.gys)

Root type: `SaveState`

```
SaveState
  version: string                 # Save format version
  story_version: string           # Original story version
  current_node_name: string       # Active node
  pc: uint32                      # Program counter
  finished: bool                  # End flag
  variables: [SavedVar]           # Runtime variables
  call_stack: [SavedCallFrame]    # Function call frames
  pending_choices: [SavedPendingChoice]  # Active menu
  visit_counts: [SavedVisitCount]        # Visit data
  chosen_once_choices: [string]          # Once-choice keys
```

### SavedVar

```
SavedVar
  name: string                    # Variable name
  value: ValueData                # Type + value
  string_value: string            # String type actual value
  list_items: [string]            # List type items
```

### SavedCallFrame

```
SavedCallFrame
  node_name: string               # Frame's node
  pc: uint32                      # Return address
  return_var_name: string         # Variable for return value
  shadowed_vars: [SavedShadowedVar]  # Saved parameter overrides
  param_names: [string]           # Parameter names
```

### SavedPendingChoice

```
SavedPendingChoice
  text: string                    # Choice display text
  target_node_name: string        # Jump target
  choice_modifier: ChoiceModifier # Modifier type
```

### SavedVisitCount

```
SavedVisitCount
  node_name: string               # Node name
  count: uint32                   # Visit count
```

## Building from Schema

FlatBuffers headers are generated automatically during CMake build via FetchContent. To regenerate manually:

```bash
flatc --cpp schemas/gyeol.fbs -o src/gyeol_core/include/generated/
```

## FlatBuffers Object API

The codebase uses two APIs:

| API | Usage | Pattern |
|-----|-------|---------|
| **Object API** (`*T` types) | Building/writing | `StoryT`, `NodeT`, `InstructionT` |
| **Zero-copy API** | Reading | `GetRoot<Story>()`, pointer access |

The Parser builds using Object API types, then calls `Pack()` to serialize. The Runner reads using zero-copy pointers for maximum performance.
