#!/usr/bin/env python3
"""Flash the FreeRTOS-free kernel spike and capture serial evidence."""

from __future__ import annotations

import argparse
import json
import os
import shlex
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build" / "kernel-spike"
EVIDENCE = ROOT / "benchmarks" / "results" / "kernel-spike.json"

REQUIRED_MARKERS = (
    "cirvane-spike boot=ok",
    "cirvane-spike trap",
    "cirvane-spike interrupt",
    "cirvane-spike timer",
    "cirvane-spike memory static=",
    "cirvane-spike flash magic=",
    "cirvane-spike priv",
    "cirvane-spike recovery",
    "cirvane-spike freertos=absent",
    "cirvane-spike result=PASS",
)


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env.setdefault("IDF_PATH", str(Path.home() / "esp" / "esp-idf"))
    completed = subprocess.run(cmd, text=True, capture_output=True, env=env)
    if completed.returncode != 0:
        raise SystemExit(
            f"{' '.join(cmd)} failed:\n{completed.stdout}\n{completed.stderr}"
        )
    return completed


def idf_run(argv: list[str]) -> subprocess.CompletedProcess[str]:
    idf = Path(os.environ.get("IDF_PATH", str(Path.home() / "esp" / "esp-idf")))
    quoted = " ".join(shlex.quote(part) for part in argv)
    return run(
        [
            "bash",
            "-lc",
            f"source {shlex.quote(str(idf / 'export.sh'))} >/dev/null && {quoted}",
        ]
    )


def default_port() -> str:
    import glob

    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/tty.usbmodem*"))
    if not ports:
        raise SystemExit("no USB serial port found")
    return ports[0]


def build() -> tuple[Path, Path]:
    run(["bash", str(ROOT / "scripts" / "build-kernel-spike.sh"), str(OUT)])
    return OUT / "cirvane-spike.elf", OUT / "cirvane-spike.bin"


def maybe_sign(image: Path) -> Path:
    key = ROOT / "secure_boot_signing_key.pem"
    if not key.is_file():
        return image
    signed = OUT / "cirvane-spike.signed.bin"
    idf_run(
        [
            "espsecure.py",
            "sign_data",
            "--version",
            "2",
            "--keyfile",
            str(key),
            "--output",
            str(signed),
            str(image),
        ]
    )
    return signed


def flash(port: str, image: Path, offset: str) -> None:
    idf_run(
        [
            "esptool",
            "--chip",
            "esp32c5",
            "--port",
            port,
            "--before",
            "usb-reset",
            "--after",
            "hard-reset",
            "write-flash",
            "--force",
            offset,
            str(image),
        ]
    )


def capture(port_path: str, timeout_s: float) -> str:
    import serial

    deadline = time.monotonic() + timeout_s
    last_error: Exception | None = None
    received = bytearray()
    while time.monotonic() < deadline:
        try:
            port = serial.Serial(port_path, 115200, timeout=0.1)
        except (OSError, serial.SerialException) as error:
            last_error = error
            time.sleep(0.1)
            continue
        try:
            while time.monotonic() < deadline:
                chunk = port.read(512)
                if chunk:
                    received.extend(chunk)
                    if b"cirvane-spike result=" in received:
                        time.sleep(0.2)
                        received.extend(port.read(512))
                        return received.decode("utf-8", errors="replace")
        finally:
            port.close()
    raise TimeoutError(
        f"spike output not seen; last_error={last_error!r} tail={bytes(received[-240:])!r}"
    )


def classify(log: str) -> str:
    if all(marker in log for marker in REQUIRED_MARKERS):
        if "stale=0" in log and "advance=1" in log:
            return "pass"
        return "partial"
    return "fail"


def write_record(record: dict) -> None:
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="")
    parser.add_argument("--offset", default="0x20000")
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--flash", action="store_true")
    args = parser.parse_args()
    port = args.port or default_port()
    elf, image = build()
    image = maybe_sign(image)
    record = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "port": port,
        "offset": args.offset if args.flash else None,
        "elf": str(elf.relative_to(ROOT)),
        "image": str(image.relative_to(ROOT)) if image.is_relative_to(ROOT) else str(image),
    }
    try:
        if args.flash:
            flash(port, image, args.offset)
            time.sleep(1.0)
        log = capture(port, args.timeout)
        record["result"] = classify(log)
        record["log"] = log
    except (SystemExit, TimeoutError, OSError) as error:
        record["result"] = "blocked"
        record["reason"] = str(error)
        write_record(record)
        raise
    write_record(record)
    print(record["result"])
    print(record.get("log", ""))
    if record["result"] != "pass":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
