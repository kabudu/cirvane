#!/usr/bin/env python3
"""Compile and execute portable Cirvane runtime-boundary tests."""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RuntimeBoundsTests(unittest.TestCase):
    def test_serial_generation_arithmetic(self):
        with tempfile.TemporaryDirectory(prefix="cirvane-runtime-") as directory:
            executable = Path(directory) / "runtime-bounds-test"
            subprocess.run(
                [
                    "cc",
                    "-std=c11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "main"),
                    str(ROOT / "tests" / "runtime_bounds_test.c"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
