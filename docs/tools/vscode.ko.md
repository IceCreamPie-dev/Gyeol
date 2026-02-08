# VS Code 확장

`gyeol-lang` 확장은 Visual Studio Code에서 `.gyeol` 파일에 대한 완전한 에디터 지원을 제공합니다.

## 설치

확장은 `editors/vscode/`에 위치해 있습니다. 설치 방법:

1. VS Code를 엽니다
2. 확장 사이드바로 이동합니다
3. "..." 메뉴 -> "VSIX에서 설치..."를 클릭합니다 (패키징된 경우)
4. 또는 개발을 위해 `editors/vscode/` 폴더를 워크스페이스로 엽니다

## 기능

### 구문 하이라이팅

`.gyeol` 파일을 위한 완전한 TextMate 문법:

- **키워드** - `label`, `menu`, `jump`, `call`, `if`, `elif`, `else`, `return`, `random`, `import`
- **문자열** - 이스케이프 시퀀스가 있는 큰따옴표 텍스트
- **주석** - `#`으로 시작하는 줄
- **변수** - `$ variable = value` 선언
- **명령** - `@` 명령 줄
- **캐릭터** - 캐릭터 정의와 대사
- **숫자** - 정수 및 실수 리터럴
- **연산자** - `==`, `!=`, `>`, `<`, `>=`, `<=`, `and`, `or`, `not`

### LSP 연동

`GyeolLSP`로 구동되는 실시간 언어 기능:

- **진단** - 에러와 경고가 물결 밑줄로 표시됩니다
- **자동완성** - 키워드, label, 변수, 내장 함수
- **정의로 이동** - label 이름과 변수에서 F12
- **호버** - 키워드, label, 매개변수에 대한 문서
- **문서 심볼** - label과 변수가 있는 아웃라인 뷰

### 디버거 연동

`GyeolDebugger`를 위한 디버그 어댑터 지원:

- **브레이크포인트** - 거터를 클릭하여 브레이크포인트 설정
- **실행 구성** - 에디터에서 `.gyb` 파일 실행

## 설정

### 세팅

| 세팅 | 타입 | 기본값 | 설명 |
|---------|------|---------|-------------|
| `gyeol.lsp.path` | `string` | `""` | `GyeolLSP` 실행 파일 경로 |
| `gyeol.debugger.path` | `string` | `""` | `GyeolDebugger` 실행 파일 경로 |

### 언어 설정

확장이 설정하는 항목:

- **줄 주석:** `#`
- **괄호:** `()`, `""`
- **자동 닫기 쌍:** `()`, `""`
- **들여쓰기 규칙:**
  - 증가: `label ... :`, `menu:`, `random:`, `if/elif ... ->`, `else ->` 뒤
  - 감소: `elif`, `else` 줄에서
- **폴딩:** 들여쓰기 기반

## 파일 연결

| 확장자 | 언어 ID |
|-----------|------------|
| `.gyeol` | `gyeol` |

## 개발

확장을 개발하려면:

```bash
cd editors/vscode
npm install
```

확장 소스는 `src/extension.ts`에 있으며, `.gyeol` 파일이 열리면 활성화됩니다. LSP 서버를 자식 프로세스로 실행합니다.

## 확장 구조

```
editors/vscode/
  package.json                    # Extension manifest
  language-configuration.json     # Language rules
  syntaxes/
    gyeol.tmLanguage.json         # TextMate grammar
  src/
    extension.ts                  # LSP client connection
```
