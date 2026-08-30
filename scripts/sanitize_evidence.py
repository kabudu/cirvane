#!/usr/bin/env python3
"""Remove local network and device identifiers from committed JSON evidence."""

from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "benchmarks" / "results"
sys.path.insert(0, str(ROOT / "benchmarks" / "tools"))
from evidence_redaction import redact_evidence  # noqa: E402


for path in sorted(RESULTS.glob("*.json")):
    data = json.loads(path.read_text(encoding="utf-8"))
    path.write_text(json.dumps(redact_evidence(data), indent=2) + "\n", encoding="utf-8")

print("sanitized committed hardware evidence")
