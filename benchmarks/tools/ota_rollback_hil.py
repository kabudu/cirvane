#!/usr/bin/env python3
"""Exercise Cirvane signed OTA rollback and confirmation on real hardware."""

from __future__ import annotations

import argparse
import json
import time
from datetime import datetime, timezone
from pathlib import Path

from serial_benchmark import await_prompt, reconnect_and_await, sha256, sync


def run_command(port, command: str, timeout: float = 10) -> str:
    sync(port)
    port.reset_input_buffer()
    port.write(command.encode("ascii") + b"\n")
    port.flush()
    return await_prompt(port, time.monotonic() + timeout).decode("utf-8", "replace")


def restart(port, path: str):
    sync(port)
    port.reset_input_buffer()
    port.write(b"restart\n")
    port.flush()
    deadline = time.monotonic() + 15
    try:
        output = await_prompt(port, deadline)
        return port, output.decode("utf-8", "replace")
    except ConnectionError:
        port.close()
        port, output = reconnect_and_await(path, deadline)
        return port, output.decode("utf-8", "replace")


def require(output: str, *fragments: str) -> None:
    missing = [fragment for fragment in fragments if fragment not in output]
    if missing:
        raise RuntimeError(f"missing {missing!r} in device output: {output!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbmodem3101")
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    records: list[dict[str, str]] = []
    port, _ = reconnect_and_await(args.port, time.monotonic() + 10)
    try:
        def command(name: str, text: str, timeout: float = 10) -> str:
            output = run_command(port, text, timeout)
            records.append({"step": name, "command": text, "output": output})
            return output

        initial = command("initial", "ota-status")
        require(initial, "running=ota_0", "boot=ota_0")

        staged = command("stage-unconfirmed", "ota-stage-self", 60)
        require(staged, "ota stage-self: ESP_OK")
        selected = command("selected-unconfirmed", "ota-status")
        require(selected, "running=ota_0", "boot=ota_1")

        port, output = restart(port, args.port)
        records.append({"step": "boot-pending", "command": "restart", "output": output})
        pending = command("pending-unconfirmed", "ota-status")
        require(pending, "running=ota_1", "state=pending_verify")

        port, output = restart(port, args.port)
        records.append({"step": "trigger-rollback", "command": "restart", "output": output})
        rolled_back = command("rolled-back", "ota-status")
        require(rolled_back, "running=ota_0", "boot=ota_0")

        staged = command("stage-confirmed", "ota-stage-self", 60)
        require(staged, "ota stage-self: ESP_OK")
        port, output = restart(port, args.port)
        records.append({"step": "boot-to-confirm", "command": "restart", "output": output})
        pending = command("pending-to-confirm", "ota-status")
        require(pending, "running=ota_1", "state=pending_verify")

        confirmed = command("confirm", "ota-confirm")
        require(confirmed, "ota confirm: ESP_OK")
        valid = command("confirmed-valid", "ota-status")
        require(valid, "running=ota_1", "boot=ota_1", "state=valid")

        port, output = restart(port, args.port)
        records.append({"step": "reboot-confirmed", "command": "restart", "output": output})
        persisted = command("confirmed-persisted", "ota-status")
        require(persisted, "running=ota_1", "boot=ota_1", "state=valid")
    finally:
        port.close()

    result = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "firmware": str(args.firmware),
        "firmware_sha256": sha256(args.firmware),
        "result": "pass",
        "records": records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "result": "pass"}))


if __name__ == "__main__":
    main()
