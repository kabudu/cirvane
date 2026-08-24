#!/usr/bin/env python3
"""Deterministic repository metadata and claim-boundary checks."""

from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "README.md", "AGENTS.md", "docs/PRODUCT_SPECIFICATION.md",
    "docs/ARCHITECTURE.md", "docs/ADOPTION_AND_INTEGRATION.md",
    "docs/BRAND_IDENTITY.md", "docs/IMPLEMENTATION_PLAN.md",
    "docs/E2E_TESTING.md", "docs/VALIDATION.md", "docs/THREAT_MODEL.md",
    "docs/OPERATIONS.md", "docs/RISK_REGISTER.md",
    "docs/REQUIREMENTS_TRACEABILITY.md", "docs/NOVELTY.md",
    "docs/PRIOR_ART_MATRIX.md", "docs/RELEASE.md",
    "docs/ENGINEERING_REVIEW.md", "docs/DECISIONS/0001-project-scope.md",
]


def fail(message: str) -> None:
    print(f"validation failed: {message}", file=sys.stderr)
    raise SystemExit(1)


for relative in REQUIRED:
    path = ROOT / relative
    if not path.is_file() or not path.read_text(encoding="utf-8").strip():
        fail(f"missing or empty {relative}")

for path in ROOT.rglob("*"):
    relative = path.relative_to(ROOT)
    if not path.is_file() or ".git" in path.parts or relative.parts[0] == "build":
        continue
    try:
        raw = path.read_bytes()
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = ""
    if "\u2014" in text:
        fail(f"Unicode U+2014 in {relative}")
    if path.name in {"secure_boot_signing_key.pem", "sdkconfig", "sdkconfig.old"}:
        fail(f"local or secret file in project tree: {relative}")
    if path.suffix == ".json":
        try:
            json.loads(text)
        except (UnicodeDecodeError, json.JSONDecodeError):
            fail(f"invalid JSON: {relative}")

print("repository metadata validation passed")
