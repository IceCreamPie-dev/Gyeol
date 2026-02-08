# LSP 서버

`GyeolLSP`는 `.gyeol` 파일을 위한 Language Server Protocol 서버입니다. 자동완성, 진단, 정의로 이동 등의 에디터 기능을 제공합니다.

## 사용법

LSP 서버는 stdin/stdout을 통한 JSON-RPC로 통신합니다. 일반적으로 에디터 확장에 의해 자동으로 실행됩니다.

```bash
GyeolLSP
```

## 기능

### 진단

입력하는 동안 실시간 에러 보고를 합니다. LSP는 파일이 변경될 때마다 Gyeol 파서를 실행하여 에러를 보고합니다:

- 구문 에러
- 알 수 없는 jump/call/choice 대상
- 중복 label 이름
- Import 에러
- 매개변수 에러

### 자동완성

문맥 인식 자동완성:

| 문맥 | 제안 항목 |
|---------|------------|
| 키워드 | `label`, `menu`, `jump`, `call`, `if`, `elif`, `else`, `random`, `return`, `import` |
| `jump` / `call` 뒤 | 프로젝트의 모든 label 이름 |
| `->` 뒤 | 모든 label 이름 |
| `$` 또는 `if` 뒤 | 모든 변수 이름 |
| 표현식 내 | `visit_count()`, `visited()`, `list_contains()`, `list_length()` |

### 정의로 이동

Ctrl+Click 또는 F12로 정의로 이동합니다:

| 요소 | 이동 대상 |
|---------|-------------|
| Label 이름 (jump/call/choice 내) | Label 선언 줄 |
| 변수 이름 | 첫 번째 할당 또는 전역 선언 |

### 호버

요소 위에 마우스를 올리면 문서가 표시됩니다:

| 요소 | 정보 |
|---------|------------|
| 키워드 | 구문 간략 설명 |
| Label 이름 | 매개변수 포함 시그니처 |
| 변수 | 스코프 정보 (전역/지역) |

### 문서 심볼

아웃라인 뷰에 표시됩니다:

| 심볼 타입 | 요소 |
|-------------|----------|
| Function | Label 정의 |
| Variable | 변수 선언 |

## VS Code 연동

VS Code 확장(`editors/vscode/`)이 자동으로 `GyeolLSP`에 연결됩니다.

### 설정

VS Code 설정(`settings.json`)에서:

```json
{
    "gyeol.lsp.path": "path/to/GyeolLSP"
}
```

### VS Code 기능

- **구문 하이라이팅** - `.gyeol` 파일이 자동으로 색상 표시됩니다
- **에러 밑줄** - 구문 에러에 빨간색 물결 밑줄이 표시됩니다
- **자동완성** - 자동으로 또는 Ctrl+Space로 트리거됩니다
- **정의로 이동** - label/변수에서 F12 또는 Ctrl+Click
- **호버** - 키워드, label, 변수 위에 마우스 올리기
- **아웃라인** - 사이드바에서 모든 label과 변수 보기
- **브레이크포인트** - 거터를 클릭하여 브레이크포인트 설정 (디버거용)

## 프로토콜 세부사항

LSP 서버가 구현하는 메서드:

| 메서드 | 방향 | 설명 |
|--------|-----------|-------------|
| `initialize` | 클라이언트 -> 서버 | 기능 협상 |
| `shutdown` | 클라이언트 -> 서버 | 종료 준비 |
| `exit` | 클라이언트 -> 서버 | 서버 종료 |
| `textDocument/didOpen` | 클라이언트 -> 서버 | 파일 열림 (진단 트리거) |
| `textDocument/didChange` | 클라이언트 -> 서버 | 파일 변경 (진단 트리거) |
| `textDocument/didClose` | 클라이언트 -> 서버 | 파일 닫힘 |
| `textDocument/publishDiagnostics` | 서버 -> 클라이언트 | 에러/경고 목록 전송 |
| `textDocument/completion` | 클라이언트 -> 서버 | 자동완성 요청 |
| `textDocument/definition` | 클라이언트 -> 서버 | 정의로 이동 |
| `textDocument/hover` | 클라이언트 -> 서버 | 호버 정보 |
| `textDocument/documentSymbol` | 클라이언트 -> 서버 | 문서 아웃라인 |

## 의존성

- **nlohmann/json** v3.11.3 (CMake FetchContent로 가져옴)

## 빌드 위치

CMake로 빌드 후:

```
build/src/gyeol_lsp/GyeolLSP
```
