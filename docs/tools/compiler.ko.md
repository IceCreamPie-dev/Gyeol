# 컴파일러 CLI

`GyeolCompiler`는 `.gyeol` 텍스트 스크립트를 `.gyb` 바이너리 파일로 컴파일합니다.

## 사용법

```bash
GyeolCompiler <input.gyeol> [options]
```

## 옵션

| 옵션 | 설명 |
|--------|-------------|
| `-o <path>` | 출력 `.gyb` 파일 경로 (기본값: `story.gyb`) |
| `--export-strings <path>` | 번역 가능한 문자열을 CSV로 추출 |
| `--analyze [path]` | 정적 분석 실행 (stdout 또는 파일로 출력) |
| `-O` | 최적화 활성화 (상수 폴딩, 데드 코드 제거) |
| `-h`, `--help` | 도움말 메시지 표시 |
| `--version` | 버전 번호 표시 |

## 예제

### 기본 컴파일

```bash
# Compile to default output (story.gyb)
GyeolCompiler my_story.gyeol

# Specify output path
GyeolCompiler my_story.gyeol -o game/assets/story.gyb
```

### 번역 문자열 추출

```bash
GyeolCompiler my_story.gyeol --export-strings strings.csv
```

대사 Line ID와 원본 텍스트가 포함된 CSV 파일을 생성합니다:

```csv
id,text
start:0:a3f2,"Hello, how are you?"
start:1:b7c1,"Sure, let's go!"
```

### 정적 분석

```bash
# Print analysis to stdout
GyeolCompiler my_story.gyeol --analyze

# Write analysis to file
GyeolCompiler my_story.gyeol --analyze report.txt
```

## 에러 처리

컴파일러는 첫 번째 에러에서 멈추지 않고 **모든** 에러를 수집합니다:

```bash
$ GyeolCompiler broken.gyeol
Error: [line 5] Unknown jump target: 'missing_node'
Error: [line 12] Unknown choice target: 'also_missing'
Error: [line 18] Duplicate label name: 'start'
Compilation failed with 3 errors.
```

### 에러 카테고리

| 카테고리 | 예시 |
|----------|---------|
| 구문 에러 | 잘못된 줄 형식, label에 콜론 누락 |
| 알 수 없는 대상 | 존재하지 않는 label을 참조하는 jump/call/choice |
| 중복 label | 같은 이름의 label 2개 |
| Import 에러 | 파일 없음, 순환 import |
| 매개변수 에러 | 중복 매개변수 이름, 닫히지 않은 괄호 |

## 멀티 파일 컴파일

스크립트에서 `import`를 사용하면, 컴파일러가 모든 import를 해석합니다:

```gyeol
# main.gyeol
import "characters.gyeol"
import "chapter1/intro.gyeol"

label start:
    call intro
```

```bash
# Only specify the main file - imports are resolved automatically
GyeolCompiler main.gyeol -o game.gyb
```

- Import 경로는 가져오는 파일을 기준으로 상대 경로입니다
- 순환 import가 감지되어 보고됩니다
- 모든 파일이 하나의 `.gyb`로 병합됩니다
- 시작 노드는 메인 파일의 첫 번째 label입니다

## 출력 형식

`.gyb` 파일은 다음을 포함하는 FlatBuffers 바이너리입니다:

| 섹션 | 설명 |
|---------|-------------|
| `version` | 스키마 버전 문자열 |
| `string_pool` | 모든 고유 텍스트 문자열 (중복 제거됨) |
| `line_ids` | 번역 Line ID (string_pool과 병렬) |
| `global_vars` | 초기 변수 선언 |
| `nodes` | 모든 스토리 노드와 인스트럭션 |
| `start_node_name` | 진입점 노드 이름 |
| `characters` | 캐릭터 정의 |

전체 스키마는 [바이너리 형식](../advanced/binary-format.md)을 참고하세요.

## Line ID 형식

로컬라이제이션을 위한 자동 생성 Line ID:

```
{node_name}:{instruction_index}:{hash4}
```

- `node_name` - 포함하는 label 이름
- `instruction_index` - 0부터 시작하는 인스트럭션 위치
- `hash4` - 텍스트의 FNV-1a 해시에서 생성된 4자리 16진수

번역 가능한 텍스트(대사, 선택지 텍스트)만 Line ID를 받습니다. 구조적 텍스트(노드 이름, 변수 이름, 명령)는 빈 Line ID를 가집니다.

## 종료 코드

| 코드 | 의미 |
|------|---------|
| `0` | 성공 |
| `1` | 컴파일 에러 또는 파일 에러 |

## 빌드 위치

CMake로 빌드 후:

```
build/src/gyeol_compiler/GyeolCompiler
```
