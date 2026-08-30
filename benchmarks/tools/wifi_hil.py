#!/usr/bin/env python3
"""Exercise the secret-safe Cirvane Wi-Fi workflow on real hardware."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import stat
import time
from datetime import datetime, timezone
from pathlib import Path

from ota_rollback_hil import require, restart, run_command
from serial_benchmark import await_prompt, reconnect_and_await, sha256, sync


def read_credentials(path: Path) -> tuple[str, str]:
    mode = stat.S_IMODE(path.stat().st_mode)
    if mode != 0o600 or path.stat().st_uid != os.getuid():
        raise RuntimeError("credential file must be owned by the current user with mode 0600")
    values: dict[str, str] = {}
    allowed = {"CIRVANE_WIFI_SSID", "CIRVANE_WIFI_PASSWORD"}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if not raw_line or raw_line.lstrip().startswith("#"):
            continue
        key, separator, value = raw_line.partition("=")
        if separator == "" or key not in allowed or key in values:
            raise RuntimeError("credential file must contain each permitted field exactly once")
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "'\"":
            value = value[1:-1]
        values[key] = value
    if set(values) != allowed:
        raise RuntimeError("credential file is missing a required field")
    return values["CIRVANE_WIFI_SSID"], values["CIRVANE_WIFI_PASSWORD"]


def await_fragment(port, fragment: bytes, timeout: float) -> str:
    received = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        chunk = port.read(512)
        if chunk:
            received.extend(chunk)
            if fragment in received:
                return received.decode("utf-8", "replace")
    raise TimeoutError(f"{fragment!r} not seen: {bytes(received[-240:])!r}")


def redact(text: str, ssid: str, password: str) -> str:
    redacted = text.replace(ssid, "<redacted-ssid>")
    if password:
        redacted = redacted.replace(password, "<redacted-secret>")
    return redacted


def require_redacted(output: str, ssid: str, password: str, *fragments: str) -> None:
    missing = [fragment for fragment in fragments if fragment not in output]
    if missing:
        safe = redact(output, ssid, password)
        raise RuntimeError(f"missing {missing!r} in redacted device output: {safe!r}")


def interactive_connect(port, ssid: str, password: str) -> tuple[str, float]:
    sync(port)
    port.reset_input_buffer()
    started = time.monotonic()
    port.write(b"wifi connect\n")
    port.flush()
    scan = await_fragment(port, b"Select network number: ", 30)
    matches = re.findall(r"^\s*(\d+)\.\s+(.{1,32}?)\s+(2\.4GHz|5GHz)\s", scan,
                         flags=re.MULTILINE)
    selected = next((number for number, name, band in matches
                     if name.rstrip() == ssid and band == "2.4GHz"), None)
    if selected is None:
        selected = next((number for number, name, _ in matches
                         if name.rstrip() == ssid), None)
    if selected is None:
        raise RuntimeError("configured network was not present in bounded scan results")
    port.write(selected.encode("ascii") + b"\n")
    port.flush()
    selected_output = await_fragment(port, b"Password (input hidden; blank for open network): ", 5)
    port.write(password.encode("utf-8") + b"\n")
    port.flush()
    completed = await_prompt(port, time.monotonic() + 20).decode("utf-8", "replace")
    output = scan + selected_output + completed
    require_redacted(output, ssid, password, "wifi connected", " ip=")
    return output, (time.monotonic() - started) * 1000


def direct_connect(port, ssid: str, password: str) -> tuple[str, float]:
    sync(port)
    port.reset_input_buffer()
    started = time.monotonic()
    port.write(f'wifi connect "{ssid}"\n'.encode("utf-8"))
    port.flush()
    prefix = await_fragment(port, b"Password (input hidden; blank for open network): ", 5)
    port.write(password.encode("utf-8") + b"\n")
    port.flush()
    completed = await_prompt(port, time.monotonic() + 20).decode("utf-8", "replace")
    output = prefix + completed
    require_redacted(output, ssid, password, "wifi connected", " ip=")
    return output, (time.monotonic() - started) * 1000


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbmodem3101")
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--credentials-file", type=Path)
    args = parser.parse_args()

    if args.credentials_file:
        ssid, password = read_credentials(args.credentials_file)
    else:
        ssid = os.environ.get("CIRVANE_WIFI_SSID", "")
        password = os.environ.get("CIRVANE_WIFI_PASSWORD", "")
    if not ssid:
        raise RuntimeError("CIRVANE_WIFI_SSID must name the test network")
    if "\n" in ssid or "\r" in ssid or '"' in ssid:
        raise RuntimeError("test SSID contains unsupported shell characters")

    records: list[dict[str, object]] = []
    port, _ = reconnect_and_await(args.port, time.monotonic() + 10)
    try:
        initial = run_command(port, "wifi status")
        require(initial, "wifi disconnected")
        records.append({"step": "initial-status", "output": redact(initial, ssid, password)})

        rejected = run_command(port, 'wifi connect "ssid" "password"')
        require(rejected, "usage: wifi connect")
        records.append({"step": "reject-password-as-argument", "output": rejected})

        output, elapsed = interactive_connect(port, ssid, password)
        records.append({"step": "scan-select-connect", "elapsed_ms": elapsed,
                        "output": redact(output, ssid, password)})
        status = run_command(port, "wifi status")
        require(status, "wifi connected", " ip=")
        records.append({"step": "connected-status", "output": redact(status, ssid, password)})

        port, boot = restart(port, args.port)
        records.append({"step": "reboot-clears-session", "output": redact(boot, ssid, password)})
        after_reboot = run_command(port, "wifi status")
        require(after_reboot, "wifi disconnected")
        records.append({"step": "post-reboot-status", "output": after_reboot})

        output, elapsed = direct_connect(port, ssid, password)
        records.append({"step": "direct-connect", "elapsed_ms": elapsed,
                        "output": redact(output, ssid, password)})
        disconnected = run_command(port, "wifi disconnect")
        require(disconnected, "credentials cleared")
        records.append({"step": "disconnect-and-clear", "output": disconnected})
        final = run_command(port, "wifi status")
        require(final, "wifi disconnected")
        records.append({"step": "final-status", "output": final})
    finally:
        port.close()

    serialized = json.dumps(records)
    if password and password in serialized:
        raise RuntimeError("password leaked into Wi-Fi evidence")
    result = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "firmware": str(args.firmware),
        "firmware_sha256": sha256(args.firmware),
        "ssid_sha256": hashlib.sha256(ssid.encode("utf-8")).hexdigest(),
        "credential_storage": "ram-only",
        "result": "pass",
        "records": records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "result": "pass"}))


if __name__ == "__main__":
    main()
