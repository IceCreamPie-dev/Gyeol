# 빠른 시작

5분 안에 `.gyeol` 스토리를 검증하고 JSON IR 산출물까지 만들어 봅니다.

## 1단계: `hello.gyeol` 작성

```text
label start:
    hero "안녕! Gyeol로 시작해 보자."
    menu:
        "좋아" -> accept
        "나중에" -> decline

label accept:
    hero "좋아, 바로 출발!"

label decline:
    hero "괜찮아, 준비되면 다시 시작하자."
```

## 2단계: `.gyeol` 검증

```bash
GyeolCompiler --validate hello.gyeol
```

오류가 없다면 다음 단계로 진행합니다.

## 3단계: JSON IR 산출

```bash
GyeolCompiler --export-json-ir hello.gyeol -o hello.json
GyeolCompiler --validate-json-ir hello.json
```

JSON IR은 공식 결과물/툴 연동용 산출물입니다.

## 다음 단계

- [컴파일러 CLI](../tools/compiler.md) - 전체 옵션 레퍼런스
- [JSON IR 도구 흐름](json-ir-workflow.md) - 고급 툴 체인용 JSON IR 경로
- [JSON IR 명세](../json-ir-spec.md) - 스키마 상세
- [Runner API](../api/class-runner.md) - 런타임 계약/실행 API
