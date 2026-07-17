#!/usr/bin/env python3
"""Run clang-format on repository source files, not directory arguments."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".m",
    ".mm",
}

VENDORED_FILES = {
    Path("plugins/yaml_format/ryml_all.hpp"),
}


def is_format_target(path: Path, repo_root: Path) -> bool:
    relative_path = path.relative_to(repo_root)
    return (
        path.is_file()
        and path.suffix.lower() in SOURCE_SUFFIXES
        and "third_party" not in relative_path.parts
        and relative_path not in VENDORED_FILES
    )


def collect_files(inputs: list[str], repo_root: Path) -> list[Path]:
    files: set[Path] = set()

    for raw_path in inputs:
        path = (repo_root / raw_path).resolve()
        try:
            path.relative_to(repo_root)
        except ValueError as error:
            raise ValueError(f"format path is outside the repository: {raw_path}") from error

        if not path.exists():
            raise FileNotFoundError(f"format path does not exist: {raw_path}")

        candidates = (path,) if path.is_file() else path.rglob("*")
        files.update(candidate for candidate in candidates if is_format_target(candidate, repo_root))

    return sorted(files)


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: clang_format.py PATH [PATH ...]", file=sys.stderr)
        return 2

    clang_format = shutil.which("clang-format")
    if clang_format is None:
        print("error: clang-format was not found on PATH", file=sys.stderr)
        return 1

    repo_root = Path(__file__).resolve().parent.parent

    try:
        files = collect_files(sys.argv[1:], repo_root)
    except (FileNotFoundError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    if not files:
        print("No source files found.")
        return 0

    subprocess.run([clang_format, "-i", *(str(path) for path in files)], cwd=repo_root, check=True)
    print(f"Formatted {len(files)} files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
