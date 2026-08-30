#!/usr/bin/env python3
"""Verify signed-image acceptance and rejection with the ESP-IDF tooling."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from evidence_redaction import redact_evidence


def run(*args: str, expect_success: bool) -> dict[str, object]:
    result = subprocess.run(args, text=True, capture_output=True, check=False)
    passed = (result.returncode == 0) == expect_success
    if not passed:
        raise RuntimeError(
            f"unexpected exit {result.returncode} for {args!r}: "
            f"{result.stdout[-400:]}{result.stderr[-400:]}"
        )
    return {"command": list(args), "exit_code": result.returncode, "passed": True}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", type=Path, required=True)
    parser.add_argument("--key", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    records = [
        run("espsecure", "verify-signature", "--version", "2", "--keyfile",
            str(args.key), str(args.firmware), expect_success=True)
    ]
    with tempfile.TemporaryDirectory(prefix="cirvane-security-") as directory:
        temporary = Path(directory)
        corrupt = temporary / "corrupt.bin"
        image = bytearray(args.firmware.read_bytes())
        image[4096] ^= 1
        corrupt.write_bytes(image)
        records.append(
            run("espsecure", "verify-signature", "--version", "2", "--keyfile",
                str(args.key), str(corrupt), expect_success=False)
        )

        wrong_key = temporary / "wrong.pem"
        records.append(
            run("espsecure", "generate-signing-key", "--version", "2", "--scheme",
                "ecdsa256", str(wrong_key), expect_success=True)
        )
        records.append(
            run("espsecure", "verify-signature", "--version", "2", "--keyfile",
                str(wrong_key), str(args.firmware), expect_success=False)
        )

    result = {
        "schema": 1,
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "firmware": str(args.firmware),
        "firmware_sha256": sha256(args.firmware),
        "result": "pass",
        "records": records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(redact_evidence(result), indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "result": "pass"}))


if __name__ == "__main__":
    main()
