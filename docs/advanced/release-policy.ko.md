# 릴리즈/PR 정책

이 문서는 CI 품질과 릴리즈 동작을 고정하기 위한 운영 정책을 정의합니다.

## 브랜치 보호 규칙 (수동 설정)

GitHub 저장소의 `main` 브랜치에 아래 규칙을 적용합니다.

1. `Settings > Branches > Add branch protection rule`로 이동
2. Branch name pattern: `main`
3. `Require a pull request before merging` 활성화
4. `Require status checks to pass before merging` 활성화
5. 아래 체크를 필수(required)로 지정:
   - `Core (Linux (GCC))`
   - `Core (Windows (MSVC))`
   - `WASM (Emscripten)`
   - `Godot Extension (Windows)`
6. 권장: `Require branches to be up to date before merging` 활성화

## PR 게이트 정책

`main` 대상 Pull Request에서는:

- CI의 `Policy Gate (PR)` 잡이 자동 실행됩니다.
- 일반 PR에서 다음 파일 변경은 금지됩니다.
  - `demo/godot/bin/libgyeol*.dll`
  - `demo/godot/bin/libgyeol*.lib`
  - `demo/godot/bin/libgyeol*.exp`
- 위 파일이 변경되면 CI가 실패하며, 금지된 파일 목록을 로그에 출력합니다.

위 바이너리는 **릴리즈 전용 산출물**이며 일반 PR/merge 흐름에는 포함하면 안 됩니다.

## 릴리즈 태그 정책 (SemVer)

릴리즈 태그는 아래 형식만 허용합니다.

- `vMAJOR.MINOR.PATCH`
- 정규식: `^v[0-9]+\.[0-9]+\.[0-9]+$`

예시:

- 유효: `v1.2.3`
- 무효: `v1.2`, `v1.2.3-rc1`, `v1.2.3-beta`, `version1.2.3`

릴리즈 워크플로는 시작 단계에서 `GITHUB_REF_NAME`을 검증하며, 형식이 맞지 않으면 즉시 실패합니다.
