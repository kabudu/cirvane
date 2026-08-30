#!/usr/bin/env python3
"""Compile and execute portable Wi-Fi input-policy tests."""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class WifiPolicyTests(unittest.TestCase):
    def test_wifi_input_boundaries(self):
        with tempfile.TemporaryDirectory(prefix="cirvane-wifi-") as directory:
            executable = Path(directory) / "wifi-policy-test"
            subprocess.run(
                [
                    "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "main"),
                    str(ROOT / "tests" / "wifi_policy_test.c"),
                    "-o", str(executable),
                ],
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
