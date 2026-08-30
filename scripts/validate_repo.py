#!/usr/bin/env python3
"""Deterministic repository metadata and claim-boundary checks."""

from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "README.md", "AGENTS.md", "LICENSE", "docs/PRODUCT_SPECIFICATION.md",
    "docs/ARCHITECTURE.md", "docs/ADOPTION_AND_INTEGRATION.md",
    "docs/BRAND_IDENTITY.md", "docs/IMPLEMENTATION_PLAN.md",
    "docs/PRODUCTISATION_COMPLETION_PLAN.md",
    "docs/E2E_TESTING.md", "docs/VALIDATION.md", "docs/THREAT_MODEL.md",
    "docs/OPERATIONS.md", "docs/RISK_REGISTER.md",
    "docs/REQUIREMENTS_TRACEABILITY.md", "docs/NOVELTY.md",
    "docs/PRIOR_ART_MATRIX.md", "docs/RELEASE.md",
    "docs/ENGINEERING_REVIEW.md", "docs/DECISIONS/0001-project-scope.md",
    "docs/DECISIONS/0002-clean-sheet-kernel-release.md",
    "docs/DECISIONS/0003-bounded-recovery-transaction.md",
    "docs/DECISIONS/0004-stage2-workflow-without-wifi.md",
    "docs/DECISIONS/0005-first-cirvane-firmware-on-idf.md",
    "docs/KERNEL_SPIKE.md",
    "docs/KERNEL.md",
    "docs/DEPENDENCY_INVENTORY.md",
    "docs/MATCHED_EVALUATION.md",
    "docs/brand/ASSET_USAGE.md",
]
CURRENT_PRODUCT_IDENTITY_FILES = [
    "CMakeLists.txt",
    "main/CMakeLists.txt",
    "main/Kconfig.projbuild",
    "main/cirvane.c",
    "main/cirvane_os.c",
    "main/cirvane_os.h",
    "sdkconfig.defaults",
    "sdkconfig.hil.defaults",
    "sdkconfig.ci.defaults",
    "sdkconfig.matched-eval.defaults",
    "scripts/ci-local.sh",
    "benchmarks/tools/serial_benchmark.py",
    "benchmarks/tools/wifi_hil.py",
]
NUCLEUS_TOKEN = re.compile(r"nucleus", re.IGNORECASE)


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

for leftover in ("main/nucleus.c", "main/nucleus_os.c", "main/nucleus_os.h"):
    if (ROOT / leftover).is_file():
        fail(f"retired Nucleus source still present: {leftover}")
for relative in CURRENT_PRODUCT_IDENTITY_FILES:
    path = ROOT / relative
    if not path.is_file():
        fail(f"missing current-product identity file {relative}")
    if NUCLEUS_TOKEN.search(path.read_text(encoding="utf-8")):
        fail(f"current-product Nucleus identifier in {relative}")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
if "project(cirvane)" not in cmake:
    fail("root CMakeLists.txt must declare project(cirvane)")
ci_local = (ROOT / "scripts/ci-local.sh").read_text(encoding="utf-8")
if "build/ci/cirvane.bin" not in ci_local:
    fail("ci-local must inspect build/ci/cirvane.bin")
main_src = (ROOT / "main/cirvane.c").read_text(encoding="utf-8")
if 'repl_cfg.prompt = "cirvane> "' not in main_src:
    fail("USB shell prompt must be cirvane>")

print("repository metadata validation passed")
