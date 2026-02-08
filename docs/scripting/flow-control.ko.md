# 흐름 제어

## Jump

다른 노드로의 단방향 이동입니다. 대상 노드에서 실행이 계속되며 **복귀하지 않습니다**.

```
jump target_node
```

### 예제

```
label start:
    hero "Let's go!"
    jump next_scene

label next_scene:
    hero "We've arrived."
```

## Call / Return

Call은 현재 위치를 **콜 스택**에 저장하고 대상 노드로 이동합니다. 대상 노드가 끝나거나 `return`을 실행하면, `call` 다음 명령으로 돌아옵니다.

### 기본 Call

```
label start:
    call greeting
    hero "Back from greeting."

label greeting:
    hero "Hello there!"
    # 노드가 끝나면 자동으로 복귀
```

### 명시적 Return

```
label helper:
    hero "Doing something..."
    return      # 명시적으로 복귀 (남은 명령을 건너뜀)
```

### 반환값이 있는 Call

```
label start:
    $ result = call compute(10, 20)
    hero "The answer is {result}."

label compute(a, b):
    return a + b
```

`return` 표현식이 평가되어 `$ var = call ...`에서 지정한 변수에 저장됩니다.

## 조건문

### 단순 조건

```
if hp > 50 -> strong_path else weak_path
```

- **좌측 피연산자** - 변수 이름 또는 표현식
- **연산자** - `==`, `!=`, `>`, `<`, `>=`, `<=`
- **우측 피연산자** - 리터럴 값 또는 표현식
- **참 분기** - 조건이 참일 때 이동할 대상
- **거짓 분기** (선택 사항) - 조건이 거짓일 때 이동할 대상

### 참/거짓 판정

```
if has_key -> unlock_door
```

연산자가 없으면 변수의 "참/거짓" 여부를 확인합니다:
- Bool: true/false
- Int: 0이 아니면 참
- Float: 0이 아니면 참
- String: 비어 있지 않으면 참

### 표현식 조건

양쪽 모두 완전한 표현식을 사용할 수 있습니다:

```
if hp - 10 > 0 -> survive else die
if attack * 2 + bonus >= defense -> hit
```

### 논리 연산자

```
if hp > 0 and has_weapon == true -> fight
if is_tired or is_hungry -> rest
if not is_dead -> alive
```

논리 연산자는 어떤 조건이든 결합할 수 있습니다:

```
if hp > 50 and gold > 100 and visited("shop") -> wealthy_warrior
if not has_key or not has_torch -> cannot_enter
```

### Elif / Else 체인

```
if score >= 90 -> grade_a
elif score >= 80 -> grade_b
elif score >= 70 -> grade_c
else -> grade_f
```

- `elif`는 `if` 또는 다른 `elif` 뒤에 와야 합니다
- `else`는 `if` 또는 `elif` 뒤에 와야 합니다
- 조건은 위에서 아래로 평가되며, 첫 번째로 일치하는 것이 실행됩니다
- 연쇄된 Condition + Jump 명령으로 컴파일됩니다 (스키마 변경 불필요)

### 방문 기반 조건

```
if visited("secret_room") -> knows_secret
if visit_count("shop") > 5 -> loyal_customer
```

## Menu (선택지)

플레이어에게 선택지를 제시합니다. 플레이어가 하나를 선택할 때까지 실행이 일시 정지됩니다.

```
menu:
    "Choice A" -> node_a
    "Choice B" -> node_b
    "Choice C" -> node_c
```

### 조건부 선택지

```
menu:
    "Use key" -> unlock if has_key
    "Force the door" -> force if strength > 10
    "Walk away" -> leave
```

조건을 만족하는 선택지만 플레이어에게 표시됩니다.

### 선택지 수식어

```
menu:
    "Buy healing potion" -> buy_heal #once
    "Buy mana potion" -> buy_mana #once
    "Browse wares" -> browse #sticky
    "Leave" -> exit #fallback
```

| 수식어 | 동작 |
|--------|------|
| *(기본값)* | 조건을 만족하면 항상 표시 |
| `#once` | 한 번 선택하면 영구적으로 숨김 |
| `#sticky` | 조건을 만족하면 항상 표시 (명시적 기본값) |
| `#fallback` | 같은 메뉴의 다른 모든 선택지가 숨겨졌을 때만 표시 |

**조건과 함께 사용:**

```
menu:
    "VIP offer" -> vip_deal if is_vip #once
    "Regular deal" -> regular #once
    "Leave" -> exit #fallback
```

두 가지 순서 모두 유효합니다: `if condition #modifier`와 `#modifier if condition`.

**Fallback 동작 원리:**
1. 모든 선택지를 평가: 조건 + once 필터링 적용
2. 일반과 fallback 그룹으로 분리
3. 일반 선택지가 남아 있으면: 일반 선택지만 표시
4. 모든 일반 선택지가 숨겨지면: fallback 선택지 표시
5. 선택지가 전혀 없으면: 빈 배열 반환 (게임 엔진에서 처리)

**Once 추적:**
- 각 once 선택지는 고유 키(`nodeName:pc`)로 추적됩니다
- Once 추적 상태는 Save/Load를 통해 유지됩니다
- `start()`로 스토리를 재시작하면 초기화됩니다

## 랜덤 분기

가중치 기반 랜덤 선택:

```
random:
    50 -> common_path       # 50/(50+30+10+1) = ~55% 확률
    30 -> uncommon_path     # 30/91 = ~33% 확률
    10 -> rare_path         # 10/91 = ~11% 확률
    -> ultra_rare           # 1/91 = ~1% 확률 (기본 가중치 = 1)
```

- 각 분기에는 **가중치** (양의 정수, 기본값 1)가 있습니다
- 확률 = 가중치 / 전체 가중치 합
- 가중치 0은 선택되지 않음을 의미합니다
- `std::mt19937` RNG를 사용합니다; 결정적 테스트를 위해 `setSeed()`로 시드를 설정하세요

## 흐름 제어 요약

| 명령 | 문법 | 복귀 여부 | 스택 사용 |
|------|------|-----------|-----------|
| Jump | `jump node` | 아니오 | 아니오 |
| Call | `call node` | 예 | Push |
| Call (반환값) | `$ v = call node` | 예 | Push |
| Return | `return [expr]` | 해당 없음 | Pop |
| Condition | `if cond -> node [else node]` | 아니오 | 아니오 |
| Elif | `elif cond -> node` | 아니오 | 아니오 |
| Else | `else -> node` | 아니오 | 아니오 |
| Menu | `menu: choices...` | 일시 정지 | 아니오 |
| Random | `random: branches...` | 아니오 | 아니오 |
