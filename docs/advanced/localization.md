# 로컬라이제이션

Gyeol 로컬라이제이션은 `.gyeol` 저작을 기준으로, JSON IR 산출물에서 PO 방식과 Direct JSON 방식을 모두 지원합니다.

## 번역 범위

번역 대상:
- `Line` 텍스트
- `Choice` 텍스트
- 캐릭터 속성(property, 예: `displayName`, `title`)

기본 비대상:
- Command 인자
- 태그 값

## 런타임 모델

### 로케일 카탈로그 (Locale Catalog) v2

권장 런타임 입력은 locale catalog입니다.

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

조회 폴백 체인:
1. 정확 일치 로케일(exact locale, `ko-KR`)
2. 기본 언어 로케일(base locale, `ko`)
3. catalog `default_locale`
4. 원본 스토리 텍스트/속성

`Runner::setLocale()`는 진행 상태(PC/콜스택/변수)를 유지한 채 언어만 즉시 바꿉니다.

## 워크플로 A: PO 기반

```bash
# 1) JSON IR에서 POT 추출
GyeolCompiler --export-json-ir story.gyeol -o story.json
GyeolCompiler --export-strings-po-from-json-ir story.json -o strings.pot

# 2) PO 번역
#    - line msgctxt: line_id
#    - character 속성(property) msgctxt: char:<character_id>:<property_key>

# 3) PO -> locale JSON v2 변환 (스토리 키 검증 포함)
GyeolCompiler --po-to-locale-json strings_ko.po --story story.json -o ko.locale.json --locale ko

# 4) 선택: catalog 병합
GyeolCompiler --build-locale-catalog ko.locale.json en.locale.json -o locales.catalog.json --default-locale en
```

## 워크플로 B: Direct JSON 기반

```bash
# 1) JSON IR에서 템플릿 추출
GyeolCompiler --export-json-ir story.gyeol -o story.json
GyeolCompiler --export-locale-template story.json -o locale.template.json

# 2) locale JSON에 번역 채우기
# 3) 스토리 기준 검증
GyeolCompiler --validate-locale-json ko.locale.json --story story.json

# 4) 선택: catalog 빌드
GyeolCompiler --build-locale-catalog ko.locale.json en.locale.json -o locales.catalog.json --default-locale en
```

## 런타임 API

### C++ (Runner)

```cpp
runner.loadLocaleCatalog("locales.catalog.json");
runner.setLocale("ko-KR");

std::string requested = runner.getLocale();         // "ko-KR"
std::string resolved = runner.getResolvedLocale();  // exact가 없으면 "ko"

runner.clearLocale(); // 원문으로 복귀
```

## 하위 호환

- `Runner::loadLocale(path)`는 계속 지원됩니다.
  - locale JSON v1 (`version: 1`, `entries`)
  - CSV 오버레이
- 캐릭터 속성(property) 번역 + 폴백(fallback) 체인은 locale v2/catalog 경로에서만 동작합니다.
