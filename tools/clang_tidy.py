#!/usr/bin/env python3
"""Run clang-tidy on project-owned entries in a Meson compilation database."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}
REPOSITORY_SOURCE_ROOTS = {"bindings", "c-api", "core"}
DIAGNOSTIC_PATTERN = re.compile(r":\d+:\d+: (?:warning|error):|^warning: invalid configuration")
CHECK_PATTERN = re.compile(r"\[([^\]]+)\]\s*$")
LOCATION_PATTERN = re.compile(r"^(.*?):\d+:\d+:")
EXCLUDED_HEADER_PATTERN = (
    r"(^|[\\/])(subprojects|third_party)([\\/])"
    r"|(^|[\\/])(draconis_c|ryml_all)\.h(pp)?$"
)


@dataclass(frozen=True)
class TidyResult:
    source: Path
    returncode: int
    output: str


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build", help="Meson build directory (default: build)")
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=min(os.cpu_count() or 1, 4),
        help="number of parallel clang-tidy processes (default: up to 4)",
    )
    parser.add_argument("--checks", help="override the configured checker globs")
    scope_group = parser.add_mutually_exclusive_group()
    scope_group.add_argument("--repo-only", action="store_true", help="exclude configured plugin sources outside the repository")
    scope_group.add_argument("--plugins-only", action="store_true", help="check only configured plugin sources outside the repository")
    parser.add_argument("--fix", action="store_true", help="apply available fixes sequentially using the repository format style (overrides --jobs)")
    output_group = parser.add_mutually_exclusive_group()
    output_group.add_argument(
        "--summary",
        action="store_true",
        help="print only primary warning/error lines instead of full diagnostic notes",
    )
    output_group.add_argument(
        "--stats",
        action="store_true",
        help="print diagnostic counts by checker and file instead of individual diagnostics",
    )
    return parser.parse_args()


def has_plugin_manifest(source: Path) -> bool:
    return any((parent / "plugin.json").is_file() for parent in source.parents)


def is_project_source(source: Path, repo_root: Path) -> bool:
    if source.suffix.lower() not in SOURCE_SUFFIXES or not source.is_file():
        return False

    try:
        relative_path = source.relative_to(repo_root)
    except ValueError:
        return has_plugin_manifest(source)

    return (
        bool(relative_path.parts)
        and relative_path.parts[0] in REPOSITORY_SOURCE_ROOTS
        and "third_party" not in relative_path.parts
    )


def collect_sources(
    build_dir: Path,
    repo_root: Path,
    repo_only: bool = False,
    plugins_only: bool = False,
) -> list[Path]:
    database_path = build_dir / "compile_commands.json"
    if not database_path.is_file():
        raise FileNotFoundError(f"compilation database not found: {database_path}")

    with database_path.open(encoding="utf-8") as database_file:
        database = json.load(database_file)

    sources: set[Path] = set()
    for entry in database:
        directory = Path(entry["directory"])
        source = Path(entry["file"])
        if not source.is_absolute():
            source = directory / source
        source = source.resolve()
        is_repository_source = source.is_relative_to(repo_root)
        if (
            is_project_source(source, repo_root)
            and (not repo_only or is_repository_source)
            and (not plugins_only or not is_repository_source)
        ):
            sources.add(source)

    return sorted(sources)


def run_tidy(
    clang_tidy: str,
    build_dir: Path,
    config_file: Path,
    repo_root: Path,
    source: Path,
    checks: str | None,
    fix: bool,
) -> TidyResult:
    command = [
        clang_tidy,
        "-p",
        str(build_dir),
        "--config-file",
        str(config_file),
        "--header-filter=.*",
        f"--exclude-header-filter={EXCLUDED_HEADER_PATTERN}",
        "--quiet",
    ]
    if checks is not None:
        command.extend(("--checks", checks))
    if fix:
        command.extend(("--fix", "--format-style=file"))
    command.append(str(source))

    completed = subprocess.run(
        command,
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    return TidyResult(source, completed.returncode, completed.stdout)


def main() -> int:
    args = parse_arguments()
    if args.jobs < 1:
        print("error: --jobs must be at least 1", file=sys.stderr)
        return 2

    clang_tidy = shutil.which("clang-tidy")
    if clang_tidy is None:
        print("error: clang-tidy was not found on PATH", file=sys.stderr)
        return 1

    repo_root = Path(__file__).resolve().parent.parent
    build_dir = (repo_root / args.build_dir).resolve()
    config_file = repo_root / ".clang-tidy"

    try:
        sources = collect_sources(build_dir, repo_root, args.repo_only, args.plugins_only)
    except (FileNotFoundError, json.JSONDecodeError, KeyError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if not sources:
        print("error: no project-owned translation units found", file=sys.stderr)
        return 1

    # Translation units can share headers, so concurrent fixes can overwrite
    # each other's edits. Read-only analysis can still run in parallel.
    jobs = 1 if args.fix else args.jobs
    print(f"Running clang-tidy on {len(sources)} translation units with {jobs} workers...")
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        results = list(
            executor.map(
                lambda source: run_tidy(clang_tidy, build_dir, config_file, repo_root, source, args.checks, args.fix),
                sources,
            )
        )

    diagnostic_count = 0
    check_counts: Counter[str] = Counter()
    file_counts: Counter[str] = Counter()
    for result in results:
        output = result.output
        diagnostic_lines = [line for line in output.splitlines() if DIAGNOSTIC_PATTERN.search(line)]
        diagnostic_count += len(diagnostic_lines)
        for line in diagnostic_lines:
            check_match = CHECK_PATTERN.search(line)
            check_counts[check_match.group(1) if check_match else "configuration"] += 1
            location_match = LOCATION_PATTERN.search(line)
            if location_match:
                file_counts[location_match.group(1)] += 1

        if args.summary:
            output = "\n".join(diagnostic_lines)
        elif args.stats:
            output = ""

        if output:
            print(f"\n==> {result.source}")
            print(output.rstrip())

    if args.stats:
        print("\nDiagnostics by checker:")
        for check, count in check_counts.most_common():
            print(f"  {count:4}  {check}")
        print("\nDiagnostics by file:")
        for file_name, count in file_counts.most_common():
            print(f"  {count:4}  {file_name}")

    failures = [result for result in results if result.returncode != 0]
    print(f"\nChecked {len(results)} translation units; reported {diagnostic_count} diagnostics.")
    if failures:
        print(f"clang-tidy failed for {len(failures)} translation units.", file=sys.stderr)
        for failure in failures:
            print(f"  {failure.source} (exit {failure.returncode})", file=sys.stderr)
        return 1

    if diagnostic_count and not args.fix:
        print("clang-tidy reported diagnostics.", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
