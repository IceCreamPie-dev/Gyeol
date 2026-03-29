#!/usr/bin/env python3
"""Update runtime performance baseline from the core perf suite."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path


def default_perf_cli(repo_root: Path) -> Path:
    base = repo_root / "build" / "src" / "tests" / "GyeolRuntimePerfCLI"
    if os.name == "nt":
        return base.with_suffix(".exe")
    return base


def run(cmd: list[str], cwd: Path) -> None:
    proc = subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True)
    if proc.returncode != 0:
        if proc.stdout:
            print(proc.stdout, file=sys.stderr)
        if proc.stderr:
            print(proc.stderr, file=sys.stderr)
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}")


def validate_baseline(path: Path) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("format") != "gyeol-runtime-perf":
        raise RuntimeError("Baseline format must be 'gyeol-runtime-perf'.")
    if data.get("version") != 1:
        raise RuntimeError("Baseline version must be 1.")
    scenarios = data.get("scenarios")
    if not isinstance(scenarios, list) or not scenarios:
        raise RuntimeError("Baseline scenarios must be a non-empty array.")
    required_names = {"line_loop", "choice_filter", "typed_command", "locale_overlay"}
    actual_names = {s.get("name") for s in scenarios if isinstance(s, dict)}
    if actual_names != required_names:
        raise RuntimeError(
            f"Baseline scenario names mismatch. expected={sorted(required_names)} actual={sorted(actual_names)}"
        )
    for scenario in scenarios:
        if not isinstance(scenario, dict):
            raise RuntimeError("Scenario entry must be an object.")
        if int(scenario.get("median_ns", 0)) <= 0:
            raise RuntimeError(f"Scenario median_ns must be > 0: {scenario.get('name')}")


def normalize_suite_path(path: Path, suite_path: str) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    data["suite_path"] = suite_path
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run GyeolRuntimePerfCLI core suite and overwrite runtime perf baseline."
    )
    parser.add_argument(
        "--perf-cli",
        help="Path to GyeolRuntimePerfCLI binary (default: build/src/tests/GyeolRuntimePerfCLI[.exe]).",
    )
    parser.add_argument(
        "--suite",
        default="src/tests/perf/runtime_perf_suite_core.json",
        help="Path to runtime perf suite json.",
    )
    parser.add_argument(
        "--output",
        default="src/tests/perf/runtime_perf_baseline_core.json",
        help="Path to output baseline json.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    perf_cli = Path(args.perf_cli) if args.perf_cli else default_perf_cli(repo_root)
    suite_arg = Path(args.suite)
    output_arg = Path(args.output)

    if not perf_cli.is_absolute():
        perf_cli = (repo_root / perf_cli).resolve()
    suite_fs = suite_arg if suite_arg.is_absolute() else (repo_root / suite_arg)
    output_fs = output_arg if output_arg.is_absolute() else (repo_root / output_arg)
    suite_cli = str(suite_arg) if not suite_arg.is_absolute() else str(suite_fs.resolve())
    output_cli = str(output_arg) if not output_arg.is_absolute() else str(output_fs.resolve())

    if not perf_cli.exists():
        print(f"error: perf cli not found: {perf_cli}", file=sys.stderr)
        return 2
    if not suite_fs.exists():
        print(f"error: suite file not found: {suite_fs}", file=sys.stderr)
        return 2

    output_fs.parent.mkdir(parents=True, exist_ok=True)
    run([str(perf_cli), "run", "--suite", suite_cli, "--output", output_cli], cwd=repo_root)
    validate_baseline(output_fs)
    normalize_suite_path(output_fs, args.suite.replace("\\", "/"))

    print(f"Updated runtime perf baseline: {output_fs}")
    print("Next: attach compare report and reason in PR when baseline changes.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
