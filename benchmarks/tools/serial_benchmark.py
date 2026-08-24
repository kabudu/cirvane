#!/usr/bin/env python3
"""Collect reproducible Nucleus shell latency and warm-restart samples."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import statistics
import time
from datetime import datetime, timezone
from pathlib import Path

import serial


PROMPT = b"nucleus>"


def open_port(path: str, deadline: float) -> serial.Serial:
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            return serial.Serial(path, 115200, timeout=0.001, write_timeout=1)
        except (OSError, serial.SerialException) as error:
            last_error = error
            time.sleep(0.05)
    raise RuntimeError(f"serial port did not return: {last_error}")


def await_prompt(port: serial.Serial, deadline: float) -> bytes:
    received = bytearray()
    while time.monotonic() < deadline:
        try:
            chunk = port.read(512)
        except (OSError, serial.SerialException):
            raise ConnectionError("serial port disconnected")
        if chunk:
            received.extend(chunk)
            if PROMPT in received:
                return bytes(received)
    raise TimeoutError(f"prompt not seen; tail={bytes(received[-160:])!r}")


def reconnect_and_await(path: str, deadline: float) -> tuple[serial.Serial, bytes]:
    while time.monotonic() < deadline:
        port = open_port(path, deadline)
        try:
            port.write(b"\n")
            port.flush()
            return port, await_prompt(port, deadline)
        except ConnectionError:
            port.close()
    raise TimeoutError("prompt not seen after reconnect")


def sync(port: serial.Serial) -> None:
    port.reset_input_buffer()
    port.write(b"\n")
    port.flush()
    await_prompt(port, time.monotonic() + 5)


def command_latency(port: serial.Serial, samples: int) -> list[float]:
    values: list[float] = []
    for index in range(samples + 3):
        sync(port)
        started = time.perf_counter_ns()
        port.write(b"info\n")
        port.flush()
        await_prompt(port, time.monotonic() + 5)
        elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000
        if index >= 3:
            values.append(elapsed_ms)
    return values


def boot_latency(port: serial.Serial, path: str, samples: int) -> tuple[serial.Serial, list[float]]:
    values: list[float] = []
    for _ in range(samples):
        sync(port)
        started = time.perf_counter_ns()
        port.write(b"restart\n")
        port.flush()
        deadline = time.monotonic() + 15
        try:
            await_prompt(port, deadline)
        except ConnectionError:
            port.close()
            port, _ = reconnect_and_await(path, deadline)
        values.append((time.perf_counter_ns() - started) / 1_000_000)
    return port, values


def describe(values: list[float]) -> dict[str, float | None]:
    if not values:
        return {
            "median_ms": None,
            "p95_ms": None,
            "population_stdev_ms": None,
            "min_ms": None,
            "max_ms": None,
        }
    ordered = sorted(values)
    return {
        "median_ms": statistics.median(values),
        "p95_ms": ordered[math.ceil(0.95 * len(ordered)) - 1],
        "population_stdev_ms": statistics.pstdev(values),
        "min_ms": ordered[0],
        "max_ms": ordered[-1],
    }


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--label", required=True)
    parser.add_argument("--block", required=True)
    parser.add_argument("--port", default="/dev/cu.usbmodem3101")
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--boot-samples", type=int, default=10)
    parser.add_argument("--latency-samples", type=int, default=15)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    port, _ = reconnect_and_await(args.port, time.monotonic() + 10)
    try:
        latency = command_latency(port, args.latency_samples)
        port, boot = boot_latency(port, args.port, args.boot_samples)
    finally:
        port.close()

    result = {
        "schema": 1,
        "label": args.label,
        "block": args.block,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "baud": 115200,
        "firmware": str(args.firmware),
        "firmware_sha256": sha256(args.firmware),
        "latency": {"command": "info", "samples_ms": latency, **describe(latency)},
        "boot": {"method": "shell restart to prompt", "samples_ms": boot, **describe(boot)},
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "latency": describe(latency), "boot": describe(boot)}))


if __name__ == "__main__":
    main()
