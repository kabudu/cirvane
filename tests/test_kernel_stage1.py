#!/usr/bin/env python3
"""Stage 1 kernel spike and claim-boundary contract checks."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
KERNEL = ROOT / "kernel"


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


class KernelStage1Contract(unittest.TestCase):
    def test_spike_source_has_no_freertos_or_allocator(self):
        for path in KERNEL.rglob("*"):
            if not path.is_file() or path.suffix not in {".c", ".h", ".S", ".ld"}:
                continue
            text = path.read_text(encoding="utf-8")
            self.assertNotRegex(text, r"freertos/", msg=str(path))
            self.assertNotRegex(text, r"\b(malloc|calloc|realloc)\s*\(", msg=str(path))
            self.assertNotIn("vTaskStartScheduler", text)

    def test_recovery_bounds_are_compile_time_constants(self):
        header = read("kernel/recovery/recovery.h")
        self.assertRegex(header, r"#define\s+CIRVANE_RTX_SERVICE_COUNT\s+8\b")
        self.assertRegex(header, r"#define\s+CIRVANE_RTX_SLOT_COUNT\s+32\b")
        self.assertRegex(header, r"#define\s+CIRVANE_RTX_EVIDENCE_SIZE\s+16\b")
        self.assertIn("CIRVANE_HEALTH_RECOVERING", header)

    def test_required_systems_are_compared(self):
        matrix = read("docs/PRIOR_ART_MATRIX.md")
        for name in (
            "FreeRTOS",
            "Zephyr",
            "Tock",
            "RIOT",
            "NuttX",
            "seL4",
            "Microkit",
            "Hubris",
            "Minix",
            "VirtuosoNext",
        ):
            self.assertIn(name, matrix)
        self.assertIn("Search log", matrix)
        self.assertIn("2026-08-25", matrix)

    def test_wifi_feasibility_is_explicit(self):
        inventory = read("docs/DEPENDENCY_INVENTORY.md")
        self.assertIn("FreeRTOS", inventory)
        self.assertIn("Wi-Fi", inventory)
        self.assertIn("owner decision", inventory.lower())
        self.assertIn("esp_adapter.c", inventory)

    def test_claims_remain_provisional(self):
        novelty = read("docs/NOVELTY.md").lower()
        self.assertIn("hypothesis", novelty)
        self.assertIn("does not claim definitive worldwide novelty", novelty)
        self.assertIn("no implemented or proven novel kernel", novelty)
        adr = read("docs/DECISIONS/0003-bounded-recovery-transaction.md")
        self.assertIn("Accepted for planning", adr)
        self.assertIn("provisional", adr.lower())

    def test_matched_evaluation_is_pre_registered(self):
        protocol = read("docs/MATCHED_EVALUATION.md").lower()
        for token in (
            "stale-work",
            "recovery latency",
            "stopping rule",
            "missing-data",
            "freertos",
            "resource ceiling",
        ):
            self.assertIn(token, protocol)

    def test_spike_docs_and_adr_exist(self):
        for relative in (
            "docs/KERNEL_SPIKE.md",
            "docs/DEPENDENCY_INVENTORY.md",
            "docs/MATCHED_EVALUATION.md",
            "docs/DECISIONS/0003-bounded-recovery-transaction.md",
        ):
            self.assertTrue((ROOT / relative).is_file())


if __name__ == "__main__":
    unittest.main(verbosity=2)
