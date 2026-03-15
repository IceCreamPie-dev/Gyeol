# 컴파일러 CLI

`GyeolCompiler`는 `.gyeol` 스크립트를 `.gyb`로 컴파일하고, 번역 자산을 추출/변환합니다.

## 사용법

```bash
GyeolCompiler <input.gyeol> [options]
GyeolCompiler --po-to-json <input.po> -o <output.json> [--locale <code>]
```

## 옵션

| 옵션 | 설명 |
|------|------|
| `-o <path>` | 출력 파일 경로 |
| `--format <fmt>` | 출력 포맷 (`gyb`, `json`) |
| `--export-strings <path>` | 번역 문자열 CSV 추출 (하위 호환) |
| `--export-strings-po <path>` | 번역 템플릿 POT 추출 |
| `--po-to-json <path>` | PO 파일을 런타임 JSON locale로 변환 |
| `--locale <code>` | `--po-to-json` 결과의 locale 코드 지정 |
| `--analyze [path]` | 정적 분석 리포트 출력 |
| `-O` | 최적화 적용 |
| `-h`, `--help` | 도움말 출력 |
| `--version` | 버전 출력 |

## 예시

```bash
# 기본 컴파일
GyeolCompiler story.gyeol -o story.gyb

# POT 추출
GyeolCompiler story.gyeol --export-strings-po strings.pot

# PO -> JSON locale 변환
GyeolCompiler --po-to-json strings_ko.po -o ko.locale.json --locale ko
```

## JSON locale 스키마

```json
{
  "version": 1,
  "locale": "ko",
  "entries": {
    "start:0:a3f2": "안녕하세요!"
  }
}
```

## 종료 코드

| 코드 | 의미 |
|------|------|
| `0` | 성공 |
| `1` | 입력/컴파일/변환 오류 |
