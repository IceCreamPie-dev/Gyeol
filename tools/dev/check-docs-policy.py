#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path


KO_MD_REF_RE = re.compile(r"\.ko\.md\b")
MKDOCS_NAV_MD_RE = re.compile(r"^\s*-\s+[^:]+:\s+([^\s#]+\.md)\s*$")
LINK_RE = re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^\s{0,3}#{1,6}\s+(.*?)\s*$")
KOREAN_CHAR_RE = re.compile(r"[가-힣]")
ENGLISH_WORD_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_+\-]*$")
INLINE_CODE_RE = re.compile(r"`[^`]*`")
INLINE_LINK_RE = re.compile(r"\[([^\]]+)\]\([^)]+\)")

# (pattern, recommendation)
DISCOURAGED_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"\b세이브/로드\b"), "저장/로드"),
    (re.compile(r"\bJSONIR\b"), "JSON IR"),
    (re.compile(r"\blocale-catalog\b", re.IGNORECASE), "locale catalog"),
]

ENGLISH_RUN_THRESHOLD = 7


@dataclass
class Issue:
    path: Path
    line: int
    message: str


def strip_inline_markup(text: str) -> str:
    text = INLINE_CODE_RE.sub(" ", text)
    text = INLINE_LINK_RE.sub(r"\1", text)
    return text


def slugify_heading(text: str) -> str:
    text = strip_inline_markup(text)
    text = re.sub(r"<[^>]+>", "", text)
    text = text.strip().lower()
    text = re.sub(r"[^\w\s\-가-힣]", "", text, flags=re.UNICODE)
    text = re.sub(r"\s+", "-", text)
    text = re.sub(r"-{2,}", "-", text)
    return text.strip("-")


def iter_markdown_files(docs_dir: Path) -> list[Path]:
    return sorted(p for p in docs_dir.rglob("*.md") if p.is_file())


def gather_heading_anchors(md_file: Path) -> set[str]:
    anchors: set[str] = set()
    in_fence = False
    for raw_line in md_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.rstrip("\n")
        if line.strip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        m = HEADING_RE.match(line)
        if not m:
            continue
        heading = re.sub(r"\s+#+\s*$", "", m.group(1)).strip()
        slug = slugify_heading(heading)
        if slug:
            anchors.add(slug)
    return anchors


def check_ko_md_refs(path: Path, text: str, issues: list[Issue]) -> None:
    for idx, line in enumerate(text.splitlines(), start=1):
        scan_line = INLINE_CODE_RE.sub(" ", line)
        if KO_MD_REF_RE.search(scan_line):
            issues.append(Issue(path, idx, "'.ko.md' 참조는 금지됩니다. 기본 '.md' 경로를 사용하세요."))


def parse_nav_paths(mkdocs_path: Path) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    for idx, line in enumerate(mkdocs_path.read_text(encoding="utf-8").splitlines(), start=1):
        m = MKDOCS_NAV_MD_RE.match(line)
        if m:
            result.append((idx, m.group(1).strip()))
    return result


def is_external_link(target: str) -> bool:
    lowered = target.lower()
    return lowered.startswith(("http://", "https://", "mailto:", "tel:", "javascript:"))


def check_markdown_links(
    docs_dir: Path,
    md_files: list[Path],
    anchors_cache: dict[Path, set[str]],
    issues: list[Issue],
) -> None:
    for md_file in md_files:
        lines = md_file.read_text(encoding="utf-8").splitlines()
        in_fence = False
        for idx, raw_line in enumerate(lines, start=1):
            line = raw_line.rstrip("\n")
            if line.strip().startswith("```"):
                in_fence = not in_fence
                continue
            if in_fence:
                continue

            for match in LINK_RE.finditer(line):
                target = match.group(1).strip()
                if target.startswith("<") and target.endswith(">"):
                    target = target[1:-1].strip()
                if not target or is_external_link(target):
                    continue

                if target.startswith("#"):
                    anchor = target[1:]
                    if anchor and anchor not in anchors_cache.get(md_file, set()):
                        issues.append(Issue(md_file, idx, f"앵커 '{anchor}'를 찾을 수 없습니다."))
                    continue

                path_part, _, anchor = target.partition("#")
                if not path_part.endswith(".md"):
                    continue

                target_file = (md_file.parent / path_part).resolve()
                try:
                    target_file.relative_to(docs_dir.resolve())
                except ValueError:
                    issues.append(Issue(md_file, idx, f"문서 루트 밖 경로를 참조합니다: {path_part}"))
                    continue

                if not target_file.exists():
                    issues.append(Issue(md_file, idx, f"깨진 링크입니다: {path_part}"))
                    continue

                if anchor:
                    target_anchors = anchors_cache.get(target_file, set())
                    if anchor not in target_anchors:
                        issues.append(Issue(md_file, idx, f"대상 파일의 앵커 '{anchor}'를 찾을 수 없습니다: {path_part}"))


