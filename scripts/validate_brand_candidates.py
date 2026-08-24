#!/usr/bin/env python3
"""Validate candidate SVG safety, construction and palette constraints."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_DIR = ROOT / "assets" / "brand" / "candidates"
EXPECTED = {
    "quiet-horizon-a-bounded-gate.svg",
    "quiet-horizon-b-controlled-shift.svg",
    "quiet-horizon-c-recovery-channel.svg",
    "expressive/quiet-horizon-d-vane-aperture.svg",
    "expressive/quiet-horizon-e-recovery-fold.svg",
    "expressive/quiet-horizon-f-faceted-aperture.svg",
    "expressive/f-refinements/f1-open-seam.svg",
    "expressive/f-refinements/f2-returning-facet.svg",
    "expressive/f-refinements/f3-vane-counterform.svg",
    "expressive/f-depth/f2a-folded-monolith.svg",
    "expressive/f-depth/f2b-inset-aperture.svg",
    "expressive/f-depth/f2c-counterfold.svg",
}
ALLOWED_TAGS = {"svg", "title", "desc", "path"}
ALLOWED_ATTRIBUTES = {
    "aria-labelledby",
    "d",
    "fill",
    "fill-rule",
    "id",
    "role",
    "viewBox",
}
ALLOWED_COLOURS = {"#0E7C78", "#3A2748", "#CDE7E2", "#1F2326"}
REQUIRED_COLOURS = {"#0E7C78", "#3A2748"}
MATURITY_TERMS = re.compile(
    r"\b(?:alpha|beta|evaluation|experimental|preview|release candidate|production-ready)\b",
    re.IGNORECASE,
)


def local_name(name: str) -> str:
    return name.rsplit("}", 1)[-1]


def validate(path: Path) -> list[str]:
    errors: list[str] = []
    raw = path.read_text(encoding="utf-8")
    try:
        root = ElementTree.fromstring(raw)
    except ElementTree.ParseError as exc:
        return [f"invalid XML: {exc}"]

    if root.attrib.get("viewBox") != "0 0 128 96":
        errors.append("viewBox must be 0 0 128 96")
    if MATURITY_TERMS.search(raw):
        errors.append("canonical candidate contains maturity language")

    paths = 0
    colours: set[str] = set()
    for element in root.iter():
        tag = local_name(element.tag)
        if tag not in ALLOWED_TAGS:
            errors.append(f"disallowed element: {tag}")
        for attribute, value in element.attrib.items():
            name = local_name(attribute)
            if name not in ALLOWED_ATTRIBUTES:
                errors.append(f"disallowed attribute: {name}")
            if name in {"href", "src"} or "url(" in value.lower():
                errors.append(f"external resource reference: {name}")
        if tag == "path":
            paths += 1
            fill = element.attrib.get("fill")
            if fill not in ALLOWED_COLOURS:
                errors.append(f"disallowed fill: {fill!r}")
            else:
                colours.add(fill)

    if not 1 <= paths <= 6:
        errors.append(f"expected 1 to 6 paths, found {paths}")
    if not REQUIRED_COLOURS.issubset(colours):
        errors.append(f"missing core candidate colours: {sorted(REQUIRED_COLOURS - colours)}")
    return errors


def main() -> int:
    actual = {
        str(path.relative_to(CANDIDATE_DIR))
        for path in CANDIDATE_DIR.rglob("*.svg")
    }
    failures: list[str] = []
    if actual != EXPECTED:
        failures.append(f"candidate inventory mismatch: {sorted(actual)}")
    for filename in sorted(EXPECTED & actual):
        for error in validate(CANDIDATE_DIR / filename):
            failures.append(f"{filename}: {error}")
    if failures:
        print("brand candidate validation failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("brand candidate validation passed: 12 safe deterministic SVGs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
