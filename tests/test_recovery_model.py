#!/usr/bin/env python3
"""Compile and run the host recovery-transaction model."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RecoveryModelTests(unittest.TestCase):
    def test_host_model_invariants(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "recovery_model_test"
            subprocess.check_call(
                [
                    "cc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "kernel" / "recovery"),
                    str(ROOT / "kernel" / "recovery" / "recovery.c"),
                    str(ROOT / "kernel" / "recovery" / "recovery_test.c"),
                    "-o",
                    str(binary),
                ]
            )
            completed = subprocess.run(
                [str(binary)], check=True, capture_output=True, text=True
            )
            self.assertIn("recovery model checks passed", completed.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
