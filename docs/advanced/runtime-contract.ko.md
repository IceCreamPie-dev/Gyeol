# Runtime Contract v1.1

`Runtime Contract v1.1`은 `WAIT`/`YIELD`와 `resume()`을 정식 도입한 strict-breaking 런타임 계약입니다. Core, WASM, Godot adapter가 동일 규약을 따릅니다.

## 범위

- 공개 런타임 API 변경:
- `StepType`이 `LINE`, `CHOICES`, `COMMAND`, `WAIT`, `YIELD`, `END`로 확장됩니다.
- `Runner::resume()`이 추가됩니다.
- 계약 고정은 문서 + conformance 테스트 + CI 게이트로 수행합니다.

## 실행 규칙

- `step()`은 `LINE`, `CHOICES`, `COMMAND`, `WAIT`, `YIELD`, `END` 중 하나를 반환합니다.
- `WAIT` 이후에는 `resume()` 전까지 진행이 금지됩니다.
- WAIT 상태에서 `step()`/`choose()` 호출은 오류이며 `last_error`를 설정해야 합니다.
- `YIELD`는 1틱 양보 이벤트이며 다음 `step()`에서 즉시 진행됩니다.
- `choose(index)`는 직전 가시 결과가 `CHOICES`일 때만 유효합니다.

## 결정론과 상태

- `setSeed(seed)`는 랜덤 분기를 결정론적으로 고정해야 합니다.
- `saveState/loadState`, `snapshot/restore`는 WAIT 상태(`wait_blocked`, `wait_tag`)를 포함해 동치 상태를 복원해야 합니다.
- locale 오버레이는 텍스트만 바꿔야 하며 제어 흐름 타입은 변경하면 안 됩니다.

## Conformance Transcript 스키마 (v1.1)

`v1.1`은 action/transcript 스키마 버전 `2`를 사용합니다.

```json
{
  "format": "gyeol-runtime-transcript",
  "version": 2,
  "engine": "core|wasm|godot_adapter",
  "steps": [
    {
      "action": "step|choose|resume|set_seed|save|load|snapshot|restore|clear_last_error",
      "result": { "type": "LINE|CHOICES|COMMAND|WAIT|YIELD|END" },
      "state": {
        "finished": false,
        "current_node": "start",
        "variables": {}
      }
    }
  ]
}
```

## Conformance CLI

```bash
GyeolRuntimeContractCLI generate \
  --engine core \
  --story src/tests/conformance/runtime_contract_v1_story.gyeol \
  --actions src/tests/conformance/runtime_contract_v1_actions_cross.json \
  --output logs/conformance/core.actual.json

GyeolRuntimeContractCLI compare \
  --expected src/tests/conformance/runtime_contract_v1_golden_core_cross.json \
  --actual logs/conformance/core.actual.json \
  --expected-engine core \
  --expected-out logs/conformance/core.expected.json \
  --actual-out logs/conformance/core.actual.json \
  --diff-out logs/conformance/core.diff.json
```

WASM conformance는 `bindings/wasm/tests/runtime_contract_conformance.js`를 사용하며 동일하게 `expected/actual/diff` 아티팩트를 생성합니다.

## 로컬 표준 게이트

Windows 개발 환경에서 CI 검증 순서를 로컬에서 재현하려면 아래 순서를 사용합니다.

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
.\tools\dev\build-wasm.ps1
.\tools\dev\build-godot.ps1
.\tools\dev\check-runtime-contract.ps1
```

## Golden 운영 규칙

- 단일 기준 파일: `src/tests/conformance/runtime_contract_v1_golden_core_cross.json`
- 계약이 의도적으로 바뀌는 경우에만 golden 갱신을 허용합니다.
- 리팩터링 전용 PR에서는 golden 변경을 금지합니다.
- golden 갱신 시 transcript diff 근거와 변경 의도를 함께 남깁니다.
