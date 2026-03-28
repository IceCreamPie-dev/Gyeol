# Gyeol (결) 문서

**한번 작성하고, 어디서든 실행하는 스토리 엔진**

Gyeol은 C++17로 구축된 고성능 인터랙티브 스토리텔링 엔진입니다. canonical `gyeol-json-ir` 스토리를 기준으로 어떤 게임 엔진에서도 동일한 런타임 동작을 유지할 수 있습니다.

[:material-play-circle: **플레이그라운드**](../playground/){ .md-button .md-button--primary } — 브라우저에서 JSON IR 스토리를 작성하고 실행해 보세요.

---

## 목차

### 시작하기

- [소개](getting-started/introduction.md) - Gyeol이란?
- [설치](getting-started/installation.md) - 소스에서 빌드하기
- [빠른 시작](getting-started/quick-start.md) - 5분 만에 JSON IR 첫 실행
- [JSON IR 편집 흐름](getting-started/json-ir-workflow.md) - 작성/검증/패치/번역 워크플로
- [Godot 연동](getting-started/godot-integration.md) - Godot 4.3에서 Gyeol 사용하기

### API 레퍼런스

- [StoryPlayer (Godot)](api/class-story-player.md) - GDExtension 클래스 레퍼런스
- [Runner (C++)](api/class-runner.md) - Core VM 클래스 레퍼런스
- [Variant](api/class-variant.md) - 변수 타입 시스템

### 도구

- [컴파일러 CLI](tools/compiler.md) - `GyeolCompiler` 명령어 레퍼런스
- [디버거](tools/debugger.md) - `GyeolDebugger` 인터랙티브 디버거

### 심화

- [아키텍처](advanced/architecture.md) - 엔진 내부 구조 개요
- [Runtime Contract v1.1](advanced/runtime-contract.md) - 실행/이벤트/상태 합치 규약
- [저장과 불러오기](advanced/save-load.md) - 상태 직렬화
- [다국어 지원](advanced/localization.md) - 다국어 로케일 시스템
- [바이너리 포맷](advanced/binary-format.md) - `.gyb` / `.gys` FlatBuffers 스키마

## 문서 경로 정책

- 문서는 한국어 단일 체계로 운영합니다.
- 문서 파일명은 기본 `.md`를 사용합니다(이전 언어 접미사 방식 미사용).
- 교차 링크 작성 시 항상 현재 경로 기준의 `.md` 경로를 사용합니다.
- 문서 작성/용어 기준은 [문서 스타일 가이드](style-guide.md)를 기준으로 합니다.
