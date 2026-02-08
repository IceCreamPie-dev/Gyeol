# Runner (C++ API)

**Namespace:** `Gyeol`
**Header:** `#include "gyeol_runner.h"`

The core virtual machine that executes compiled Gyeol stories.

## Description

`Runner` is the engine-agnostic VM that interprets compiled `.gyb` story binaries. It provides an event-driven `step()`/`choose()` API that can be integrated into any game engine or application.

The Runner manages:
- Story execution flow (program counter, current node)
- Variable state
- Call stack (for function calls)
- Choice filtering (conditions, once/sticky/fallback modifiers)
- Visit tracking
- Random number generation
- Save/load serialization

## Tutorials

- [Quick Start](../getting-started/quick-start.md)
- [Architecture](../advanced/architecture.md)

---

## Types

### Variant

```cpp
struct Variant {
    enum Type { BOOL, INT, FLOAT, STRING, LIST };
    Type type;
    // Access via: b (bool), i (int32_t), f (float), s (string), list (vector<string>)
};
```

Variable value container. Create using static factory methods:

```cpp
Variant::Bool(true)
Variant::Int(42)
Variant::Float(3.14f)
Variant::String("hello")
Variant::List({"item1", "item2"})
```

See [Variant](class-variant.md) for details.

---

### StepType

```cpp
enum class StepType { LINE, CHOICES, COMMAND, END };
```

| Value | Description |
|-------|-------------|
| `LINE` | A dialogue or narration line |
| `CHOICES` | A menu with choices (call `choose()` to continue) |
| `COMMAND` | An engine command (call `step()` to continue) |
| `END` | Story has finished |

---

### StepResult

```cpp
struct StepResult {
    StepType type;
    LineData line;                    // Valid when type == LINE
    std::vector<ChoiceData> choices;  // Valid when type == CHOICES
    CommandData command;              // Valid when type == COMMAND
};
```

Returned by `step()`. Check `type` to determine which field to read.

---

### LineData

```cpp
struct LineData {
    const char* character;  // nullptr for narration
    const char* text;
    std::vector<std::pair<const char*, const char*>> tags;  // key-value metadata
};
```

---

### ChoiceData

```cpp
struct ChoiceData {
    const char* text;
    int index;
};
```

---

### CommandData

```cpp
struct CommandData {
    const char* type;
    std::vector<const char*> params;
};
```

---

## Methods

### Core Execution

