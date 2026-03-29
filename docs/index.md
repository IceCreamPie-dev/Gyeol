# Gyeol (결) 문서

**Windows 전용 스토리 엔진 코어/툴체인**

Gyeol은 C++17로 구축된 고성능 인터랙티브 스토리텔링 엔진입니다. 사람이 작성하는 `.gyeol` 스크립트를 기준으로 JSON IR 산출물과 런타임 계약을 제공합니다.

---

## 목차

### 시작하기

- [소개](getting-started/introduction.md) - Gyeol이란?
- [설치](getting-started/installation.md) - 소스에서 빌드하기
- [빠른 시작](getting-started/quick-start.md) - 5분 만에 `.gyeol` 첫 실행
- [JSON IR 도구 흐름](getting-started/json-ir-workflow.md) - 산출물 기반 패치/번역 워크플로

### API 레퍼런스

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

## 문서 경로 정책

- 문서는 한국어 단일 체계로 운영합니다.
- 문서 파일명은 기본 `.md`를 사용합니다(이전 언어 접미사 방식 미사용).
- 교차 링크 작성 시 항상 현재 경로 기준의 `.md` 경로를 사용합니다.
- 문서 작성/용어 기준은 [문서 스타일 가이드](style-guide.md)를 기준으로 합니다.
