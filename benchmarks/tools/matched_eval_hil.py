#!/usr/bin/env python3
"""Capture Cirvane eval recovery samples and Nucleus class-1 supervisor restarts."""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import statistics
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EVAL_OUT = ROOT / "build" / "kernel-spike-eval"
HIL_NUCLEUS = ROOT / "build" / "hil-matched"
EVIDENCE = ROOT / "benchmarks" / "results" / "matched-eval.json"
SPIKE_BOOT = ROOT / "build" / "spike-boot" / "bootloader" / "bootloader.bin"
WARMUP = 3
N = 30
SAMPLE_RE = re.compile(
    r"cirvane-eval class=(\d+) i=(\d+) outcome=(-?\d+) stale=(\d+) ticks=(\d+)"
)


def run(cmd: list[str]) -> None:
    env = dict(os.environ)
    env.setdefault("IDF_PATH", str(Path.home() / "esp" / "esp-idf"))
    completed = subprocess.run(cmd, text=True, capture_output=True, env=env)
    if completed.returncode != 0:
        raise SystemExit(
            f"{' '.join(cmd)} failed:\n{completed.stdout}\n{completed.stderr}"
        )


def openocd_home() -> Path:
    return Path(
        os.environ.get(
            "OPENOCD_HOME",
            str(
                Path.home()
                / ".espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32"
            ),
        )
    )


def kill_openocd() -> None:
    subprocess.run(["pkill", "-f", "openocd.*esp32c5"], check=False, capture_output=True)
    time.sleep(0.3)


def flash_bin(image: Path) -> None:
    """Program the app at ota_0 plus the matching unsigned second-stage loader.

    Nucleus HIL uses the quiet bootloader (USB console off) so a wedged CDC
    cannot trap ROM. The spike uses spike-boot so USB Serial/JTAG can return
    after a cable cycle. The CI bootloader verifies signatures and leaves
    unsigned images in ROM. `program_esp reset` is a JTAG CPU reset and does
    not map app IROM; a chip RST is required after programming.
    """
    hil_bl = HIL_NUCLEUS / "bootloader" / "bootloader.bin"
    if image.resolve().is_relative_to(HIL_NUCLEUS.resolve()) and hil_bl.is_file():
        bootloader: Path | None = hil_bl
    elif SPIKE_BOOT.is_file():
        bootloader = SPIKE_BOOT
    else:
        bootloader = None
    steps: list[tuple[Path, str, bool]] = []
    if bootloader is not None:
        steps.append((bootloader, "0x2000", False))
    steps.append((image, "0x20000", True))
    for part, offset, reset in steps:
        home = openocd_home()
        last: BaseException | None = None
        end = "verify reset exit" if reset else "verify exit"
        program = f"program_esp {part} {offset} {end}"
        for _attempt in range(3):
            kill_openocd()
            try:
                run(
                    [
                        str(home / "bin" / "openocd"),
                        "-s",
                        str(home / "share" / "openocd" / "scripts"),
                        "-f",
                        "board/esp32c5-builtin.cfg",
                        "-c",
                        "adapter speed 2000",
                        "-c",
                        program,
                    ]
                )
                last = None
                break
            except SystemExit as error:
                last = error
                time.sleep(3.0)
        if last is not None:
            raise last


def openocd_reset() -> None:
    home = openocd_home()
    run(
        [
            str(home / "bin" / "openocd"),
            "-s",
            str(home / "share" / "openocd" / "scripts"),
            "-f",
            "board/esp32c5-builtin.cfg",
            "-c",
            "adapter speed 2000",
            "-c",
            "init; reset run; shutdown",
        ]
    )


def symbol_address(elf: Path, name: str) -> int:
    nm = (
        Path.home()
        / ".espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/"
        / "riscv32-esp-elf-nm"
    )
    completed = subprocess.run(
        [str(nm), str(elf)], capture_output=True, text=True, check=True
    )
    for line in completed.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[-1] == name:
            return int(parts[0], 16)
    raise SystemExit(f"symbol {name} missing in {elf}")


