# Runner (C++ API)

**네임스페이스:** `Gyeol`
**헤더:** `#include "gyeol_runner.h"`

컴파일된 Gyeol 스토리를 실행하는 핵심 가상 머신입니다.

## 설명

`Runner`는 컴파일된 `.gyb` 스토리 바이너리를 해석하는 엔진 독립적 VM입니다. 어떤 게임 엔진이나 애플리케이션에도 통합할 수 있는 이벤트 기반 `step()`/`choose()` API를 제공합니다.

Runner가 관리하는 항목:
- 스토리 실행 흐름 (프로그램 카운터, 현재 노드)
- 변수 상태
- 콜 스택 (함수 호출용)
- 선택지 필터링 (조건, once/sticky/fallback 수식어)
- 방문 추적
- 랜덤 넘버 생성
- 저장/로드 직렬화

## 튜토리얼

- [빠른 시작](../getting-started/quick-start.md)
- [아키텍처](../advanced/architecture.md)

---

## 타입

### Variant

```cpp
struct Variant {
    enum Type { BOOL, INT, FLOAT, STRING, LIST };
    Type type;
    // Access via: b (bool), i (int32_t), f (float), s (string), list (vector<string>)
};
```

변수 값 컨테이너입니다. 정적 팩토리 메서드로 생성합니다:

```cpp
Variant::Bool(true)
Variant::Int(42)
Variant::Float(3.14f)
Variant::String("hello")
Variant::List({"item1", "item2"})
```

자세한 내용은 [Variant](class-variant.md)를 참고하세요.

---

### StepType

```cpp
enum class StepType { LINE, CHOICES, COMMAND, END };
```

| 값 | 설명 |
|-------|-------------|
| `LINE` | 대사 또는 나레이션 라인 |
| `CHOICES` | 선택지가 있는 메뉴 (계속하려면 `choose()` 호출) |
| `COMMAND` | 엔진 명령 (계속하려면 `step()` 호출) |
| `END` | 스토리 종료 |

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

`step()`이 반환합니다. `type`을 확인하여 어떤 필드를 읽을지 결정합니다.

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

## 메서드

### 핵심 실행

