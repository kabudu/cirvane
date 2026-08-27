#!/usr/bin/env python3
"""Capture the production-profile quiet kernel shell on the ESP32-C5."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build" / "kernel-spike-prod"
EVIDENCE = ROOT / "benchmarks" / "results" / "kernel-shell.json"

REQUIRED = (
    "cirvane boot=ok",
    "cirvane>",
    "cirvane help",
    "cirvane info panic=0",
    "cirvane crash reason=0",
    "cirvane evidence svc=0",
    "cirvane refuse",
)


def run(cmd: list[str]) -> None:
    env = dict(os.environ)
    env.setdefault("IDF_PATH", str(Path.home() / "esp" / "esp-idf"))
    completed = subprocess.run(cmd, text=True, capture_output=True, env=env)
    if completed.returncode != 0:
        raise SystemExit(
            f"{' '.join(cmd)} failed:\n{completed.stdout}\n{completed.stderr}"
        )


def default_port() -> str:
    import glob

    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/tty.usbmodem*"))
    if not ports:
        raise SystemExit("no USB serial port found")
    return ports[0]


def build() -> tuple[Path, Path]:
    run(
        [
            "bash",
            str(ROOT / "scripts" / "build-kernel-spike.sh"),
            str(OUT),
            "production",
        ]
    )
    return OUT / "cirvane-spike.elf", OUT / "cirvane-spike.bin"


def classify(log: str) -> str:
    if any(token in log for token in ("heartbeat", "result=PASS")):
        return "fail"
    if all(marker in log for marker in REQUIRED):
        return "pass"
    return "fail"


def capture(timeout_s: float) -> str:
    import glob
    import serial

    deadline = time.monotonic() + timeout_s
    received = bytearray()
    last_error: Exception | None = None
    sent_help = False
    while time.monotonic() < deadline:
        ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/tty.usbmodem*")
        if not ports:
            time.sleep(0.05)
            continue
        try:
            port = serial.Serial()
            port.port = ports[0]
            port.baudrate = 115200
            port.timeout = 0.1
            port.dsrdtr = False
            port.rtscts = False
            port.dtr = False
            port.rts = False
            port.open()
        except (OSError, serial.SerialException) as error:
            last_error = error
            time.sleep(0.05)
            continue
        try:
            while time.monotonic() < deadline:
                chunk = port.read(512)
                if chunk:
                    received.extend(chunk)
                text = received.decode("utf-8", errors="replace")
                if "cirvane>" in text and not sent_help:
                    port.write(
                        b"help\ninfo\ncrash\nevidence 0\nnosuch\n"
                        + (b"x" * 40)
                        + b"\n"
                    )
                    sent_help = True
                if sent_help and "cirvane refuse" in text and "cirvane help" in text:
                    time.sleep(0.3)
                    received.extend(port.read(2048))
                    return received.decode("utf-8", errors="replace")
        finally:
            port.close()
    raise TimeoutError(
        f"quiet shell not seen; last_error={last_error!r} tail={bytes(received[-240:])!r}"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=45.0)
    args = parser.parse_args()
    elf, image = build()
    record = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "port": default_port(),
        "offset": "0x20000",
        "elf": str(elf.relative_to(ROOT)),
        "image": str(image.relative_to(ROOT)),
        "profile": "production",
    }
    try:
        log = capture(args.timeout)
        record["result"] = classify(log)
        record["log"] = log
    except (SystemExit, TimeoutError, OSError) as error:
        record["result"] = "blocked"
        record["reason"] = str(error)
        EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
        EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        raise
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(record["result"])
    print(record.get("log", ""))
    if record["result"] != "pass":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
