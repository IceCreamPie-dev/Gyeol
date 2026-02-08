# Gyeol (결) 문서

**한번 작성하고, 어디서든 실행하는 스토리 엔진**

Gyeol은 C++17로 구축된 고성능 인터랙티브 스토리텔링 엔진입니다. `.gyeol` 스크립트 하나만 작성하면 어떤 게임 엔진에서든 수정 없이 바로 플레이할 수 있습니다.

---

## 목차

### 시작하기

- [소개](getting-started/introduction.md) - Gyeol이란?
- [설치](getting-started/installation.md) - 소스에서 빌드하기
- [빠른 시작](getting-started/quick-start.md) - 5분 만에 첫 스토리 만들기
- [Godot 연동](getting-started/godot-integration.md) - Godot 4.3에서 Gyeol 사용하기

### 스크립팅

- [스크립트 문법](scripting/syntax.md) - `.gyeol` 언어 전체 레퍼런스
- [변수와 표현식](scripting/variables-and-expressions.md) - 변수, 산술 연산, 논리 연산
- [흐름 제어](scripting/flow-control.md) - 분기, 조건문, 메뉴, 랜덤
- [함수](scripting/functions.md) - 매개변수, call/return, 로컬 스코프
- [고급 기능](scripting/advanced-features.md) - 문자열 보간, 인라인 조건, 태그, 선택지 수식어

### API 레퍼런스

- [StoryPlayer (Godot)](api/class-story-player.md) - GDExtension 클래스 레퍼런스
- [Runner (C++)](api/class-runner.md) - Core VM 클래스 레퍼런스
- [Variant](api/class-variant.md) - 변수 타입 시스템

### 도구

- [Compiler CLI](tools/compiler.md) - `GyeolCompiler` 명령어 레퍼런스
- [Debugger](tools/debugger.md) - `GyeolDebugger` 인터랙티브 디버거
- [LSP 서버](tools/lsp.md) - 에디터용 Language Server
- [VS Code 확장](tools/vscode.md) - 에디터 연동

### 심화

- [아키텍처](advanced/architecture.md) - 엔진 내부 구조 개요
- [저장과 불러오기](advanced/save-load.md) - 상태 직렬화
- [다국어 지원](advanced/localization.md) - 다국어 로케일 시스템
- [바이너리 포맷](advanced/binary-format.md) - `.gyb` / `.gys` FlatBuffers 스키마
