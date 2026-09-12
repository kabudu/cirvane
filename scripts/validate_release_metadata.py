#!/usr/bin/env python3
"""Validate Keep a Changelog structure and optional tagged release notes."""

from __future__ import annotations

import argparse
from datetime import date
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
VERSION = r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)"


def fail(message: str) -> None:
    raise SystemExit(f"release metadata validation failed: {message}")


def validate_changelog(text: str) -> None:
    required = (
        "# Changelog\n",
        "https://keepachangelog.com/en/1.1.0/",
        "https://semver.org/spec/v2.0.0.html",
        "## [Unreleased]\n",
        "[Unreleased]: https://github.com/kabudu/cirvane/compare/",
    )
    for value in required:
        if value not in text:
            fail(f"CHANGELOG.md missing {value!r}")
    if text.count("## [Unreleased]\n") != 1:
        fail("CHANGELOG.md must contain one Unreleased section")
    versions = re.findall(rf"(?m)^## \[({VERSION})\] - (\d{{4}}-\d{{2}}-\d{{2}})$", text)
    if len({version for version, _ in versions}) != len(versions):
        fail("CHANGELOG.md contains a duplicate version")
    for version, released in versions:
        try:
            date.fromisoformat(released)
        except ValueError:
            fail(f"CHANGELOG.md has invalid date for {version}")
        if not re.search(rf"(?m)^\[{re.escape(version)}\]: https://github.com/kabudu/cirvane/", text):
            fail(f"CHANGELOG.md lacks comparison link for {version}")


def validate_notes(tag: str, text: str) -> None:
    if not re.fullmatch(rf"v{VERSION}", tag):
        fail("tag must be canonical vMAJOR.MINOR.PATCH")
    lines = text.splitlines()
    if not lines or not re.fullmatch(rf"# Cirvane {re.escape(tag)}: .+", lines[0]):
        fail("release notes title must be 'Cirvane vX.Y.Z: <theme>'")
    body = "\n".join(lines[1:]).strip()
    if not body:
        fail("release notes body is empty")
    if len(re.findall(r"(?m)^- ", body)) < 3 or len(re.findall(r"(?m)^- ", body)) > 5:
        fail("release notes must contain three to five material change bullets")
    for required in ("ESP-IDF", "FreeRTOS", "XIAO ESP32-C5", "not", "install"):
        if required.lower() not in body.lower():
            fail(f"release notes missing boundary or installation term {required!r}")
    for line_number, line in enumerate(lines, 1):
        if len(line) > 240:
            fail(f"release notes line {line_number} exceeds 240 characters")
    changelog = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    version = tag.removeprefix("v")
    if not re.search(rf"(?m)^## \[{re.escape(version)}\] - \d{{4}}-\d{{2}}-\d{{2}}$", changelog):
        fail(f"CHANGELOG.md has no release section for {tag}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag")
    args = parser.parse_args()
    validate_changelog((ROOT / "CHANGELOG.md").read_text(encoding="utf-8"))
    if args.tag:
        notes = ROOT / ".github" / "release-notes" / f"{args.tag}.md"
        if not notes.is_file():
            fail(f"missing {notes.relative_to(ROOT)}")
        validate_notes(args.tag, notes.read_text(encoding="utf-8"))
    print("release metadata validation passed")


if __name__ == "__main__":
    main()
