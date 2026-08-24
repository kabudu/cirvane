#!/usr/bin/env python3
"""Exercise Nucleus v2 operator and supervised-service paths on hardware."""

from __future__ import annotations

import argparse
import json
import re
import time
from datetime import datetime, timezone
from pathlib import Path

from ota_rollback_hil import require, run_command
from serial_benchmark import reconnect_and_await


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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbmodem3101")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    records: list[dict[str, str]] = []
    port, _ = reconnect_and_await(args.port, time.monotonic() + 10)
    try:
        def command(name: str, text: str, timeout: float = 10) -> str:
            output = run_command(port, text, timeout)
            records.append({"step": name, "command": text, "output": output})
            return output

        require(command("self-test", "selftest"), "result=PASS")
        require(command("resources", "res"), "led-heartbeat", "wifi-scan")
        require(command("lazy-dual-band-scan", "scan"), "scan queued")
        scan_results = await_fragment(port, b"APs visible", 30)
        records.append({"step": "scan-results", "command": "", "output": scan_results})
        require(command("service-after-scan", "svc"), "wifi-scan", "running")
        require(command("inject-wifi-failure", "svcfail 1"), "failure injected")
        time.sleep(3)
        recovered = command("supervised-recovery", "svc")
        require(recovered, "wifi-scan", "running")
        if not re.search(r"wifi-scan\s+running\s+\d+\s+1\s+", recovered):
            raise RuntimeError(f"Wi-Fi restart was not recorded: {recovered!r}")
    finally:
        port.close()

    result = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "result": "pass",
        "records": records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "result": "pass"}))


if __name__ == "__main__":
    main()
