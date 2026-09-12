#!/usr/bin/env python3
"""Promote Keep a Changelog Unreleased entries into a versioned release."""

from __future__ import annotations

import argparse
from datetime import date
from pathlib import Path
import re


def prepare(text: str, version: str, released: str) -> str:
    if not re.fullmatch(r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)", version):
        raise ValueError("version must be canonical MAJOR.MINOR.PATCH")
    try:
        date.fromisoformat(released)
    except ValueError as error:
        raise ValueError("date must be YYYY-MM-DD") from error
    heading = "## [Unreleased]\n"
    if text.count(heading) != 1:
        raise ValueError("changelog must contain exactly one Unreleased heading")
    if re.search(rf"^## \[{re.escape(version)}\]", text, re.MULTILINE):
        raise ValueError(f"version {version} already exists")
    body_start = text.index(heading) + len(heading)
    links_start = text.find("\n[Unreleased]:", body_start)
    if links_start < 0:
        raise ValueError("Unreleased comparison link is missing")
    unreleased_body = text[body_start:links_start]
    if not re.search(r"(?m)^- ", unreleased_body):
        raise ValueError("Unreleased contains no changes")
    previous_versions = re.findall(
        r"(?m)^## \[((?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*))\] - ", text
    )
    replacement = f"\n## [{version}] - {released}\n" + unreleased_body
    updated = text[:body_start] + replacement + text[links_start:]
    link_index = updated.find("\n[Unreleased]:")
    content, links = updated[:link_index], updated[link_index:]
    links = re.sub(
        r"\[Unreleased\]: .*",
        f"[Unreleased]: https://github.com/kabudu/cirvane/compare/v{version}...HEAD",
        links,
        count=1,
    )
    if previous_versions:
        links += (
            f"[{version}]: https://github.com/kabudu/cirvane/compare/"
            f"v{previous_versions[0]}...v{version}\n"
        )
    else:
        links += f"[{version}]: https://github.com/kabudu/cirvane/releases/tag/v{version}\n"
    return content + links


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("version")
    parser.add_argument("--date", required=True)
    parser.add_argument("--changelog", type=Path, default=Path("CHANGELOG.md"))
    args = parser.parse_args()
    original = args.changelog.read_text(encoding="utf-8")
    args.changelog.write_text(prepare(original, args.version, args.date), encoding="utf-8")
    print(f"prepared changelog for v{args.version}")


if __name__ == "__main__":
    main()
