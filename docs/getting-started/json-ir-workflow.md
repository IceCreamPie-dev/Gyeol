# JSON IR 편집 흐름

이 문서는 Gyeol의 canonical 소스 포맷인 `JSON IR` 기준으로 편집부터 번역까지의 전체 흐름을 정리합니다.

## 표준 저작 루프

권장 루프는 아래 순서입니다.

1. `--init-json-ir`로 템플릿 생성
2. 내용 편집 후 `--lint-json-ir`로 품질 점검
3. `--validate-json-ir`로 계약 검증
4. 그래프 패치 전 `--preview-graph-patch`로 변경 요약 확인
5. `--compile-json-ir`로 실행 바이너리 생성

## 1) 템플릿 초기화

```bash
GyeolCompiler --init-json-ir story.json
```

생성된 파일은 즉시 `--validate-json-ir`를 통과하는 최소 실행 템플릿입니다.

## 2) JSON IR 편집

필수 루트 필드:

- `format: "gyeol-json-ir"`
- `format_version: 2`
- `start_node_name`
- `nodes`

자세한 구조는 [JSON IR 명세](../json-ir-spec.md)를 참고하세요.

## 3) Lint 점검

```bash
GyeolCompiler --lint-json-ir story.json
GyeolCompiler --lint-json-ir story.json --format json
```

lint는 참조 누락, 빈 텍스트, 잘못된 target 같은 품질 문제를 `code/severity/message/hint` 구조로 반환합니다.

## 4) 계약 검증

```bash
GyeolCompiler --validate-json-ir story.json
```

검증 단계에서 필수 필드 누락, 타입 불일치, 참조 오류를 최종 확인합니다.

## 5) 그래프 문서/패치 사전 확인

```bash
GyeolCompiler --export-graph-json story.json -o story.graph.json
GyeolCompiler --preview-graph-patch story.json --patch patch.json
GyeolCompiler --apply-graph-patch story.json --patch patch.json -o story.patched.json
```

패치 적용 후 결과는 다시 canonical JSON IR로 출력됩니다.

## 6) 바이너리 컴파일

```bash
GyeolCompiler --compile-json-ir story.json -o story.gyb
```

생성된 `.gyb`는 `Runner` VM/Godot 바인딩/WASM 런타임에서 공통으로 사용됩니다.

## 7) 번역 추출/적용

```bash
GyeolCompiler --export-strings-po-from-json-ir story.json -o strings.pot
GyeolCompiler --po-to-locale-json strings_ko.po --story story.json -o ko.locale.json --locale ko
GyeolCompiler --build-locale-catalog ko.locale.json en.locale.json -o locales.catalog.json --default-locale en
```

런타임에서 locale 전환은 `loadLocaleCatalog` + `setLocale` 경로를 사용합니다.

## 8) 실행 검증

```bash
GyeolTest story.gyb
```

엔진 연동 시에는 [StoryPlayer API](../api/class-story-player.md) 또는 [Runner API](../api/class-runner.md)를 사용합니다.
