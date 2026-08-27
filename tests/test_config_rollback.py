#!/usr/bin/env python3
"""Compile and run the host config journal and rollback policy."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ConfigRollbackTests(unittest.TestCase):
    def test_host_config_and_rollback_invariants(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "config_rollback_test"
            subprocess.check_call(
                [
                    "cc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DCIRVANE_CFG_TEST",
                    "-I",
                    str(ROOT / "kernel"),
                    str(ROOT / "kernel" / "config.c"),
                    str(ROOT / "kernel" / "rollback.c"),
                    str(ROOT / "kernel" / "config_rollback_test.c"),
                    "-o",
                    str(binary),
                ]
            )
            completed = subprocess.run(
                [str(binary)], check=True, capture_output=True, text=True
            )
            self.assertIn("config rollback checks passed", completed.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