| 반환 타입 | 메서드 |
|--------|--------|
| `bool` | [start](#start)`(const uint8_t* buffer, size_t size)` |
| `StepResult` | [step](#step)`()` |
| `void` | [choose](#choose)`(int index)` |
| `bool` | [isFinished](#isfinished)`() const` |
| `bool` | [hasStory](#hasstory)`() const` |

### 변수 접근

| 반환 타입 | 메서드 |
|--------|--------|
| `Variant` | [getVariable](#getvariable)`(const std::string& name) const` |
| `void` | [setVariable](#setvariable)`(const std::string& name, const Variant& value)` |
| `bool` | [hasVariable](#hasvariable)`(const std::string& name) const` |
| `std::vector<std::string>` | [getVariableNames](#getvariablenames)`() const` |

### 저장 / 로드

| 반환 타입 | 메서드 |
|--------|--------|
| `bool` | [saveState](#savestate)`(const std::string& filepath) const` |
| `bool` | [loadState](#loadstate)`(const std::string& filepath)` |

### 로케일

| 반환 타입 | 메서드 |
|--------|--------|
| `bool` | [loadLocale](#loadlocale)`(const std::string& csvPath)` |
| `void` | [clearLocale](#clearlocale)`()` |
| `std::string` | [getLocale](#getlocale)`() const` |

### 방문 추적

| 반환 타입 | 메서드 |
|--------|--------|
| `int32_t` | [getVisitCount](#getvisitcount)`(const std::string& nodeName) const` |
| `bool` | [hasVisited](#hasvisited)`(const std::string& nodeName) const` |

### 캐릭터

| 반환 타입 | 메서드 |
|--------|--------|
| `std::string` | [getCharacterProperty](#getcharacterproperty)`(const std::string& characterId, const std::string& key) const` |
| `std::vector<std::string>` | [getCharacterNames](#getcharacternames)`() const` |
| `std::string` | [getCharacterDisplayName](#getcharacterdisplayname)`(const std::string& characterId) const` |

### 노드 태그

| 반환 타입 | 메서드 |
|--------|--------|
| `std::string` | [getNodeTag](#getnodetag)`(const std::string& nodeName, const std::string& key) const` |
| `std::vector<std::pair<std::string, std::string>>` | [getNodeTags](#getnodetags)`(const std::string& nodeName) const` |
| `bool` | [hasNodeTag](#hasnodetag)`(const std::string& nodeName, const std::string& key) const` |

### RNG

| 반환 타입 | 메서드 |
|--------|--------|
| `void` | [setSeed](#setseed)`(uint32_t seed)` |

---

## 메서드 설명

### start

```cpp
bool start(const uint8_t* buffer, size_t size)
```

컴파일된 `.gyb` 버퍼로 Runner를 초기화합니다. FlatBuffers 데이터를 파싱하고, 전역 변수를 초기화하며, 캐시(캐릭터, 노드 태그)를 구축하고, 방문 횟수와 once 선택지 추적을 리셋한 후, 시작 노드로 이동합니다.

성공 시 `true`를 반환합니다.

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

다음 인스트럭션을 실행하고 `StepResult`를 반환합니다. 결과의 `type` 필드에 따라 진행 방법이 달라집니다:

| 타입 | 동작 |
|------|--------|
| `LINE` | 대사를 표시하고 `step()`을 다시 호출 |
| `CHOICES` | 선택지를 표시하고 `choose(index)`를 호출 |
| `COMMAND` | 명령을 처리하고 `step()`을 다시 호출 |
| `END` | 스토리 종료 |

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

0부터 시작하는 인덱스로 선택지를 선택하고 자동으로 스토리를 진행합니다. `step()`이 `StepType::CHOICES`를 반환한 후 호출해야 합니다.

---

### isFinished

```cpp
bool isFinished() const
```

스토리가 끝에 도달했으면 `true`를 반환합니다.

---

### hasStory

```cpp
bool hasStory() const
```

`start()`를 통해 스토리 버퍼가 로드되었으면 `true`를 반환합니다.

---

### getVariable

```cpp
Variant getVariable(const std::string& name) const
```

변수의 현재 값을 반환합니다. 변수가 존재하지 않으면 `Variant::Int(0)`에 해당하는 값을 반환합니다.

먼저 [hasVariable](#hasvariable)로 존재 여부를 확인하세요.

---

### setVariable

```cpp
void setVariable(const std::string& name, const Variant& value)
```

변수를 설정하거나 생성합니다.

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

변수가 존재하면 `true`를 반환합니다.

---

### getVariableNames

```cpp
std::vector<std::string> getVariableNames() const
```

현재 스코프에 있는 모든 변수 이름의 목록을 반환합니다.

---

### saveState

```cpp
bool saveState(const std::string& filepath) const
```

Runner의 전체 상태를 `.gys` FlatBuffers 바이너리 파일로 직렬화합니다. 포함 항목:
- 현재 노드와 프로그램 카운터
- 모든 변수 (콜 프레임의 섀도된 변수 포함)
- 콜 스택
- 대기 중인 선택지 (수식어 포함)
- 방문 횟수
- Once 선택지 추적 상태

---

### loadState

```cpp
bool loadState(const std::string& filepath)
```

`.gys` 파일에서 상태를 복원합니다. 스토리가 이미 로드되어 있어야 합니다.

---

### loadLocale

```cpp
bool loadLocale(const std::string& csvPath)
```

CSV 로케일 오버레이를 로드합니다. 번역된 문자열이 Line과 Choice 텍스트의 원본을 대체합니다.

---

### clearLocale

```cpp
void clearLocale()
```

로케일 오버레이를 제거하여 원본 텍스트로 되돌립니다.

---

### getLocale

```cpp
std::string getLocale() const
```

현재 로케일 식별자를 반환하거나, 없으면 빈 문자열을 반환합니다.

---

### getVisitCount

```cpp
int32_t getVisitCount(const std::string& nodeName) const
```

해당 노드에 진입한 횟수를 반환합니다.

---

### hasVisited

```cpp
bool hasVisited(const std::string& nodeName) const
```

해당 노드를 최소 한 번 방문했으면 `true`를 반환합니다.

---

### getCharacterProperty

```cpp
std::string getCharacterProperty(const std::string& characterId, const std::string& key) const
```

캐릭터 속성 값을 반환합니다. 찾을 수 없으면 빈 문자열을 반환합니다.

---

### getCharacterNames

```cpp
std::vector<std::string> getCharacterNames() const
```

정의된 모든 캐릭터 ID를 반환합니다.

---

### getCharacterDisplayName

```cpp
std::string getCharacterDisplayName(const std::string& characterId) const
```

`displayName` 속성을 반환하는 편의 메서드입니다.

---

### getNodeTag

```cpp
std::string getNodeTag(const std::string& nodeName, const std::string& key) const
```

노드의 메타데이터 태그 값을 반환합니다. 찾을 수 없으면 빈 문자열을 반환합니다.

---

### getNodeTags

```cpp
std::vector<std::pair<std::string, std::string>> getNodeTags(const std::string& nodeName) const
```

노드의 모든 메타데이터 태그를 키-값 쌍으로 반환합니다.

---

### hasNodeTag

```cpp
bool hasNodeTag(const std::string& nodeName, const std::string& key) const
```

해당 노드에 지정된 태그가 있으면 `true`를 반환합니다.

---

### setSeed

```cpp
void setSeed(uint32_t seed)
```

결정적 랜덤 분기 선택을 위한 RNG 시드를 설정합니다.

---

## Debug API

Runner는 CLI 디버거를 위한 Debug API도 제공합니다. 사용법은 [디버거](../tools/debugger.md)를 참고하세요.

### 디버그 메서드

| 반환 타입 | 메서드 |
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

### 디버그 타입

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

## 예제: 최소 콘솔 플레이어

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