def dump_words(addr: int, count: int) -> list[int]:
    import struct

    home = openocd_home()
    kill_openocd()
    blob = HIL_NUCLEUS / "matched-rtc.bin"
    blob.parent.mkdir(parents=True, exist_ok=True)
    nbytes = count * 4
    completed = subprocess.run(
        [
            str(home / "bin" / "openocd"),
            "-s",
            str(home / "share" / "openocd" / "scripts"),
            "-f",
            "board/esp32c5-builtin.cfg",
            "-c",
            "adapter speed 2000",
            "-c",
            "init; halt; riscv set_mem_access sysbus; "
            f"dump_image {blob} 0x{addr:08x} {nbytes}; resume; shutdown",
        ],
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0 or not blob.is_file() or blob.stat().st_size < nbytes:
        text = completed.stdout + "\n" + completed.stderr
        raise TimeoutError(f"jtag dump failed rc={completed.returncode} text={text[-800:]}")
    data = blob.read_bytes()[:nbytes]
    return list(struct.unpack("<" + "I" * count, data))


def dump_matched_class1(elf: Path) -> list[float]:
    addr = symbol_address(elf, "s_cirvane_matched")
    words = dump_words(addr, 3 + N + WARMUP)
    magic, count, done = words[0], words[1], words[2]
    if magic != 0xC1455E01:
        raise TimeoutError(f"matched magic {magic:#x} count={count} done={done}")
    if done != 1 or count < WARMUP + N:
        raise TimeoutError(f"matched incomplete count={count} done={done}")
    return [float(v) for v in words[3 + WARMUP : 3 + WARMUP + N]]


def capture_until(token: str, timeout_s: float) -> str:
    import glob
    import serial

    deadline = time.monotonic() + timeout_s
    received = bytearray()
    last_error: Exception | None = None
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
                if token in text:
                    time.sleep(0.2)
                    received.extend(port.read(4096))
                    return received.decode("utf-8", errors="replace")
        finally:
            port.close()
    raise TimeoutError(
        f"{token!r} not seen; last_error={last_error!r} tail={bytes(received[-240:])!r}"
    )


def stats(values: list[float]) -> dict[str, float]:
    if not values:
        return {"n": 0}
    pop = statistics.pstdev(values) if len(values) > 1 else 0.0
    ordered = sorted(values)
    p95_i = min(len(ordered) - 1, math.ceil(0.95 * len(ordered)) - 1)
    return {
        "n": len(values),
        "median": statistics.median(values),
        "p95": ordered[p95_i],
        "sd": pop,
        "min": ordered[0],
        "max": ordered[-1],
    }


def parse_cirvane(log: str) -> dict[str, list[dict[str, int]]]:
    classes: dict[str, list[dict[str, int]]] = {str(i): [] for i in range(5)}
    for match in SAMPLE_RE.finditer(log):
        class_id, i, outcome, stale, ticks = (int(v) for v in match.groups())
        if int(i) < WARMUP:
            continue
        classes[str(class_id)].append(
            {"outcome": outcome, "stale": stale, "ticks": ticks}
        )
    return classes


def capture_cirvane() -> tuple[str, dict[str, list[dict[str, int]]]]:
    run(
        [
            "bash",
            str(ROOT / "scripts" / "build-kernel-spike.sh"),
            str(EVAL_OUT),
            "eval",
        ]
    )
    image = EVAL_OUT / "cirvane-spike.bin"
    flash_bin(image)
    log = capture_until("cirvane-eval done", 60.0)
    return log, parse_cirvane(log)


def idf_hil_build() -> Path:
    env = dict(os.environ)
    idf = Path(env.get("IDF_PATH", str(Path.home() / "esp" / "esp-idf")))
    HIL_NUCLEUS.mkdir(parents=True, exist_ok=True)
    key = ROOT / "build" / "ci" / "signing-key.pem"
    if not key.is_file():
        key.parent.mkdir(parents=True, exist_ok=True)
        run(
            [
                "bash",
                "-lc",
                f"source {idf / 'export.sh'} >/dev/null && "
                f"python -m espsecure generate-signing-key --version 2 "
                f"--scheme ecdsa256 {key}",
            ]
        )
    quoted = (
        f"source {idf / 'export.sh'} >/dev/null && "
        f"idf.py -B {HIL_NUCLEUS} "
        f"-D SDKCONFIG={HIL_NUCLEUS / 'sdkconfig'} "
        f"-D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ci.defaults;sdkconfig.hil.defaults;sdkconfig.matched-eval.defaults' build"
    )
    run(["bash", "-lc", quoted])
    unsigned = HIL_NUCLEUS / "nucleus-unsigned.bin"
    signed = HIL_NUCLEUS / "nucleus.bin"
    return unsigned if unsigned.is_file() else signed


def open_usb_serial():
    import glob
    import serial

    ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/tty.usbmodem*")
    if not ports:
        return None
    port = serial.Serial()
    port.port = ports[0]
    port.baudrate = 115200
    port.timeout = 0.2
    port.write_timeout = 1
    port.dsrdtr = False
    port.rtscts = False
    port.dtr = False
    port.rts = False
    try:
        port.open()
        return port
    except (OSError, serial.SerialException):
        return None


def wait_usb_reenum(timeout_s: float) -> None:
    import glob

    deadline = time.monotonic() + timeout_s
    vanished = False
    while time.monotonic() < deadline:
        ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/tty.usbmodem*")
        if not vanished:
            if not ports:
                vanished = True
            time.sleep(0.05)
            continue
        if ports:
            time.sleep(0.3)
            return
        time.sleep(0.05)
    # Port may never vanish on this CDC; continue anyway.


def await_nucleus_prompt(port, deadline: float) -> str:
    received = bytearray()
    while time.monotonic() < deadline:
        try:
            chunk = port.read(2048)
        except Exception as error:  # noqa: BLE001 - USB CDC can drop mid-boot
            raise TimeoutError(
                f"serial read failed: {error!r} tail={bytes(received[-240:])!r}"
            ) from error
        if chunk:
            received.extend(chunk)
            if b"nucleus>" in received:
                return received.decode("utf-8", errors="replace")
        try:
            port.write(b"\n")
        except Exception:
            pass
        time.sleep(0.2)
    raise TimeoutError(f"prompt not seen; tail={bytes(received[-240:])!r}")


def nucleus_command(port, command: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    port.write(command.encode("ascii") + b"\n")
    received = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(2048)
        if chunk:
            received.extend(chunk)
            if b"nucleus>" in received:
                return received.decode("utf-8", errors="replace")
        time.sleep(0.05)
    raise TimeoutError(f"{command!r} produced no prompt; tail={bytes(received[-240:])!r}")


def nucleus_class1(timeout_s: float, skip_flash: bool = False) -> list[float]:
    samples: list[float] = []
    unsigned = HIL_NUCLEUS / "nucleus-unsigned.bin"
    if skip_flash and unsigned.is_file():
        image = unsigned
    else:
        image = idf_hil_build()
        if not skip_flash:
            flash_bin(image)
            wait_usb_reenum(8.0)
    while len(samples) < WARMUP + N:
        deadline = time.monotonic() + timeout_s
        port = None
        while time.monotonic() < deadline and port is None:
            port = open_usb_serial()
            if port is None:
                time.sleep(0.05)
        if port is None:
            raise TimeoutError("nucleus serial missing")
        try:
            await_nucleus_prompt(port, deadline)
            for _attempt in range(3):
                if len(samples) >= WARMUP + N:
                    break
                start = time.monotonic()
                injected = nucleus_command(port, "svcfail 0", 8.0)
                if "failure injected" not in injected:
                    raise TimeoutError(f"svcfail not acknowledged: {injected[-200:]}")
                left_running = False
                running = False
                while time.monotonic() < deadline:
                    table = nucleus_command(port, "svc", 8.0)
                    phase = None
                    for line in table.splitlines():
                        if "led-heartbeat" in line:
                            parts = line.split()
                            if len(parts) >= 3:
                                phase = parts[2]
                    if phase in {"backoff", "failed"}:
                        left_running = True
                    if left_running and phase == "running":
                        running = True
                        break
                    time.sleep(0.15)
                if not running:
                    raise TimeoutError(
                        "led-heartbeat did not leave then re-enter running"
                    )
                samples.append(time.monotonic() - start)
            if len(samples) < WARMUP + N:
                try:
                    port.write(b"restart\n")
                    time.sleep(0.2)
                except Exception:
                    pass
        finally:
            port.close()
        if len(samples) < WARMUP + N:
            wait_usb_reenum(8.0)
    return samples[WARMUP:]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-nucleus", action="store_true")
    parser.add_argument("--skip-cirvane", action="store_true")
    parser.add_argument("--skip-flash", action="store_true")
    parser.add_argument(
        "--jtag-class1",
        action="store_true",
        help="Collect Nucleus class-1 via HIL RTC record dumped over JTAG.",
    )
    args = parser.parse_args()
    record: dict = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "amendment": (
            "2026-08-27: single-board OpenOCD cost makes per-sample A,B,B,A "
            "impractical. Collection is image-blocked (Cirvane eval, then Nucleus "
            "HIL class 1). Warm-up 3 and n>=30 remain. Classes 2-5 are Cirvane-only "
            "and incomparable. No Wi-Fi samples (ADR 0004)."
        ),
        "warmup": WARMUP,
        "n": N,
    }
    if args.skip_cirvane:
        prior = json.loads(EVIDENCE.read_text(encoding="utf-8"))
        record["cirvane_log"] = prior["cirvane_log"]
        record["cirvane"] = prior["cirvane"]
        record["cirvane_stale_total"] = prior["cirvane_stale_total"]
        stale_total = int(prior["cirvane_stale_total"])
    else:
        cirvane_log, classes = capture_cirvane()
        record["cirvane_log"] = cirvane_log
        record["cirvane"] = {}
        stale_total = 0
        for class_id, rows in classes.items():
            ticks = [row["ticks"] for row in rows]
            stale_total += sum(row["stale"] for row in rows)
            record["cirvane"][class_id] = {
                "samples": rows,
                "ticks": stats([float(v) for v in ticks]),
                "stale": sum(row["stale"] for row in rows),
            }
        record["cirvane_stale_total"] = stale_total
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    if args.skip_nucleus:
        record["nucleus"] = {"status": "skipped"}
    elif args.jtag_class1:
        elf = HIL_NUCLEUS / "nucleus.elf"
        if not args.skip_flash:
            idf_hil_build()
            unsigned = HIL_NUCLEUS / "nucleus-unsigned.bin"
            signed = HIL_NUCLEUS / "nucleus.bin"
            image = unsigned if unsigned.is_file() else signed
            flash_bin(image)
            # 11 software restarts x 3 recoveries; do not halt until done.
            time.sleep(240.0)
        elif not elf.is_file():
            idf_hil_build()
        ms = dump_matched_class1(elf)
        record["nucleus"] = {
            "class_1_ms": ms,
            "stats_ms": stats(ms),
            "source": "jtag_noinit",
        }
    else:
        ms = [s * 1000.0 for s in nucleus_class1(180.0, skip_flash=args.skip_flash)]
        record["nucleus"] = {"class_1_ms": ms, "stats_ms": stats(ms), "source": "usb"}
    cirvane_c1 = record["cirvane"]["0"]["ticks"]
    nucleus = record.get("nucleus", {})
    differentiated = []
    if nucleus.get("stats_ms") and cirvane_c1.get("n", 0) >= N:
        differentiated.append(
            {
                "metric": "class_1_recovery_latency",
                "cirvane_ticks_median": cirvane_c1["median"],
                "nucleus_ms_median": nucleus["stats_ms"]["median"],
                "note": (
                    "Units differ (SYSTIMER ticks vs wall ms). Cirvane is a "
                    "synchronous admit; Nucleus uses 1s-shifted supervisor backoff. "
                    "Stale-work on Cirvane class 1 must be 0."
                ),
            }
        )
    record["differentiated"] = differentiated
    record["incomparable"] = [
        "Cirvane classes 2-5 vs Nucleus (no matching injectors)",
        "Scheduler/message/interrupt tails vs FreeRTOS (no matched Nucleus probes)",
        "Wi-Fi (ADR 0004)",
    ]
    record["result"] = (
        "pass"
        if stale_total == 0
        and all(v["ticks"].get("n", 0) >= N for v in record["cirvane"].values())
        else "fail"
    )
    if not args.skip_nucleus and nucleus.get("stats_ms", {}).get("n", 0) < N:
        record["result"] = "fail"
    EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(record["result"])
    try:
        run(
            [
                "bash",
                str(ROOT / "scripts" / "build-kernel-spike.sh"),
                str(ROOT / "build" / "kernel-spike"),
                "hil",
            ]
        )
        flash_bin(ROOT / "build" / "kernel-spike" / "cirvane-spike.bin")
    except SystemExit:
        record["hil_restored"] = False
        EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        raise
    record["hil_restored"] = True
    EVIDENCE.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    if record["result"] != "pass":
        raise SystemExit(1)


if __name__ == "__main__":
    main()
