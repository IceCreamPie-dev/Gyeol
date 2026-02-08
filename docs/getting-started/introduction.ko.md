# 소개

## Gyeol이란?

Gyeol (결)은 C++17로 구축된 인터랙티브 스토리텔링 미들웨어 엔진입니다. 게임에서 분기형 내러티브를 작성, 컴파일, 실행하기 위한 완전한 파이프라인을 제공합니다.

"결"이라는 이름은 한국어로 "결(texture, grain)"을 의미하며, 어떤 게임에든 풍부한 내러티브 줄기를 엮어 넣겠다는 엔진의 목표를 담고 있습니다.

### 설계 철학

Gyeol은 세 가지 인기 있는 내러티브 도구의 장점을 결합합니다:

| 특징 | 영감을 받은 도구 |
|------|-----------------|
| 캐릭터 중심 대사 문법 | Ren'Py |
| 구조적 노드/그래프 시스템 | Ink |
| 엔진 독립적 미들웨어 설계 | Yarn Spinner |

### 주요 기능

- **단일 스크립트 저작** - Ren'Py 스타일 문법의 `.gyeol` 텍스트 파일 작성
- **바이너리 컴파일** - FlatBuffers 포맷의 `.gyb`로 컴파일하여 zero-copy 로딩
- **엔진 독립적 VM** - Runner VM은 엔진 의존성 없는 순수 C++
- **표현식 엔진** - 산술, 논리, 문자열 보간, 인라인 조건 완벽 지원
- **함수 시스템** - 매개변수, 반환값, 로컬 스코프를 갖춘 Label 함수
- **방문 추적** - `visit_count()` / `visited()`로 노드 방문 횟수 자동 추적
- **다국어 지원** - 자동 생성 Line ID, CSV 기반 로케일 오버레이
- **저장/불러오기** - 완전한 상태 직렬화 (변수, 콜 스택, 방문 횟수)
- **랜덤 분기** - 가중치 기반 확률 분기 및 결정적 시드 설정
- **개발 도구** - LSP 서버, CLI 디버거, VS Code 확장

### 지원 플랫폼

| 플랫폼 | 상태 | 바인딩 |
|--------|------|--------|
| Godot 4.3 | 사용 가능 | GDExtension (`StoryPlayer` 노드) |
| Unity | 계획 중 | Native Plugin |
| WebAssembly | 계획 중 | Emscripten 빌드 |
| 콘솔 (CLI) | 사용 가능 | `GyeolTest` 플레이어 |

### 동작 방식

```
.gyeol script  -->  GyeolCompiler  -->  .gyb binary  -->  Runner VM  -->  Game Engine
  (text)              (parser)          (FlatBuffers)      (C++ VM)        (Godot/Unity/...)
```

1. 대사, 선택지, 변수, 로직이 포함된 `.gyeol` 스크립트를 **작성**합니다
2. CLI 컴파일러를 사용하여 `.gyb` 바이너리로 **컴파일**합니다
3. 게임 엔진의 바인딩을 통해 Runner VM에 바이너리를 **로드**합니다
4. 이벤트 기반 `step()`/`choose()` API로 스토리를 **플레이**합니다

### 다음 단계

- [설치](installation.md) - 소스에서 Gyeol 빌드하기
- [빠른 시작](quick-start.md) - 첫 스토리 작성하기
- [스크립트 문법](../scripting/syntax.md) - 전체 언어 레퍼런스
