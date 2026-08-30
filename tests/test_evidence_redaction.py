#!/usr/bin/env python3
"""Regression tests for privacy-safe committed hardware evidence."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "benchmarks" / "tools"))

from evidence_redaction import redact_evidence, redact_text  # noqa: E402


def test_text_redaction() -> None:
    raw = (
        "station=aa:bb:cc:dd:ee:ff ip=192.168.4.19 "
        "port=/dev/cu.usbmodem2101\n"
        "1. Home Network (-42 dBm) secure\n"
        "connection succeeded"
    )
    cleaned = redact_text(raw)
    assert "aa:bb:cc:dd:ee:ff" not in cleaned
    assert "192.168.4.19" not in cleaned
    assert "/dev/cu.usbmodem2101" not in cleaned
    assert "Home Network" not in cleaned
    assert "connection succeeded" in cleaned
    assert "<redacted-mac>" in cleaned
    assert "<redacted-private-ip>" in cleaned
    assert "<serial-port>" in cleaned
    assert "<redacted-network-entry>" in cleaned


def test_structured_redaction() -> None:
    raw = {
        "ssid_sha256": "secret-derived-identifier",
        "nested": ["bssid=01:23:45:67:89:ab", {"ip": "10.0.0.7"}],
        "count": 2,
    }
    cleaned = redact_evidence(raw)
    assert "ssid_sha256" not in cleaned
    assert cleaned["network_identity"] == "redacted"
    assert cleaned["nested"] == [
        "bssid=<redacted-mac>",
        {"ip": "<redacted-private-ip>"},
    ]
    assert cleaned["count"] == 2


if __name__ == "__main__":
    test_text_redaction()
    test_structured_redaction()
    print("evidence redaction tests passed")
