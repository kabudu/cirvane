#!/usr/bin/env python3
"""Release-process unit tests."""

from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("prepare_release", ROOT / "scripts/prepare_release.py")
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)

BASE = """# Changelog

## [Unreleased]

### Added

- Feature.

[Unreleased]: https://github.com/kabudu/cirvane/compare/HEAD...HEAD
"""


class ReleaseProcessTests(unittest.TestCase):
    def test_first_release(self):
        result = MODULE.prepare(BASE, "0.1.0", "2026-09-12")
        self.assertIn("## [Unreleased]\n\n## [0.1.0] - 2026-09-12", result)
        self.assertIn("compare/v0.1.0...HEAD", result)
        self.assertIn("releases/tag/v0.1.0", result)

    def test_second_release_compares_tags(self):
        first = MODULE.prepare(BASE, "0.1.0", "2026-09-12")
        first = first.replace("## [Unreleased]\n", "## [Unreleased]\n\n### Fixed\n\n- Fix.\n", 1)
        result = MODULE.prepare(first, "0.1.1", "2026-09-13")
        self.assertIn("compare/v0.1.0...v0.1.1", result)

    def test_refuses_empty_or_duplicate_release(self):
        with self.assertRaises(ValueError):
            MODULE.prepare(BASE.replace("- Feature.\n", ""), "0.1.0", "2026-09-12")
        first = MODULE.prepare(BASE, "0.1.0", "2026-09-12")
        with self.assertRaises(ValueError):
            MODULE.prepare(first, "0.1.0", "2026-09-13")


if __name__ == "__main__":
    unittest.main(verbosity=2)
