#!/usr/bin/env python3
"""Validate Cirvane canonical brand assets, tokens and manifest."""

from __future__ import annotations

import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
BRAND = ROOT / "assets" / "brand"
MANIFEST = BRAND / "BRAND_ASSET_MANIFEST.json"
REQUIRED_SOURCES = {
    "cirvane-horizontal-reversed.svg", "cirvane-horizontal.svg",
    "cirvane-icons.svg", "cirvane-stacked.svg", "cirvane-symbol-mono.svg",
    "cirvane-symbol-reversed.svg", "cirvane-symbol-small.svg",
    "cirvane-symbol.svg", "cirvane-symbol-dimensional.png",
    "cirvane-symbol-mono-dimensional.png", "cirvane-wordmark-reversed.svg",
    "cirvane-wordmark.svg",
}
FORBIDDEN_TAGS = {"script", "foreignObject", "iframe", "audio", "video"}
ALLOWED_LOCAL_IMAGES = {
    "cirvane-symbol-dimensional.png", "cirvane-symbol-mono-dimensional.png",
    "cirvane-wordmark.svg", "cirvane-wordmark-reversed.svg",
}
MATURITY = re.compile(r"\b(?:alpha|beta|evaluation|experimental|preview|release candidate|production-ready)\b", re.I)


def local_name(value: str) -> str:
    return value.rsplit("}", 1)[-1]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_luminance(value: str) -> float:
    channels = [int(value[index:index + 2], 16) / 255 for index in (1, 3, 5)]
    linear = [channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4 for channel in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast(foreground: str, background: str) -> float:
    light, dark = sorted((relative_luminance(foreground), relative_luminance(background)), reverse=True)
    return (light + 0.05) / (dark + 0.05)


def png_dimensions(path: Path) -> list[int]:
    raw = path.read_bytes()[:24]
    if raw[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("invalid PNG signature")
    return list(struct.unpack(">II", raw[16:24]))


def main() -> int:
    failures: list[str] = []
    actual_sources = {path.name for path in (BRAND / "source").iterdir() if path.is_file()}
    if actual_sources != REQUIRED_SOURCES:
        failures.append(f"canonical source inventory mismatch: {sorted(actual_sources)}")

    canonical_svgs = list((BRAND / "source").rglob("*.svg")) + list((BRAND / "templates").rglob("*.svg"))
    for path in canonical_svgs:
        raw = path.read_text(encoding="utf-8")
        try:
            root = ElementTree.fromstring(raw)
        except ElementTree.ParseError as exc:
            failures.append(f"{path.relative_to(ROOT)}: invalid XML: {exc}")
            continue
        if "viewBox" not in root.attrib:
            failures.append(f"{path.relative_to(ROOT)}: missing viewBox")
        if not any(local_name(child.tag) == "title" for child in root):
            failures.append(f"{path.relative_to(ROOT)}: missing title")
        if not any(local_name(child.tag) == "desc" for child in root):
            failures.append(f"{path.relative_to(ROOT)}: missing desc")
        for element in root.iter():
            tag = local_name(element.tag)
            if tag in FORBIDDEN_TAGS:
                failures.append(f"{path.relative_to(ROOT)}: forbidden element {tag}")
            for name, value in element.attrib.items():
                attribute = local_name(name)
                if attribute.lower().startswith("on"):
                    failures.append(f"{path.relative_to(ROOT)}: event attribute {attribute}")
                if attribute in {"href", "src"} and value not in ALLOWED_LOCAL_IMAGES:
                    failures.append(f"{path.relative_to(ROOT)}: undeclared or external resource {value}")
                if "url(" in value and "url(#" not in value:
                    failures.append(f"{path.relative_to(ROOT)}: external URL reference")
        if "/overlays/" not in f"/{path.relative_to(BRAND)}" and MATURITY.search(raw):
            failures.append(f"{path.relative_to(ROOT)}: maturity language in canonical asset")

    tokens = json.loads((BRAND / "tokens" / "cirvane.tokens.json").read_text(encoding="utf-8"))
    for foreground, background, threshold in tokens["contrastPairs"]:
        ratio = contrast(foreground, background)
        if ratio + 1e-9 < threshold:
            failures.append(f"contrast {foreground} on {background} is {ratio:.2f}, below {threshold}")

    if not MANIFEST.is_file():
        failures.append("missing BRAND_ASSET_MANIFEST.json")
    else:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        paths = [entry["path"] for entry in manifest["assets"]]
        inventory_roots = [BRAND / "source", BRAND / "templates", BRAND / "tokens", BRAND / "LICENSES", BRAND / "exports"]
        actual_paths = {
            str(path.relative_to(ROOT))
            for root in inventory_roots
            for path in root.rglob("*")
            if path.is_file()
        }
        if set(paths) != actual_paths:
            failures.append(
                "manifest inventory mismatch: "
                f"missing={sorted(actual_paths - set(paths))}, extra={sorted(set(paths) - actual_paths)}"
            )
        if len(paths) != len(set(paths)):
            failures.append("duplicate manifest paths")
        for entry in manifest["assets"]:
            path = ROOT / entry["path"]
            if not path.is_file():
                failures.append(f"manifest path missing: {entry['path']}")
                continue
            if sha256(path) != entry["sha256"]:
                failures.append(f"manifest digest mismatch: {entry['path']}")
            if path.suffix == ".png" and png_dimensions(path) != entry["dimensions"]:
                failures.append(f"manifest dimensions mismatch: {entry['path']}")
            if path.suffix == ".png" and "/exports/" in f"/{entry['path']}":
                source_path = entry.get("sourcePath")
                source_digest = entry.get("sourceSha256")
                if not source_path or not (ROOT / source_path).is_file():
                    failures.append(f"manifest export source missing: {entry['path']}")
                elif sha256(ROOT / source_path) != source_digest:
                    failures.append(f"manifest export source digest mismatch: {entry['path']}")

    if failures:
        print("brand validation failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("brand validation passed: sources, accessibility, safety and manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
