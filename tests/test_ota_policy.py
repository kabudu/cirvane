#!/usr/bin/env python3
"""Compile and execute portable authenticated-OTA policy tests."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OtaPolicyTests(unittest.TestCase):
    def test_ota_policy_boundaries(self):
        with tempfile.TemporaryDirectory(prefix="cirvane-ota-") as directory:
            executable = Path(directory) / "ota-policy-test"
            subprocess.run(
                [
                    "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "main"),
                    str(ROOT / "main" / "cirvane_ota_policy.c"),
                    str(ROOT / "tests" / "ota_policy_test.c"),
                    "-o", str(executable),
                ],
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
