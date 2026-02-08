# 함수

Gyeol의 label은 매개변수, 반환값, 로컬 스코프를 갖춘 함수로 사용할 수 있습니다.

## 함수 정의

함수는 매개변수가 있는 label입니다:

```gyeol
label greet(name, title):
    hero "Hello, {title} {name}!"
```

- 매개변수는 label 이름 뒤 괄호 안에 선언합니다
- 매개변수는 함수 내에서 로컬 변수처럼 동작합니다
- 여러 매개변수는 쉼표로 구분합니다

## 함수 호출

### 단순 호출

```gyeol
call greet("Hero", "Sir")
```

인자는 호출 시점에 표현식으로 평가됩니다.

### 반환값이 있는 호출

```gyeol
$ result = call compute(10, 20)
hero "Result: {result}"
```

반환값은 지정된 변수에 저장됩니다.

### 표현식 인자

인자로 어떤 표현식이든 사용할 수 있습니다:

```gyeol
$ dmg = call calculate_damage(attack * 2, defense - 5)
```

## 반환값

### 표현식을 반환

```gyeol
label add(a, b):
    return a + b
```

### 리터럴을 반환

```gyeol
label get_greeting():
    return "Hello!"
```

### 값 없이 반환

```gyeol
label do_something():
    hero "Processing..."
    return     # 조기 종료, 반환값 없음
```

### 암묵적 반환

함수가 명시적 `return` 없이 마지막 명령에 도달하면 자동으로 반환됩니다 (값 없음).

```gyeol
label side_effect():
    $ counter = counter + 1
    # 여기서 암묵적으로 반환
```

## 매개변수 스코프

### 로컬 스코프 (섀도잉)

매개변수는 동일한 이름의 기존 전역 변수를 **섀도잉**하는 로컬 스코프를 생성합니다:

```gyeol
$ name = "Global"

label start:
    hero "Before: {name}"          # "Global"
    call set_name("Local")
    hero "After: {name}"           # "Global" (복원됨!)

label set_name(name):
    hero "Inside: {name}"          # "Local"
    # 이 함수가 반환되면 'name'은 "Global"로 되돌아감
```

섀도잉의 동작 원리:
1. 호출 전에 `name`의 현재 값을 콜 프레임에 저장합니다
2. 매개변수 값(`"Local"`)이 `name`에 대입됩니다
3. 함수 본문이 매개변수 값으로 실행됩니다
4. 반환 시 저장된 값(`"Global"`)이 복원됩니다

### 기본값

누락된 인자는 `Int(0)`으로 기본 설정됩니다:

```gyeol
label damage(amount, multiplier):
    # call damage(10)으로 호출하면:
    # amount = 10, multiplier = 0 (기본값)
    return amount * multiplier

label start:
    $ d = call damage(10)      # multiplier는 기본값 0
    $ d = call damage(10, 2)   # 둘 다 제공
```

매개변수 수를 초과하는 추가 인자는 조용히 무시됩니다.

## 중첩 호출

함수에서 다른 함수를 호출할 수 있습니다:

```gyeol
label start:
    $ msg = call format_greeting("Hero", 100)
    hero "{msg}"

label format_greeting(name, hp):
    $ status = call get_status(hp)
    return "{name}: {status}"

label get_status(hp):
    if hp > 50 -> high_status else low_status

label high_status:
    return "Healthy"

label low_status:
    return "Weak"
```

콜 스택은 각 중첩 호출을 추적하며, 반환 시마다 상태를 올바르게 복원합니다.

## Jump vs Call

| 기능 | `jump` | `call` |
|------|--------|--------|
| 호출자로 복귀? | 아니오 | 예 |
| 콜 스택 사용? | 아니오 | 예 |
| 매개변수 지원? | 아니오 | 예 |
| 반환값 지원? | 아니오 | 예 |
| 용도 | 장면 전환 | 재사용 가능한 로직 |

```gyeol
# 장면 흐름에는 jump 사용
jump next_chapter

# 재사용 가능한 함수에는 call 사용
$ greeting = call format_name("Hero", "Sir")
call play_cutscene
```

> **참고:** `jump`은 인자를 지원하지 않습니다. 매개변수가 필요한 호출에는 `call`을 사용하세요.

## Save/Load 호환성

섀도된 변수와 매개변수 이름을 포함한 콜 스택은 완전히 직렬화됩니다:
- `.gys` 파일에 저장됩니다 (FlatBuffers 바이너리)
- 로드 시 적절한 스코프 복원과 함께 복원됩니다
- 이전 버전(매개변수 없음)의 세이브와 하위 호환됩니다

## 실용적인 패턴

### 유틸리티 함수

```gyeol
label clamp_hp(value):
    if value > 100 -> cap_high
    if value < 0 -> cap_low
    return value

label cap_high:
    return 100

label cap_low:
    return 0

label start:
    $ hp = call clamp_hp(hp + heal_amount)
```

### 대화 헬퍼

```gyeol
label say_with_effect(character, text):
    @ shake 0.2
    # 참고: character/text를 변수로 사용하면 동적 디스패치가 되지 않습니다
    # 이것은 단순화된 패턴입니다
    hero "{text}"
    return
```

### 상태 머신

```gyeol
label start:
    call game_loop

label game_loop:
    menu:
        "Fight" -> fight_action
        "Heal" -> heal_action
        "Flee" -> flee_action

label fight_action:
    $ hp = hp - 10
    hero "Took 10 damage! HP: {hp}"
    if hp <= 0 -> game_over
    jump game_loop

label heal_action:
    $ hp = hp + 20
    hero "Healed! HP: {hp}"
    jump game_loop

label flee_action:
    hero "You ran away!"
```
