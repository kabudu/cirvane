#!/usr/bin/env python3
"""Adversarial shell, OTA, and configuration checks on a development board."""

from __future__ import annotations

import argparse
import json
import time
from datetime import datetime, timezone
from pathlib import Path

from ota_rollback_hil import require, run_command
from serial_benchmark import await_prompt, reconnect_and_await, sync


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbmodem3101")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    records: list[dict[str, str]] = []
    port, _ = reconnect_and_await(args.port, time.monotonic() + 10)
    try:
        def command(name: str, text: str, *expected: str, timeout: float = 10) -> str:
            output = run_command(port, text, timeout)
            require(output, *expected)
            records.append({"step": name, "command": text, "output": output})
            return output

        config_before = command("config-before", "config", "heartbeat_ms=")
        command("negative-config", "config heartbeat -1", "invalid heartbeat_ms")
        command("signed-config", "config heartbeat +1000", "invalid heartbeat_ms")
        command("trailing-config", "config heartbeat 1000junk", "invalid heartbeat_ms")
        command("overflow-config", "config heartbeat 4294967296", "invalid heartbeat_ms")
        config_after = command("config-after", "config", "heartbeat_ms=")
        if config_before.split("\r\n")[1] != config_after.split("\r\n")[1]:
            raise RuntimeError("invalid configuration input changed durable state")

        command("power-before", "power", "budget=active")
        command("negative-sleep", "power sentinel -1", "invalid wake_ms")
        command("short-sleep", "power sentinel 99", "invalid wake_ms")
        command("oversized-sleep", "power sentinel 86400001", "invalid wake_ms")
        command("extra-power-args", "power sentinel 1000 extra", "usage: power")
        command("power-after", "power", "budget=active")

        command("negative-service", "svcctl -1 restart", "invalid service id")
        command("trailing-service", "svcfail 1junk", "invalid service id")
        command("unknown-command", "../../../../bin/sh", "Unrecognized command")

        sync(port)
        port.reset_input_buffer()
        port.write(b"A" * 1024 + b"\n")
        port.flush()
        oversized = await_prompt(port, time.monotonic() + 10).decode("utf-8", "replace")
        records.append({"step": "oversized-line", "command": "<1024 bytes>", "output": oversized})

        ota_before = command("ota-before", "ota-status", "ota running=", "boot=")
        command("corrupt-ota", "ota-reject-corrupt", "ota corrupt rejection: ESP_OK", timeout=60)
        ota_after = command("ota-after", "ota-status", "ota running=", "boot=")
        before_boot = ota_before.split("boot=", 1)[1].split()[0]
        after_boot = ota_after.split("boot=", 1)[1].split()[0]
        if before_boot != after_boot:
            raise RuntimeError("corrupted OTA image changed selected boot partition")

        command("corrupt-config-slot", "config-corrupt-test", "config corrupt fallback: ESP_OK")
        command("post-adversarial-selftest", "selftest", "result=PASS")
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
