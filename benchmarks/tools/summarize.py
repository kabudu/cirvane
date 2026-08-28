#!/usr/bin/env python3
"""Aggregate the committed Nucleus v1/v2 HIL measurement records."""

from __future__ import annotations

import json
import math
import statistics
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"


def samples(paths: list[str], metric: str) -> list[float]:
    values: list[float] = []
    for path in paths:
        values.extend(json.loads((RESULTS / path).read_text())[metric]["samples_ms"])
    return values


def describe(values: list[float]) -> dict[str, object]:
    ordered = sorted(values)
    return {
        "count": len(values),
        "median_ms": statistics.median(values),
        "p95_ms": ordered[math.ceil(0.95 * len(ordered)) - 1],
        "population_stdev_ms": statistics.pstdev(values),
        "min_ms": ordered[0],
        "max_ms": ordered[-1],
        "samples_ms": values,
    }


def main() -> None:
    boot_paths = {
        "v1": ["v1-block-1.json", "v1-block-2.json"],
        "v2": ["v2-block-1.json", "v2-block-2.json"],
    }
    latency_paths = {
        "v1": ["v1-latency-a.json", "v1-latency-b.json"],
        "v2": ["v2-latency-a.json", "v2-latency-b.json"],
    }
    boot = {label: describe(samples(paths, "boot")) for label, paths in boot_paths.items()}
    latency = {
        label: describe(samples(paths, "latency")) for label, paths in latency_paths.items()
    }
    optimized_paths = {
        "v1": ["v1-controlled-current.json", "v1-controlled-current-b2.json"],
        "v2": ["v2-fast-default-final.json", "v2-fast-default-final-b2.json"],
    }
    optimized = {
        label: {
            metric: describe(samples(paths, metric)) for metric in ("boot", "latency")
        }
        for label, paths in optimized_paths.items()
    }
    output = {
        "schema": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "method": {
            "boot_order": ["v1", "v2", "v2", "v1"],
            "latency_order": ["v2", "v1", "v1", "v2"],
            "boot_definition": "shell restart write to first operator prompt",
            "latency_definition": "info command write to next operator prompt",
            "serial_baud": 115200,
            "latency_reader_timeout_ms": 1,
            "excluded": "latency fields in *-block-[12].json used a 50 ms reader timeout and are retained only as an audit trail",
        },
        "boot": boot,
        "latency": latency,
        "optimized_final": optimized,
        "comparison": {
            "v2_boot_median_improvement_percent":
                (boot["v1"]["median_ms"] - boot["v2"]["median_ms"])
                / boot["v1"]["median_ms"] * 100,
            "v2_latency_median_improvement_percent":
                (latency["v1"]["median_ms"] - latency["v2"]["median_ms"])
                / latency["v1"]["median_ms"] * 100,
        },
        "energy": {
            "status": "not_measured",
            "reason": "no calibrated external current instrument was connected",
        },
        "ota_rollback": "ota-rollback.json",
        "optimized_ota_rollback": "ota-rollback-fast-final.json",
        "optimized_functional_hil": "functional-fast-final.json",
        "security_host": "security-host.json",
        "security_hil": "security-hil.json",
        "security_regression_functional": "functional-security-final.json",
        "security_regression_ota_rollback": "ota-rollback-security-final.json",
    }
    (RESULTS / "summary.json").write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()
