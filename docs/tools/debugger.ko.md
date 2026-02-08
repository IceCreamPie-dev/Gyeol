# 디버거

`GyeolDebugger`는 컴파일된 스토리를 인스트럭션 단위로 단계별 실행할 수 있는 CLI 인터랙티브 디버거입니다.

## 사용법

```bash
GyeolDebugger <story.gyb> [--help] [--version]
```

## 시작하기

```bash
$ GyeolDebugger story.gyb
[Gyeol Debugger] Loaded story.gyb
[start:0] Line  hero "Hello!"
(gyeol) step
[hero] Hello!
[start:1] Choice  "Yes" -> good
(gyeol)
```

디버거는 **스텝 모드**(각 인스트럭션 후 일시 정지)로 시작합니다. `(gyeol)` 프롬프트에서 커맨드를 입력합니다.

## 커맨드

### 실행 제어

| 커맨드 | 약어 | 인자 | 설명 |
|---------|---------|-----------|-------------|
| `step` | `s` | - | 인스트럭션 하나 실행 |
| `continue` | `c` | - | 브레이크포인트 또는 END까지 실행 |
| `choose` | `ch` | `INDEX` | 선택지 선택 (0부터 시작) |
| `restart` | `r` | - | 시작 노드에서 다시 시작 |

### 브레이크포인트

| 커맨드 | 약어 | 인자 | 설명 |
|---------|---------|-----------|-------------|
| `break` | `b` | `NODE [PC]` | 브레이크포인트 설정 (PC 기본값은 0) |
| `delete` | `d` | `[NODE [PC]]` | 브레이크포인트 제거 또는 전체 삭제 |
| `breakpoints` | `bp` | - | 모든 브레이크포인트 나열 |

### 검사

| 커맨드 | 약어 | 인자 | 설명 |
|---------|---------|-----------|-------------|
| `locals` | `l` | - | 모든 변수와 값 표시 |
| `print` | `p` | `VARIABLE` | 단일 변수 출력 |
| `set` | - | `VARIABLE VALUE` | 변수 설정 (int/float/bool/"string") |
| `where` | `w` | - | 위치 + 콜 스택 + 방문 횟수 표시 |

### 노드 정보

| 커맨드 | 약어 | 인자 | 설명 |
|---------|---------|-----------|-------------|
| `nodes` | `n` | - | 인스트럭션 수와 함께 모든 노드 나열 |
| `info` | `i` | `NODE` | 노드의 모든 인스트럭션 표시 |

### 기타

| 커맨드 | 약어 | 인자 | 설명 |
|---------|---------|-----------|-------------|
| `help` | `h` | - | 커맨드 도움말 표시 |
| `quit` | `q`, `exit` | - | 디버거 종료 |

## 브레이크포인트

### 브레이크포인트 설정

```
(gyeol) break start         # Break at start:0
(gyeol) break encounter 3   # Break at encounter:3
```

### 브레이크포인트 나열

```
(gyeol) breakpoints
Breakpoints:
  [1] start:0
  [2] encounter:3
```

### 브레이크포인트 제거

```
(gyeol) delete start 0      # Remove specific breakpoint
(gyeol) delete              # Clear all breakpoints
```

### 브레이크포인트 적중

```
(gyeol) continue
... (running)
[Breakpoint hit] encounter:3
[encounter:3] Condition  hp > 0 -> brave else coward
(gyeol)
```

## 스텝 모드

디버거는 스텝 모드가 활성화된 상태로 시작합니다. 각 `step` 커맨드는 정확히 하나의 인스트럭션을 실행합니다:

```
(gyeol) step
[start:0] Line  hero "Hello!"

(gyeol) step
[hero] Hello!
[start:1] Jump  -> encounter
```

`continue`를 사용하면 브레이크포인트나 END까지 자유롭게 실행합니다:

```
(gyeol) continue
... (executing multiple instructions)
--- END ---
```

## 변수 검사

### 모든 변수 보기

```
(gyeol) locals
Variables:
  hp = 100 (Int)
  name = "Hero" (String)
  has_key = true (Bool)
  speed = 3.14 (Float)
```

### 단일 변수 보기

```
(gyeol) print hp
hp = 100 (Int)
```

### 변수 수정

```
(gyeol) set hp 50
hp = 50

(gyeol) set name "Warrior"
name = "Warrior"

(gyeol) set has_key false
has_key = false
```

## 위치 및 콜 스택

### 현재 위치

```
(gyeol) where
Location: encounter:3 [Condition]
Call stack:
  [0] start:5 (call)
  [1] encounter:3 (current)
Visit counts:
  start: 1
  encounter: 1
```

### 노드 검사

```
(gyeol) nodes
Nodes:
  start (5 instructions)
  encounter (4 instructions)
  brave (2 instructions)
  coward (2 instructions)
```

```
(gyeol) info encounter
encounter (4 instructions):
  [0] Line  "A giant wolf appears!"
  [1] Command  sfx "wolf_growl.wav"
  [2] Line  hero "What do we do?"
  [3] Condition  courage == 1 -> brave else coward
```

## 워크플로우 예제

```bash
$ GyeolDebugger story.gyb

# Set a breakpoint at the encounter
(gyeol) break encounter

# Run to the breakpoint
(gyeol) continue
[Breakpoint hit] encounter:0

# Inspect state
(gyeol) locals
  courage = 1 (Int)

# Step through
(gyeol) step
[encounter:0] Line  "A giant wolf appears!"

(gyeol) step
"A giant wolf appears!"
[encounter:1] Condition  courage == 1 -> brave else coward

# Modify variable to test different branch
(gyeol) set courage 0
(gyeol) step
[coward:0] Line  hero "Help!"

# Check where we are
(gyeol) where
Location: coward:0 [Line]
```

## ANSI 색상

디버거는 색상 출력을 사용합니다:

| 색상 | 용도 |
|-------|-------|
| 빨간색 | 에러 |
| 초록색 | 성공 메시지 |
| 시안 | 노드 이름과 label |
| 노란색 | 강조와 값 |
| 흐린색 | 상태 정보 |

## 빌드 위치

CMake로 빌드 후:

```
build/src/gyeol_debugger/GyeolDebugger
```
