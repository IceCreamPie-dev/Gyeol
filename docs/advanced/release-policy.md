# 릴리즈/PR 정책

이 문서는 CI 품질과 릴리즈 동작을 고정하기 위한 운영 정책을 정의합니다.

## 브랜치 보호 규칙 (수동 설정)

GitHub 저장소의 `main` 브랜치에 아래 규칙을 적용합니다.

1. `Settings > Branches > Add branch protection rule`로 이동
2. Branch name pattern: `main`
3. `Require a pull request before merging` 활성화
4. `Require status checks to pass before merging` 활성화
5. 아래 체크를 필수(required)로 지정:
   - `Core (Windows)`
6. 권장: `Require branches to be up to date before merging` 활성화

## PR 게이트 정책

`main` 대상 Pull Request에서는:

- CI의 `Core (Windows)` 잡이 하드 게이트로 실행됩니다.
- 런타임 계약(Conformance)과 성능(Perf) 비교가 모두 통과해야 병합할 수 있습니다.

## 지원 범위 / 책임 경계

- 공식 지원 플랫폼: **Windows only**
- 외부 엔진/툴 연동 구현은 파트너 책임
- 우리 팀은 Windows 코어/CLI/계약 안정성에 집중

## 산출물 정책

- 저작 소스: `.gyeol`
- 공식 결과물/배포물: `JSON IR (.json)`

## Core 런타임 성능 게이트

Core CI(Windows)에서는 런타임 성능 회귀를 hard fail로 차단합니다.

- suite 파일: `src/tests/perf/runtime_perf_suite_core.json`
- 기준선 파일: `src/tests/perf/runtime_perf_baseline_core.json`
- 측정 도구: `GyeolRuntimePerfCLI`
- 실패 기준: baseline 대비 시나리오별 `median_ns`가 `15%` 초과 증가
- 노이즈 완화: baseline의 `(p95_ns - median_ns) / median_ns`를 시나리오별 버퍼로 사용(최대 `+10%`)
- 시나리오 불일치(누락/추가)도 실패 처리
- 측정 입력: `JSON IR` fixture 4종
  - `line_loop`
  - `choice_filter`
  - `typed_command`
  - `locale_overlay`
- CI 순서: `run --suite` -> `compare --threshold 0.15`

기준선 갱신은 성능 특성 변경이 명확한 PR에서만 허용하며, 변경 근거를 PR 본문에 남겨야 합니다.

기준선 갱신 예시:

```powershell
python tools/dev/update-runtime-perf-baseline.py
```

기준선 변경 PR에는 아래 2가지를 함께 첨부해야 합니다.

1. 변경 이유(의도된 성능 변화인지)
2. compare 리포트(`logs/perf/core.compare.json` 또는 동등한 결과)

## 릴리즈 태그 정책 (SemVer)

릴리즈 태그는 아래 형식만 허용합니다.

- `vMAJOR.MINOR.PATCH`
- 정규식: `^v[0-9]+\.[0-9]+\.[0-9]+$`

예시:

- 유효: `v1.2.3`
- 무효: `v1.2`, `v1.2.3-rc1`, `v1.2.3-beta`, `version1.2.3`

릴리즈 워크플로는 시작 단계에서 `GITHUB_REF_NAME`을 검증하며, 형식이 맞지 않으면 즉시 실패합니다.

## 문서 운영 체크리스트

문서 변경 PR에서는 아래 항목을 모두 확인합니다.

1. `python tools/dev/check-docs-policy.py`가 통과해야 합니다.
2. 저작 소스는 `.gyeol` 단일 기준으로 안내하고, JSON IR은 생성 산출물로 설명해야 합니다.
3. 문서/예제에 포함된 `.gyeol` 샘플은 `GyeolCompiler --validate` 점검을 통과해야 합니다.
4. `mkdocs build --strict`가 통과해야 합니다.
5. 삭제된 구 문서 경로(스크립팅 섹션, LSP/VSCode 문서) 참조가 0건이어야 합니다.
6. `README.md` 내부 `docs/...` 링크 점검이 통과해야 합니다.
7. `mkdocs.yml`의 nav 경로가 실제 파일과 1:1로 매칭되어야 합니다.
8. [문서 스타일 가이드](../style-guide.md)의 용어 규칙 위반이 없어야 합니다.

로컬 점검 예시:

```powershell
python tools/dev/check-docs-policy.py
GyeolCompiler --validate path/to/example.gyeol
rg -n "<삭제-대상-구문서-패턴>" docs README.md mkdocs.yml -S
```
