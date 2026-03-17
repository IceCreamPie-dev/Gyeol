# 컴파일러 CLI

`GyeolCompiler`는 `.gyeol` 스크립트를 컴파일하고, 번역/그래프 툴링용 산출물을 생성합니다.

## 사용법

```bash
GyeolCompiler <input.gyeol> [options]
GyeolCompiler --po-to-json <input.po> -o <output.json> [--locale <code>]
GyeolCompiler <input.gyeol> --export-graph-json <output.graph.json>
GyeolCompiler <input.gyeol> --validate-graph-patch <patch.json>
GyeolCompiler <input.gyeol> --apply-graph-patch <patch.json> -o <output.gyeol>
```

## 옵션

| 옵션 | 설명 |
|------|------|
| `-o <path>` | 출력 파일 경로 |
| `--format <fmt>` | 출력 포맷 (`gyb`, `json`) |
| `--export-strings <path>` | 번역 CSV 추출 (하위 호환) |
| `--export-strings-po <path>` | 번역 POT 추출 |
| `--po-to-json <path>` | PO를 런타임 JSON locale로 변환 |
| `--locale <code>` | `--po-to-json` locale 코드 지정 |
| `--export-graph-json <path>` | 그래프 계약 JSON(`gyeol-graph-doc`, v1) 내보내기 |
| `--validate-graph-patch <path>` | 그래프 패치 JSON(`gyeol-graph-patch`, v1/v2) 검증 |
| `--apply-graph-patch <path>` | 그래프 패치(v1/v2) 적용 후 canonical `.gyeol` 출력 |
| `--preserve-line-id` | `--apply-graph-patch`와 함께 line-id 맵(`*.lineidmap.json`) 생성 |
| `--line-id-map <path>` | 컴파일 시 `gyeol-line-id-map` v1 적용 |
| `--analyze [path]` | 정적 분석 리포트 출력 |
| `-O` | 최적화 적용 |
| `-h`, `--help` | 도움말 |
| `--version` | 버전 |

## 예시

```bash
# 기본 컴파일
GyeolCompiler story.gyeol -o story.gyb

# POT 추출
GyeolCompiler story.gyeol --export-strings-po strings.pot

# PO -> JSON locale
GyeolCompiler --po-to-json strings_ko.po -o ko.locale.json --locale ko

# 그래프 문서 export
GyeolCompiler story.gyeol --export-graph-json story.graph.json

# 그래프 패치 validate
GyeolCompiler story.gyeol --validate-graph-patch patch.json

# 그래프 패치 apply (실패 시 출력 파일 미생성)
GyeolCompiler story.gyeol --apply-graph-patch patch.json -o story.patched.gyeol

# 그래프 패치 적용 + 기존 line_id 보존 맵 생성
GyeolCompiler story.gyeol --apply-graph-patch patch.json --preserve-line-id -o story.patched.gyeol

# 보존 맵을 사용해 재컴파일
GyeolCompiler story.patched.gyeol --line-id-map story.patched.gyeol.lineidmap.json -o story.patched.gyb
```

## 그래프 계약 스키마 헤더

### Graph Doc

```json
{
  "format": "gyeol-graph-doc",
  "version": 1,
  "start_node": "start"
}
```

`gyeol-graph-doc`에는 결정론적 `edge_id`와 노드별 `instructions[]`(예: `instruction_id = "n0:i3"`)가 포함되어 v2 패치에서 안정적으로 참조할 수 있습니다.

### Graph Patch

```json
{
  "format": "gyeol-graph-patch",
  "version": 2,
  "ops": []
}
```

v1 지원 연산:
- `add_node`
- `rename_node` (모든 참조 자동 갱신)
- `delete_node` (`redirect_target` 필수)
- `retarget_edge` (`edge_id` 필수)
- `set_start_node`

v2 지원 연산:
- `update_line_text`
- `update_choice_text`
- `update_command`
- `update_expression`
- `insert_instruction`
- `delete_instruction`
- `move_instruction`

`instruction_id`는 patch 시작 스냅샷 기준으로 해석되며, 같은 patch에서 삽입된 instruction은 `instruction_id`로 재참조할 수 없습니다.
