# Release & PR Policy

This document defines the project policy that keeps CI quality and release behavior stable.

## Branch Protection (Manual Setup)

Apply this rule to `main` in GitHub repository settings:

1. Open `Settings > Branches > Add branch protection rule`.
2. Branch name pattern: `main`.
3. Enable `Require a pull request before merging`.
4. Enable `Require status checks to pass before merging`.
5. Mark these checks as required:
   - `Core (Linux (GCC))`
   - `Core (Windows (MSVC))`
   - `WASM (Emscripten)`
   - `Godot Extension (Windows)`
6. Recommended: enable `Require branches to be up to date before merging`.

## PR Gate Policy

For pull requests to `main`:

- The CI job `Policy Gate (PR)` runs automatically.
- The following files are blocked in normal PRs:
  - `demo/godot/bin/libgyeol*.dll`
  - `demo/godot/bin/libgyeol*.lib`
  - `demo/godot/bin/libgyeol*.exp`
- If such files are changed, CI fails with the exact blocked file list.

These binaries are **release-only** artifacts and must not be committed in regular PR/merge flow.

## Release Tag Policy (SemVer)

Release tags must match this format:

- `vMAJOR.MINOR.PATCH`
- Regex: `^v[0-9]+\.[0-9]+\.[0-9]+$`

Examples:

- Valid: `v1.2.3`
- Invalid: `v1.2`, `v1.2.3-rc1`, `v1.2.3-beta`, `version1.2.3`

The release workflow validates `GITHUB_REF_NAME` at the start and fails immediately if the tag format is invalid.
