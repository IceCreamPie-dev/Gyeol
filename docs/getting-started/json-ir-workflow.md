# JSON IR 도구 흐름

이 문서는 `.gyeol`에서 생성된 `JSON IR`을 대상으로 그래프 패치/번역 등 고급 툴 체인을 사용하는 흐름을 정리합니다.
일반 저작은 `.gyeol` 기준으로 진행하고, JSON IR은 산출물로 다룹니다.

## 표준 저작 루프

권장 루프는 아래 순서입니다.

1. `.gyeol`에서 `--export-json-ir`로 산출물 생성
2. 필요 시 `--lint-json-ir`/`--validate-json-ir`로 JSON IR 점검
3. 그래프 패치 전 `--preview-graph-patch`로 변경 요약 확인
4. `--apply-graph-patch` 후 `--validate-json-ir`로 결과 정합성 확인

## 1) `.gyeol`에서 JSON IR 생성

```bash
GyeolCompiler --export-json-ir story.gyeol -o story.json
```
생성된 `story.json`은 JSON IR 도구 체인 입력으로 사용합니다.

## 2) Lint/검증 점검

```bash
GyeolCompiler --lint-json-ir story.json
GyeolCompiler --lint-json-ir story.json --format json
GyeolCompiler --validate-json-ir story.json
```

lint는 참조 누락, 빈 텍스트, 잘못된 target 같은 품질 문제를 `code/severity/message/hint` 구조로 반환합니다.

## 3) 그래프 문서/패치 사전 확인

```bash
GyeolCompiler --export-graph-json story.json -o story.graph.json
GyeolCompiler --preview-graph-patch story.json --patch patch.json
GyeolCompiler --apply-graph-patch story.json --patch patch.json -o story.patched.json
```

패치 적용 후 결과는 다시 canonical JSON IR로 출력됩니다.

## 4) 패치 결과 검증

```bash
GyeolCompiler --validate-json-ir story.json
```

검증된 JSON IR은 공식 배포/연동 산출물로 사용합니다.

## 5) 번역 추출/적용

```bash
GyeolCompiler --export-strings-po-from-json-ir story.json -o strings.pot
GyeolCompiler --po-to-locale-json strings_ko.po --story story.json -o ko.locale.json --locale ko
GyeolCompiler --build-locale-catalog ko.locale.json en.locale.json -o locales.catalog.json --default-locale en
```

런타임에서 locale 전환은 `loadLocaleCatalog` + `setLocale` 경로를 사용합니다.

## 6) 런타임 계약 검증

실행 계약 점검은 [Runtime Contract v1.1](../advanced/runtime-contract.md) 기준 CLI로 수행합니다.
