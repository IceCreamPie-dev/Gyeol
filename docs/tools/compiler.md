# 컴파일러 CLI

`GyeolCompiler`는 JSON IR 전용 파이프라인(검증, 패치, 컴파일, 번역)을 제공하는 도구입니다.

## 사용법

```bash
GyeolCompiler --init-json-ir <story.json>
GyeolCompiler --lint-json-ir <story.json> [--format text|json]
GyeolCompiler --validate-json-ir <story.json>
GyeolCompiler --format-json-ir <story.json> [-o <story.json>]
GyeolCompiler --compile-json-ir <story.json> -o <story.gyb>
GyeolCompiler --export-graph-json <story.json> -o <story.graph.json>
GyeolCompiler --preview-graph-patch <story.json> --patch <patch.json> [--format text|json]
GyeolCompiler --apply-graph-patch <story.json> --patch <patch.json> -o <story.json>

GyeolCompiler --export-strings-po-from-json-ir <story.json> -o <strings.pot>
GyeolCompiler --export-locale-template <story.json> -o <locale.template.json>
GyeolCompiler --po-to-locale-json <strings.po> --story <story.json> -o <locale.json> [--locale <code>]
GyeolCompiler --validate-locale-json <locale.json> --story <story.json>
GyeolCompiler --build-locale-catalog <localeA.json> <localeB.json> ... -o <catalog.json> [--default-locale <code>]
```

## 핵심 명령

| 명령 | 설명 |
|------|------|
| `--init-json-ir <story.json>` | 최소 실행 가능한 JSON IR 템플릿 생성 |
| `--lint-json-ir <story.json> [--format text\|json]` | JSON IR 품질 검사(참조/타입/빈 값 등) |
| `--validate-json-ir <story.json>` | `gyeol-json-ir` (`format_version: 2`) 검증 |
| `--format-json-ir <story.json> [-o <story.json>]` | canonical 정렬/포맷으로 JSON IR 재출력 |
| `--compile-json-ir <story.json> -o <story.gyb>` | JSON IR을 `.gyb`로 컴파일 |
| `--export-graph-json <story.json> -o <story.graph.json>` | 그래프 계약 JSON(`gyeol-graph-doc`) 내보내기 |
| `--preview-graph-patch <story.json> --patch <patch.json> [--format text\|json]` | 패치 적용 전 변경 요약(dry-run) |
| `--apply-graph-patch <story.json> --patch <patch.json> -o <story.json>` | 그래프 패치 적용 후 canonical JSON IR 출력 |

## 번역 명령

| 명령 | 설명 |
|------|------|
| `--export-strings-po-from-json-ir <story.json> -o <strings.pot>` | JSON IR에서 POT 추출 (`line_id` + 캐릭터 속성(property) 키 포함) |
| `--export-locale-template <story.json> -o <locale.template.json>` | Direct JSON 번역 템플릿(v2) 생성 |
| `--po-to-locale-json <strings.po> --story <story.json> -o <locale.json> [--locale <code>]` | 스토리 키 검증 포함 PO -> locale JSON v2 변환 |
| `--validate-locale-json <locale.json> --story <story.json>` | locale JSON의 키/타입을 스토리 기준 검증 |
| `--build-locale-catalog <localeA.json> <localeB.json> ... -o <catalog.json> [--default-locale <code>]` | 여러 locale JSON을 catalog로 병합 |

## 그래프 패치 옵션

| 옵션 | 설명 |
|------|------|
| `--preserve-line-id` | `--apply-graph-patch`와 함께 `<output>.lineidmap.json` 생성 |
| `--line-id-map-out <path>` | line-id 맵 출력 경로 지정 |

## 진단 출력 규격

`--lint-json-ir --format json`과 패치 프리뷰 오류 출력은 아래 필드를 공통으로 사용합니다.

- `code`
- `severity`
- `path`
- `node`
- `instruction_index`
- `message`
- `hint`

예시:

```json
{
  "diagnostics": [
    {
      "code": "IR_TARGET_MISSING",
      "severity": "error",
      "path": "story.json",
      "node": "start",
      "instruction_index": 4,
      "message": "Choice의 target_node가 존재하지 않습니다.",
      "hint": "target_node를 존재하는 노드 이름으로 수정하세요."
    }
  ],
  "summary": {
    "errors": 1,
    "warnings": 0,
    "total": 1
  }
}
```

## 호환 명령

| 명령 | 설명 |
|------|------|
| `--po-to-json <input.po> -o <output.json> [--locale <code>]` | 레거시 locale v1 변환기 |

## 예시

### JSON IR 검증/컴파일

```bash
GyeolCompiler --init-json-ir story.json
GyeolCompiler --lint-json-ir story.json
GyeolCompiler --validate-json-ir story.json
GyeolCompiler --compile-json-ir story.json -o story.gyb
```

### 그래프 편집 워크플로 (JSON IR canonical)

```bash
# JSON IR에서 그래프 문서(graph doc) 추출
GyeolCompiler --export-graph-json story.json -o story.graph.json

# 패치 적용 전 변경량 미리 확인
GyeolCompiler --preview-graph-patch story.json --patch patch.json --format text

# 패치(patch) 적용 후 canonical JSON IR 재출력
GyeolCompiler --apply-graph-patch story.json --patch patch.json -o story.patched.json

# line_id 보존 맵도 함께 생성
GyeolCompiler --apply-graph-patch story.json --patch patch.json --preserve-line-id -o story.patched.json
```

### PO + Direct JSON 번역

```bash
# JSON IR 기반 POT 추출
GyeolCompiler --export-strings-po-from-json-ir story.json -o strings.pot

# PO -> locale JSON v2
GyeolCompiler --po-to-locale-json strings_ko.po --story story.json -o ko.locale.json --locale ko

# 로케일 JSON 검증
GyeolCompiler --validate-locale-json ko.locale.json --story story.json

# 런타임 핫스위치/폴백용 catalog 빌드
GyeolCompiler --build-locale-catalog ko.locale.json en.locale.json -o locales.catalog.json --default-locale en
```

### 로케일 v2 단일 파일 형태

```json
{
  "format": "gyeol-locale",
  "version": 2,
  "locale": "ko",
  "line_entries": {
    "start:0:a3f2": "안녕하세요"
  },
  "character_entries": {
    "hero": {
      "displayName": "주인공"
    }
  }
}
```

### 로케일 카탈로그(locale catalog) v2 형태

```json
{
  "format": "gyeol-locale-catalog",
  "version": 2,
  "default_locale": "en",
  "locales": {
    "ko": {
      "line_entries": {
        "start:0:a3f2": "안녕하세요"
      },
      "character_entries": {
        "hero": {
          "displayName": "주인공"
        }
      }
    }
  }
}
```

## 종료 코드

| 코드 | 의미 |
|------|------|
| `0` | 성공 |
| `1` | 검증/컴파일/입출력 오류 |

## 빌드 경로

CMake 빌드 후:

```
build/src/gyeol_compiler/GyeolCompiler
```
