# 빠른 시작

5분 안에 JSON IR 스토리를 검증/컴파일하고 실행해 봅니다.

## 1단계: `hello.json` 작성

```json
{
  "format": "gyeol-json-ir",
  "format_version": 2,
  "version": "0.2.0",
  "start_node_name": "start",
  "characters": [
    {
      "name": "hero",
      "properties": [
        { "key": "displayName", "value": "주인공" }
      ]
    }
  ],
  "nodes": [
    {
      "name": "start",
      "instructions": [
        { "type": "Line", "character": "hero", "text": "안녕! JSON IR로 시작해 보자." },
        { "type": "Choice", "text": "좋아", "target_node": "accept" },
        { "type": "Choice", "text": "나중에", "target_node": "decline" }
      ]
    },
    {
      "name": "accept",
      "instructions": [
        { "type": "Line", "character": "hero", "text": "좋아, 바로 출발!" }
      ]
    },
    {
      "name": "decline",
      "instructions": [
        { "type": "Line", "character": "hero", "text": "괜찮아, 준비되면 다시 시작하자." }
      ]
    }
  ]
}
```

## 2단계: JSON IR 검증

```bash
GyeolCompiler --validate-json-ir hello.json
```

오류가 없다면 다음 단계로 진행합니다.

## 3단계: `.gyb` 컴파일

```bash
GyeolCompiler --compile-json-ir hello.json -o hello.gyb
```

## 4단계: 실행

```bash
GyeolTest hello.gyb
```

## 5단계: 그래프 편집(선택)

```bash
GyeolCompiler --export-graph-json hello.json -o hello.graph.json
GyeolCompiler --apply-graph-patch hello.json --patch patch.json -o hello.patched.json
GyeolCompiler --compile-json-ir hello.patched.json -o hello.patched.gyb
```

## 다음 단계

- [JSON IR 편집 흐름](json-ir-workflow.md) - 작성/검증/패치/번역 전체 흐름
- [컴파일러 CLI](../tools/compiler.md) - 전체 옵션 레퍼런스
- [JSON IR 명세](../json-ir-spec.md) - 스키마 상세
- [Godot 연동](godot-integration.md) - Godot 4.3에서 실행하기
