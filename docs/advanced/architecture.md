# 아키텍처

## 개요

Gyeol의 공식 운영 정책은 아래 3가지입니다.

- 저작 소스: `.gyeol`
- 공식 산출물/교환 포맷: `gyeol-json-ir` (`format_version: 2`)
- 공식 지원 범위: Windows Core

외부 엔진 연동은 파트너 구현 범위이며, 본 저장소는 코어 VM/컴파일러/계약 검증 도구를 제공합니다.

## 실행 파이프라인

```text
.gyeol (authoring)
  -> Parser
  -> StoryT
  -> JsonExport
  -> story.json (JSON IR, official output)
  -> JsonIrReader
  -> in-memory runtime buffer (internal)
  -> Runner VM
```

- 사용자/배포 경로에서는 내부 바이너리 파일을 공식 산출물로 취급하지 않습니다.
- 런타임 도구(Contract/Perf)는 JSON IR 입력을 기준으로 동작합니다.

## 핵심 컴포넌트

### Parser (`gyeol_parser.cpp`)
- `.gyeol` 문법 파싱
- 에러/경고 수집
- StoryT 구성

### JsonExport / JsonIrReader
- `JsonExport`: StoryT -> canonical JSON IR
- `JsonIrReader`: JSON IR -> StoryT/런타임 버퍼
- graph patch/locale tooling과 동일 데이터 모델 공유

### Runner VM (`gyeol_runner.cpp`)
- `step()` / `choose()` / `resume()` 기반 이벤트 실행
- 변수/콜스택/방문 카운트/선택지 수식어 처리
- locale catalog 기반 텍스트/캐릭터 속성 오버레이 및 fallback
- save/load(`.gys`) 상태 직렬화

## Runtime Contract

실행 동작은 [Runtime Contract v1.1](runtime-contract.md)로 고정합니다.

- StepType: `LINE`, `CHOICES`, `COMMAND`, `WAIT`, `YIELD`, `END`
- WAIT 이후 `resume()` 강제
- transcript golden 비교로 회귀 차단
- 성능 게이트(15%)를 별도 hard fail로 운영

## 빌드 타깃

```text
GyeolCore      # Runner/Story core
GyeolParser    # Parser + JSON IR/Graph/Locale tooling
GyeolCompiler  # CLI
GyeolLSP       # LSP 서버
GyeolDebugger  # CLI 디버거 (JSON IR 입력)
GyeolTests     # 단위 테스트
GyeolRuntimeContractCLI
GyeolRuntimePerfCLI
```

## 설계 원칙

1. 데이터 계약 우선: JSON IR 스키마와 runtime contract를 함께 고정
2. 실행 결정성: seed/상태 저장/복원/합치 transcript로 재현성 보장
3. 운영 단순화: Windows Core 하드 게이트 중심으로 CI/릴리즈 단순 유지
