#!/usr/bin/env python3
"""Offline, deterministic public-source metadata and policy checks."""

from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]

REQUIRED = [
    "LICENSE",
    "CHANGELOG.md",
    "CITATION.cff",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "GOVERNANCE.md",
    "SECURITY.md",
    "SUPPORT.md",
    "THIRD_PARTY_NOTICES.md",
    ".github/CODEOWNERS",
    ".github/PULL_REQUEST_TEMPLATE.md",
    ".github/ISSUE_TEMPLATE/bug_report.yml",
    ".github/ISSUE_TEMPLATE/feature_request.yml",
    ".github/ISSUE_TEMPLATE/config.yml",
    "docs/OPEN_SOURCE_READINESS.md",
]


def fail(message: str) -> None:
    raise SystemExit(f"public-readiness validation failed: {message}")


for relative in REQUIRED:
    path = ROOT / relative
    if not path.is_file() or not path.read_text(encoding="utf-8").strip():
        fail(f"missing or empty {relative}")

license_text = (ROOT / "LICENSE").read_text(encoding="utf-8")
if not license_text.startswith("                                 Apache License\n                           Version 2.0, January 2004\n"):
    fail("root licence must be Apache-2.0")

readme = (ROOT / "README.md").read_text(encoding="utf-8")
if "[Apache License 2.0](LICENSE)" not in readme:
    fail("README licence does not match the root Apache-2.0 licence")
for stale in ("private pre-release", "While the repository remains private"):
    if stale in readme:
        fail(f"visibility-dependent README wording remains: {stale}")

for root_name in ("main", "kernel", "tests"):
    for path in (ROOT / root_name).rglob("*"):
        if path.suffix not in {".c", ".h", ".S"}:
            continue
        text = path.read_text(encoding="utf-8")
        match = re.search(r"SPDX-License-Identifier:\s*([^\r\n*]+)", text)
        if match and match.group(1).strip() != "Apache-2.0":
            fail(f"project source has non-Apache-2.0 SPDX identifier: {path.relative_to(ROOT)}")

security = (ROOT / "SECURITY.md").read_text(encoding="utf-8")
if "private vulnerability reporting" not in security.lower():
    fail("SECURITY.md must name the private disclosure channel")

contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")
for required in ("./scripts/ci-local.sh", "Signed-off-by", "SECURITY.md"):
    if required not in contributing:
        fail(f"CONTRIBUTING.md missing {required}")

if list((ROOT / ".github" / "workflows").glob("*")) if (ROOT / ".github" / "workflows").exists() else []:
    fail("hosted workflow present without recorded owner approval")

mac = re.compile(r"(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b")
private_ip = re.compile(
    r"\b(?:10(?:\.\d{1,3}){3}|192\.168(?:\.\d{1,3}){2}|"
    r"172\.(?:1[6-9]|2\d|3[01])(?:\.\d{1,3}){2})\b"
)
network_row = re.compile(r"(?m)^\s*\d+\.\s+.*(?:2\.4GHz|5GHz)")
for path in (ROOT / "benchmarks" / "results").glob("*.json"):
    text = path.read_text(encoding="utf-8")
    json.loads(text)
    if mac.search(text) or private_ip.search(text) or network_row.search(text):
        fail(f"local network or device identifier in {path.relative_to(ROOT)}")
    if '"ssid_sha256"' in text or re.search(r"/dev/(?:cu|tty)\.usbmodem", text):
        fail(f"reversible network metadata or local serial path in {path.relative_to(ROOT)}")

print("public-source readiness metadata validation passed")
