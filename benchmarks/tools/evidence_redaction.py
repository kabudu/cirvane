#!/usr/bin/env python3
"""Deterministic privacy redaction for committed hardware evidence."""

from __future__ import annotations

import re

MAC = re.compile(r"(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b")
PRIVATE_IP = re.compile(
    r"\b(?:10(?:\.\d{1,3}){3}|192\.168(?:\.\d{1,3}){2}|"
    r"172\.(?:1[6-9]|2\d|3[01])(?:\.\d{1,3}){2})\b"
)
NETWORK_ROW = re.compile(r"(?m)^\s*\d+\.\s+.*?(?:\r?\n|$)")
SERIAL_PORT = re.compile(r"/dev/(?:cu|tty)\.usbmodem[\w.-]+")


def redact_text(value: str) -> str:
    value = MAC.sub("<redacted-mac>", value)
    value = PRIVATE_IP.sub("<redacted-private-ip>", value)
    value = NETWORK_ROW.sub("<redacted-network-entry>\r\n", value)
    return SERIAL_PORT.sub("<serial-port>", value)


def redact_evidence(value):
    if isinstance(value, str):
        return redact_text(value)
    if isinstance(value, list):
        return [redact_evidence(item) for item in value]
    if isinstance(value, dict):
        cleaned = {}
        for key, item in value.items():
            if key == "ssid_sha256":
                cleaned["network_identity"] = "redacted"
            else:
                cleaned[key] = redact_evidence(item)
        return cleaned
    return value
