#!/usr/bin/env python3
"""Compile and run host crash, evidence and quiet-shell checks."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class KernelObsTests(unittest.TestCase):
    def test_host_obs_invariants(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "obs_test"
            subprocess.check_call(
                [
                    "cc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "kernel"),
                    "-I",
                    str(ROOT / "kernel" / "recovery"),
                    str(ROOT / "kernel" / "obs.c"),
                    str(ROOT / "kernel" / "kernel.c"),
                    str(ROOT / "kernel" / "recovery" / "recovery.c"),
                    str(ROOT / "kernel" / "obs_test.c"),
                    "-o",
                    str(binary),
                ]
            )
            completed = subprocess.run(
                [str(binary)], check=True, capture_output=True, text=True
            )
            self.assertIn("kernel obs checks passed", completed.stdout)
            self.assertNotIn("heartbeat", completed.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
