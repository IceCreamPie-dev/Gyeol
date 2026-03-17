# Runtime Contract v1.1

`Runtime Contract v1.1` is a strict-breaking runtime contract that adds `WAIT`/`YIELD` and `resume()` semantics across Core, WASM, and Godot adapter conformance.

## Scope

- Public runtime API changes:
- `StepType` expands to `LINE`, `CHOICES`, `COMMAND`, `WAIT`, `YIELD`, `END`.
- `Runner::resume()` is added.
- Contract is enforced by docs + conformance tests + CI gates.

## Execution Rules

- `step()` returns one of `LINE`, `CHOICES`, `COMMAND`, `WAIT`, `YIELD`, `END`.
- `WAIT` blocks progression; `resume()` must be called before execution can continue.
- Calling `step()` or `choose()` while waiting is invalid and must set `last_error`.
- `YIELD` is a one-tick cooperative yield event; next `step()` proceeds immediately.
- `choose(index)` is only valid when the previous visible result is `CHOICES`.

## Determinism and State

- `setSeed(seed)` must produce deterministic random branching.
- `saveState/loadState` and `snapshot/restore` must preserve equivalent continuation state, including WAIT state (`wait_blocked`, `wait_tag`).
- Locale overlays may change text payloads, but must not change control-flow result types.

## Conformance Transcript Schema (v1.1)

`v1.1` uses transcript/action schema version `2`.

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

WASM conformance uses `bindings/wasm/tests/runtime_contract_conformance.js` with the same `expected/actual/diff` artifact contract.

## Local Standard Gate

Use this sequence to reproduce the CI verification flow on a Windows dev machine:

```powershell
.\tools\dev\bootstrap-toolchains.ps1
.\tools\dev\activate-toolchains.ps1
.\tools\dev\doctor-toolchains.ps1
.\tools\dev\build-wasm.ps1
.\tools\dev\build-godot.ps1
.\tools\dev\check-runtime-contract.ps1
```

## Golden Policy

- Single source of truth: `src/tests/conformance/runtime_contract_v1_golden_core_cross.json`
- Golden updates are allowed only for intentional contract changes.
- Refactor-only PRs must keep golden unchanged.
- Golden updates must include transcript diff evidence and rationale.
