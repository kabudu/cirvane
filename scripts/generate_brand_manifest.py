#!/usr/bin/env python3
"""Generate the deterministic Cirvane brand asset manifest."""

from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[1]
BRAND = ROOT / "assets" / "brand"
EXPORT_SOURCES = {
    "cirvane-symbol-mono-dimensional.png": "assets/brand/source/cirvane-symbol-dimensional.png",
    "cirvane-symbol-16.png": "assets/brand/source/cirvane-symbol-small.svg",
    "cirvane-symbol-32.png": "assets/brand/source/cirvane-symbol-small.svg",
    "cirvane-symbol-64.png": "assets/brand/source/cirvane-symbol.svg",
    "cirvane-symbol-128.png": "assets/brand/source/cirvane-symbol.svg",
    "cirvane-symbol-512.png": "assets/brand/source/cirvane-symbol.svg",
    "cirvane-symbol-mono-512.png": "assets/brand/source/cirvane-symbol-mono.svg",
    "cirvane-symbol-reversed-512.png": "assets/brand/source/cirvane-symbol-reversed.svg",
    "cirvane-wordmark-432.png": "assets/brand/source/cirvane-wordmark.svg",
    "cirvane-horizontal-680.png": "assets/brand/source/cirvane-horizontal.svg",
    "cirvane-horizontal-reversed-680.png": "assets/brand/source/cirvane-horizontal-reversed.svg",
    "cirvane-stacked-432.png": "assets/brand/source/cirvane-stacked.svg",
    "cirvane-icons-1024.png": "assets/brand/source/cirvane-icons.svg",
    "cirvane-diagram-key-1280.png": "assets/brand/templates/diagram-key.svg",
    "cirvane-chart-palette-1280.png": "assets/brand/templates/chart-palette.svg",
    "cirvane-release-card-1200.png": "assets/brand/templates/overlays/release-social-card.svg",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def svg_dimensions(path: Path) -> list[int]:
    root = ElementTree.parse(path).getroot()
    values = root.attrib["viewBox"].split()
    return [round(float(values[2])), round(float(values[3]))]


def png_dimensions(path: Path) -> list[int]:
    data = path.read_bytes()[:24]
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    return list(struct.unpack(">II", data[16:24]))


def allowed_use(relative: Path) -> str:
    text = str(relative)
    if "/overlays/" in f"/{text}":
        return "versioned release overlay"
    if "/templates/" in f"/{text}":
        return "brand template"
    if "/tokens/" in f"/{text}":
        return "design system"
    if "/exports/" in f"/{text}":
        return "generated distribution asset"
    if "/source/" in f"/{text}":
        return "canonical identity source"
    return "licence and provenance"


def main() -> int:
    export_dir = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else BRAND / "exports"
    output = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else BRAND / "BRAND_ASSET_MANIFEST.json"
    roots = [BRAND / "source", BRAND / "templates", BRAND / "tokens", BRAND / "LICENSES", export_dir]
    paths = sorted(path for root in roots for path in root.rglob("*") if path.is_file())
    entries = []
    for path in paths:
        if path.suffix == ".svg":
            dimensions = svg_dimensions(path)
            media_type = "image/svg+xml"
        elif path.suffix == ".png":
            dimensions = png_dimensions(path)
            media_type = "image/png"
        elif path.suffix == ".json":
            dimensions = None
            media_type = "application/json"
        elif path.suffix == ".css":
            dimensions = None
            media_type = "text/css"
        else:
            dimensions = None
            media_type = "text/markdown"
        try:
            relative = path.relative_to(ROOT)
        except ValueError:
            relative = Path("assets/brand/exports") / path.name
        is_export = relative.parts[:3] == ("assets", "brand", "exports")
        is_dimensional_source = relative.name in {
            "cirvane-symbol-dimensional.png", "cirvane-symbol-mono-dimensional.png"
        }
        entry = {
            "path": str(relative),
            "sha256": digest(path),
            "mediaType": media_type,
            "dimensions": dimensions,
            "colourSpace": "sRGB" if media_type.startswith("image/") else None,
            "licence": "MIT",
            "creator": "OpenAI Codex under owner direction",
            "provenance": (
                "owner-approved AI-assisted raster master and deterministic derivative"
                if is_dimensional_source else
                "deterministic librsvg export" if is_export else
                "manually authored deterministic source"
            ),
            "allowedUse": allowed_use(relative),
            "exportCommand": (
                "scripts/export-brand-assets.sh" if is_export else
                "scripts/generate-brand-derivatives.sh" if relative.name == "cirvane-symbol-mono-dimensional.png" else
                None
            ),
            "sourcePath": EXPORT_SOURCES.get(path.name),
            "sourceSha256": digest(ROOT / EXPORT_SOURCES[path.name]) if path.name in EXPORT_SOURCES else None,
        }
        entries.append(entry)
    document = {"schemaVersion": 1, "brandVersion": "2.0.0", "assets": entries}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Generated manifest with {len(entries)} entries at {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
