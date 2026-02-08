# Variant

**네임스페이스:** `Gyeol`
**헤더:** `#include "gyeol_runner.h"`

Gyeol 런타임의 변수 값 컨테이너입니다.

## 설명

`Variant`는 Gyeol 런타임에서 변수 값을 저장하는 태그 유니온입니다. 스토리의 각 변수는 현재 값을 담고 있는 Variant를 가집니다.

## 타입 열거형

```cpp
enum Type { BOOL, INT, FLOAT, STRING, LIST };
```

| 타입 | C++ 필드 | 크기 | 설명 |
|------|-----------|------|-------------|
| `BOOL` | `b` | `bool` | 불리언 값 |
| `INT` | `i` | `int32_t` | 32비트 부호 있는 정수 |
| `FLOAT` | `f` | `float` | 32비트 부동소수점 |
| `STRING` | `s` | `std::string` | UTF-8 문자열 |
| `LIST` | `list` | `std::vector<std::string>` | 문자열 리스트 |

## 팩토리 메서드

```cpp
static Variant Bool(bool v);
static Variant Int(int32_t v);
static Variant Float(float v);
static Variant String(const std::string& v);
static Variant List(const std::vector<std::string>& v);
static Variant List(std::vector<std::string>&& v);  // Move version
```

## 사용법

### Variant 생성

```cpp
auto hp = Variant::Int(100);
auto name = Variant::String("Hero");
auto alive = Variant::Bool(true);
auto speed = Variant::Float(3.14f);
auto items = Variant::List({"sword", "shield"});
```

### Variant 읽기

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

### 변수 설정

```cpp
runner.setVariable("hp", Variant::Int(100));
runner.setVariable("name", Variant::String("Hero"));
runner.setVariable("alive", Variant::Bool(true));
```

## 타입 매핑

### Gyeol 스크립트에서 Variant로

| 스크립트 값 | Variant 타입 |
|-------------|-------------|
| `true` / `false` | `BOOL` |
| `42`, `-10` | `INT` |
| `3.14` | `FLOAT` |
| `"hello"` | `STRING` |
| `+= "item"` | `LIST` (append로 생성) |

### Variant에서 Godot로

GDExtension StoryPlayer를 통해 사용할 때:

| Variant 타입 | Godot 타입 |
|-------------|------------|
| `BOOL` | `bool` |
| `INT` | `int` |
| `FLOAT` | `float` |
| `STRING` | `String` |
| `LIST` | `Array[String]` |

### Godot에서 Variant로

| Godot 타입 | Variant 타입 |
|-----------|-------------|
| `bool` | `BOOL` |
| `int` | `INT` |
| `float` | `FLOAT` |
| `String` | `STRING` |
| `Array` | `LIST` (요소가 String으로 변환됨) |

## 산술 동작

| 연산 | INT + INT | INT + FLOAT | FLOAT + FLOAT |
|-----------|-----------|-------------|---------------|
| `+` | INT | FLOAT | FLOAT |
| `-` | INT | FLOAT | FLOAT |
| `*` | INT | FLOAT | FLOAT |
| `/` | INT (절삭) | FLOAT | FLOAT |
| `%` | INT | 해당 없음 | 해당 없음 |

## 참/거짓 판정 (Truthiness)

조건문과 인라인 `{if}` 표현식에서 사용됩니다:

| 타입 | 참 (Truthy) | 거짓 (Falsy) |
|------|--------|-------|
| BOOL | `true` | `false` |
| INT | 0이 아닌 값 | `0` |
| FLOAT | 0이 아닌 값 | `0.0` |
| STRING | 비어있지 않은 문자열 | `""` |

## 문자열 변환

텍스트에서 보간될 때 (`{variable}`), Variant는 다음과 같이 문자열로 변환됩니다:

| 타입 | 형식 |
|------|--------|
| BOOL | `"true"` 또는 `"false"` |
| INT | 10진수 (예: `"42"`) |
| FLOAT | 소수 포함 (예: `"3.140000"`) |
| STRING | 그대로 |
| LIST | 쉼표 구분 (예: `"sword, shield"`) |
