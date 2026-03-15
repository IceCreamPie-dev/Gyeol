# 로컬라이제이션

Gyeol은 안정적인 `line_id`를 기준으로 다국어 텍스트를 오버레이합니다.

## 권장 워크플로 (PO + JSON)

```
1) 스토리 작성 -> 2) POT 추출 -> 3) PO 번역 -> 4) PO를 JSON으로 변환 -> 5) 런타임 로드
```

1. `.gyeol` 스크립트를 작성합니다.
2. 번역 템플릿(POT)을 추출합니다.
3. 번역자는 PO에서 `msgstr`를 채웁니다.
4. PO를 런타임 JSON locale로 변환합니다.
5. 게임에서 locale 파일을 로드합니다.

## 1단계: POT 추출

```bash
GyeolCompiler story.gyeol --export-strings-po strings.pot
```

각 번역 항목은 다음처럼 생성됩니다:

```po
msgctxt "start:0:a3f2"
msgid "Hello, how are you?"
msgstr ""
```

## 2단계: PO 번역

- `msgctxt`는 `line_id`입니다.
- `msgstr`에 번역문을 입력합니다.
- v1에서는 `#, fuzzy` 항목을 변환에서 제외합니다.

## 3단계: PO -> JSON 변환

```bash
GyeolCompiler --po-to-json strings_ko.po -o ko.locale.json --locale ko
```

출력 JSON 스키마:

```json
{
  "version": 1,
  "locale": "ko",
  "entries": {
    "start:0:a3f2": "안녕하세요!"
  }
}
```

규칙:
- `version`은 필수이며 현재 `1`만 지원합니다.
- `entries`는 필수입니다 (`line_id -> 번역 문자열`).
- 빈 번역(`msgstr == ""`)은 적용하지 않습니다.
- 스토리에 없는 `line_id`는 무시합니다.

## 4단계: 런타임 로드

### C++ (Runner)

```cpp
runner.loadLocale("ko.locale.json");
runner.clearLocale();
```

### GDScript (StoryPlayer)

```gdscript
story_player.load_locale("res://locales/ko.locale.json")
story_player.clear_locale()
```

## 하위 호환

기존 CSV 방식도 계속 지원합니다:

```bash
GyeolCompiler story.gyeol --export-strings strings.csv
```

`Runner::loadLocale(path)`는 확장자로 자동 인식합니다:
- `.json` -> JSON locale
- 그 외 확장자 -> CSV locale