| Return | Method |
|--------|--------|
| `bool` | [start](#start)`(const uint8_t* buffer, size_t size)` |
| `StepResult` | [step](#step)`()` |
| `void` | [choose](#choose)`(int index)` |
| `bool` | [isFinished](#isfinished)`() const` |
| `bool` | [hasStory](#hasstory)`() const` |

### Variable Access

| Return | Method |
|--------|--------|
| `Variant` | [getVariable](#getvariable)`(const std::string& name) const` |
| `void` | [setVariable](#setvariable)`(const std::string& name, const Variant& value)` |
| `bool` | [hasVariable](#hasvariable)`(const std::string& name) const` |
| `std::vector<std::string>` | [getVariableNames](#getvariablenames)`() const` |

### Save / Load

| Return | Method |
|--------|--------|
| `bool` | [saveState](#savestate)`(const std::string& filepath) const` |
| `bool` | [loadState](#loadstate)`(const std::string& filepath)` |

### Locale

| Return | Method |
|--------|--------|
| `bool` | [loadLocale](#loadlocale)`(const std::string& csvPath)` |
| `void` | [clearLocale](#clearlocale)`()` |
| `std::string` | [getLocale](#getlocale)`() const` |

### Visit Tracking

| Return | Method |
|--------|--------|
| `int32_t` | [getVisitCount](#getvisitcount)`(const std::string& nodeName) const` |
| `bool` | [hasVisited](#hasvisited)`(const std::string& nodeName) const` |

### Characters

| Return | Method |
|--------|--------|
| `std::string` | [getCharacterProperty](#getcharacterproperty)`(const std::string& characterId, const std::string& key) const` |
| `std::vector<std::string>` | [getCharacterNames](#getcharacternames)`() const` |
| `std::string` | [getCharacterDisplayName](#getcharacterdisplayname)`(const std::string& characterId) const` |

### Node Tags

| Return | Method |
|--------|--------|
| `std::string` | [getNodeTag](#getnodetag)`(const std::string& nodeName, const std::string& key) const` |
| `std::vector<std::pair<std::string, std::string>>` | [getNodeTags](#getnodetags)`(const std::string& nodeName) const` |
| `bool` | [hasNodeTag](#hasnodetag)`(const std::string& nodeName, const std::string& key) const` |

### RNG

| Return | Method |
|--------|--------|
| `void` | [setSeed](#setseed)`(uint32_t seed)` |

---

## Method Descriptions

### start

```cpp
bool start(const uint8_t* buffer, size_t size)
```

Initializes the Runner with a compiled `.gyb` buffer. Parses the FlatBuffers data, initializes global variables, builds caches (characters, node tags), resets visit counts and once-choice tracking, and jumps to the start node.

Returns `true` on success.

```cpp
std::vector<uint8_t> data = loadFile("story.gyb");
Gyeol::Runner runner;
if (runner.start(data.data(), data.size())) {
    // Ready to step()
}
```

---

### step

```cpp
StepResult step()
```

Executes the next instruction and returns a `StepResult`. The result's `type` field determines how to proceed:

| Type | Action |
|------|--------|
| `LINE` | Display dialogue, then call `step()` again |
| `CHOICES` | Display choices, then call `choose(index)` |
| `COMMAND` | Handle command, then call `step()` again |
| `END` | Story is finished |

```cpp
auto result = runner.step();
switch (result.type) {
    case StepType::LINE:
        printf("[%s] %s\n", result.line.character, result.line.text);
        break;
    case StepType::CHOICES:
        for (auto& c : result.choices) {
            printf("[%d] %s\n", c.index, c.text);
        }
        break;
    // ...
}
```

---

### choose

```cpp
void choose(int index)
```

Selects a choice by 0-based index and automatically advances the story. Must be called after `step()` returns `StepType::CHOICES`.

---

### isFinished

```cpp
bool isFinished() const
```

Returns `true` if the story has reached its end.

---

### hasStory

```cpp
bool hasStory() const
```

Returns `true` if a story buffer has been loaded via `start()`.

---

### getVariable

```cpp
Variant getVariable(const std::string& name) const
```

Returns the current value of a variable. Returns `Variant::Int(0)` equivalent if the variable doesn't exist.

Check existence first with [hasVariable](#hasvariable).

---

### setVariable

```cpp
void setVariable(const std::string& name, const Variant& value)
```

Sets or creates a variable.

```cpp
runner.setVariable("hp", Variant::Int(100));
runner.setVariable("name", Variant::String("Hero"));
runner.setVariable("alive", Variant::Bool(true));
```

---

### hasVariable

```cpp
bool hasVariable(const std::string& name) const
```

Returns `true` if the variable exists.

---

### getVariableNames

```cpp
std::vector<std::string> getVariableNames() const
```

Returns a list of all variable names currently in scope.

---

### saveState

```cpp
bool saveState(const std::string& filepath) const
```

Serializes the complete runner state to a `.gys` FlatBuffers binary file. Includes:
- Current node and program counter
- All variables (including shadowed ones in call frames)
- Call stack
- Pending choices (with modifiers)
- Visit counts
- Once-choice tracking

---

### loadState

```cpp
bool loadState(const std::string& filepath)
```

Restores state from a `.gys` file. A story must already be loaded.

---

### loadLocale

```cpp
bool loadLocale(const std::string& csvPath)
```

Loads a CSV locale overlay. Translated strings replace originals for Line and Choice text.

---

### clearLocale

```cpp
void clearLocale()
```

Removes the locale overlay, reverting to original text.

---

### getLocale

```cpp
std::string getLocale() const
```

Returns the current locale identifier, or empty string if none.

---

### getVisitCount

```cpp
int32_t getVisitCount(const std::string& nodeName) const
```

Returns how many times the named node has been entered.

---

### hasVisited

```cpp
bool hasVisited(const std::string& nodeName) const
```

Returns `true` if the node has been visited at least once.

---

### getCharacterProperty

```cpp
std::string getCharacterProperty(const std::string& characterId, const std::string& key) const
```

Returns a character property value. Returns empty string if not found.

---

### getCharacterNames

```cpp
std::vector<std::string> getCharacterNames() const
```

Returns all defined character IDs.

---

### getCharacterDisplayName

```cpp
std::string getCharacterDisplayName(const std::string& characterId) const
```

Convenience method returning the `displayName` property.

---

### getNodeTag

```cpp
std::string getNodeTag(const std::string& nodeName, const std::string& key) const
```

Returns the value of a metadata tag on a node. Returns empty string if not found.

---

### getNodeTags

```cpp
std::vector<std::pair<std::string, std::string>> getNodeTags(const std::string& nodeName) const
```

Returns all metadata tags on a node as key-value pairs.

---

### hasNodeTag

```cpp
bool hasNodeTag(const std::string& nodeName, const std::string& key) const
```

Returns `true` if the node has the specified tag.

---

### setSeed

```cpp
void setSeed(uint32_t seed)
```

Sets the RNG seed for deterministic random branch selection.

---

## Debug API

The Runner also exposes a Debug API for the CLI debugger. See [Debugger](../tools/debugger.md) for usage.

### Debug Methods

| Return | Method |
|--------|--------|
| `void` | `addBreakpoint(nodeName, pc)` |
| `void` | `removeBreakpoint(nodeName, pc)` |
| `void` | `clearBreakpoints()` |
| `bool` | `hasBreakpoint(nodeName, pc) const` |
| `vector<pair<string, uint32_t>>` | `getBreakpoints() const` |
| `void` | `setStepMode(bool)` |
| `bool` | `isStepMode() const` |
| `DebugLocation` | `getLocation() const` |
| `vector<CallFrameInfo>` | `getCallStack() const` |
| `string` | `getCurrentNodeName() const` |
| `uint32_t` | `getCurrentPC() const` |
| `vector<string>` | `getNodeNames() const` |
| `uint32_t` | `getNodeInstructionCount(nodeName) const` |
| `string` | `getInstructionInfo(nodeName, pc) const` |

### Debug Types

```cpp
struct DebugLocation {
    std::string nodeName;
    uint32_t pc;
    std::string instructionType;
};

struct CallFrameInfo {
    std::string nodeName;
    uint32_t pc;
    std::string returnVarName;
    std::vector<std::string> paramNames;
};
```

## Example: Minimal Console Player

```cpp
#include "gyeol_runner.h"
#include <fstream>
#include <iostream>

int main() {
    // Load .gyb file
    std::ifstream ifs("story.gyb", std::ios::binary | std::ios::ate);
    auto size = ifs.tellg();
    ifs.seekg(0);
    std::vector<uint8_t> buffer(size);
    ifs.read(reinterpret_cast<char*>(buffer.data()), size);

    // Start runner
    Gyeol::Runner runner;
    runner.start(buffer.data(), buffer.size());

    // Main loop
    while (!runner.isFinished()) {
        auto result = runner.step();
        switch (result.type) {
            case Gyeol::StepType::LINE:
                if (result.line.character)
                    std::cout << "[" << result.line.character << "] ";
                std::cout << result.line.text << "\n";
                break;

            case Gyeol::StepType::CHOICES:
                for (auto& c : result.choices)
                    std::cout << "  [" << c.index << "] " << c.text << "\n";
                int choice;
                std::cin >> choice;
                runner.choose(choice);
                break;

            case Gyeol::StepType::COMMAND:
                std::cout << "CMD: " << result.command.type << "\n";
                break;

            case Gyeol::StepType::END:
                std::cout << "--- END ---\n";
                break;
        }
    }
    return 0;
}
```
