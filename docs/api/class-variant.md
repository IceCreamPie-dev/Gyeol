# Variant

**Namespace:** `Gyeol`
**Header:** `#include "gyeol_runner.h"`

Variable value container for the Gyeol runtime.

## Description

`Variant` is a tagged union that stores variable values in the Gyeol runtime. Each variable in a story has a Variant holding its current value.

## Type Enum

```cpp
enum Type { BOOL, INT, FLOAT, STRING, LIST };
```

| Type | C++ Field | Size | Description |
|------|-----------|------|-------------|
| `BOOL` | `b` | `bool` | Boolean value |
| `INT` | `i` | `int32_t` | 32-bit signed integer |
| `FLOAT` | `f` | `float` | 32-bit floating point |
| `STRING` | `s` | `std::string` | UTF-8 string |
| `LIST` | `list` | `std::vector<std::string>` | List of strings |

## Factory Methods

```cpp
static Variant Bool(bool v);
static Variant Int(int32_t v);
static Variant Float(float v);
static Variant String(const std::string& v);
static Variant List(const std::vector<std::string>& v);
static Variant List(std::vector<std::string>&& v);  // Move version
```

## Usage

### Creating Variants

```cpp
auto hp = Variant::Int(100);
auto name = Variant::String("Hero");
auto alive = Variant::Bool(true);
auto speed = Variant::Float(3.14f);
auto items = Variant::List({"sword", "shield"});
```

### Reading Variants

```cpp
Variant v = runner.getVariable("hp");
switch (v.type) {
    case Variant::BOOL:   printf("bool: %s\n", v.b ? "true" : "false"); break;
    case Variant::INT:    printf("int: %d\n", v.i); break;
    case Variant::FLOAT:  printf("float: %f\n", v.f); break;
    case Variant::STRING: printf("str: %s\n", v.s.c_str()); break;
    case Variant::LIST:
        for (auto& item : v.list) printf("- %s\n", item.c_str());
        break;
}
```

### Setting Variables

```cpp
runner.setVariable("hp", Variant::Int(100));
runner.setVariable("name", Variant::String("Hero"));
runner.setVariable("alive", Variant::Bool(true));
```

## Type Mapping

### Gyeol Script to Variant

| Script Value | Variant Type |
|-------------|-------------|
| `true` / `false` | `BOOL` |
| `42`, `-10` | `INT` |
| `3.14` | `FLOAT` |
| `"hello"` | `STRING` |
| `+= "item"` | `LIST` (created by append) |

### Variant to Godot

When used through the GDExtension StoryPlayer:

| Variant Type | Godot Type |
|-------------|------------|
| `BOOL` | `bool` |
| `INT` | `int` |
| `FLOAT` | `float` |
| `STRING` | `String` |
| `LIST` | `Array[String]` |

### Godot to Variant

| Godot Type | Variant Type |
|-----------|-------------|
| `bool` | `BOOL` |
| `int` | `INT` |
| `float` | `FLOAT` |
| `String` | `STRING` |
| `Array` | `LIST` (elements converted to String) |

## Arithmetic Behavior

| Operation | INT + INT | INT + FLOAT | FLOAT + FLOAT |
|-----------|-----------|-------------|---------------|
| `+` | INT | FLOAT | FLOAT |
| `-` | INT | FLOAT | FLOAT |
| `*` | INT | FLOAT | FLOAT |
| `/` | INT (truncated) | FLOAT | FLOAT |
| `%` | INT | N/A | N/A |

## Truthiness

Used in conditions and inline `{if}` expressions:

| Type | Truthy | Falsy |
|------|--------|-------|
| BOOL | `true` | `false` |
| INT | non-zero | `0` |
| FLOAT | non-zero | `0.0` |
| STRING | non-empty | `""` |

## String Conversion

When interpolated in text (`{variable}`), Variants convert to strings:

| Type | Format |
|------|--------|
| BOOL | `"true"` or `"false"` |
| INT | Decimal (e.g., `"42"`) |
| FLOAT | Decimal with fraction (e.g., `"3.140000"`) |
| STRING | As-is |
| LIST | Comma-separated (e.g., `"sword, shield"`) |