def check_excessive_english(md_file: Path, issues: list[Issue]) -> None:
    in_fence = False
    for idx, raw_line in enumerate(md_file.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.rstrip("\n")
        stripped = line.strip()
        if stripped.startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        if not stripped or stripped.startswith("#") or "|" in stripped:
            continue
        if KOREAN_CHAR_RE.search(stripped):
            continue

        normalized = strip_inline_markup(stripped)
        tokens = re.findall(r"[A-Za-z][A-Za-z0-9_+\-]*|[^A-Za-z\s]+", normalized)
        run = 0
        max_run = 0
        for token in tokens:
            if ENGLISH_WORD_RE.match(token):
                run += 1
                if run > max_run:
                    max_run = run
            else:
                run = 0
        if max_run >= ENGLISH_RUN_THRESHOLD:
            issues.append(Issue(md_file, idx, f"영문 연속 단어 {max_run}개 감지(임계치 {ENGLISH_RUN_THRESHOLD}). 한국어 설명문으로 바꿔주세요."))


def check_discouraged_patterns(md_file: Path, issues: list[Issue]) -> None:
    in_fence = False
    for idx, raw_line in enumerate(md_file.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.rstrip("\n")
        stripped = line.strip()
        if stripped.startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        scan_line = INLINE_CODE_RE.sub(" ", stripped)
        for pattern, recommendation in DISCOURAGED_PATTERNS:
            if pattern.search(scan_line):
                issues.append(Issue(md_file, idx, f"비권장 용어 감지: '{pattern.pattern}' -> 권장: '{recommendation}'"))


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    docs_dir = repo_root / "docs"
    mkdocs_path = repo_root / "mkdocs.yml"

    if not docs_dir.exists() or not mkdocs_path.exists():
        print("오류: docs/ 또는 mkdocs.yml을 찾을 수 없습니다.", file=sys.stderr)
        return 2

    issues: list[Issue] = []
    md_files = iter_markdown_files(docs_dir)

    # 1) .ko.md 참조 금지
    check_ko_md_refs(mkdocs_path, mkdocs_path.read_text(encoding="utf-8"), issues)
    for md_file in md_files:
        check_ko_md_refs(md_file, md_file.read_text(encoding="utf-8"), issues)

    # 2) mkdocs nav 경로 검증
    nav_paths = parse_nav_paths(mkdocs_path)
    if not nav_paths:
        issues.append(Issue(mkdocs_path, 1, "mkdocs.yml에서 nav의 .md 경로를 찾지 못했습니다."))
    for line_no, rel_path in nav_paths:
        if not rel_path.endswith(".md"):
            issues.append(Issue(mkdocs_path, line_no, f"nav 경로는 .md여야 합니다: {rel_path}"))
            continue
        target = (docs_dir / rel_path).resolve()
        if not target.exists():
            issues.append(Issue(mkdocs_path, line_no, f"nav 경로 파일이 없습니다: {rel_path}"))

    # 3) 링크/앵커 정합성
    anchors_cache = {md_file.resolve(): gather_heading_anchors(md_file) for md_file in md_files}
    check_markdown_links(docs_dir, md_files, anchors_cache, issues)

    # 4) 과도한 영문 문장 / 5) 비권장 용어
    for md_file in md_files:
        check_excessive_english(md_file, issues)
        check_discouraged_patterns(md_file, issues)

    if issues:
        print(f"[check-docs-policy] 실패: {len(issues)}건")
        for issue in sorted(issues, key=lambda i: (str(i.path), i.line, i.message)):
            rel = issue.path.resolve().relative_to(repo_root.resolve())
            print(f"- {rel}:{issue.line}: {issue.message}")
        return 1

    print("[check-docs-policy] 통과")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
