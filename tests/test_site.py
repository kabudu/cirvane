#!/usr/bin/env python3
"""Contract checks for the public site source."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SiteContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.html = (ROOT / "site/index.html").read_text(encoding="utf-8")
        cls.css = (ROOT / "site/styles.css").read_text(encoding="utf-8")

    def test_claim_boundaries_are_visible(self) -> None:
        for text in (
            "developer release",
            "Authenticated remote OTA",
            "powered by ESP-IDF and FreeRTOS",
            "XIAO ESP32-C5",
        ):
            self.assertIn(text, self.html)
        self.assertIn('id="install"', self.html)
        self.assertIn("releases/tag/v0.1.0", self.html)

    def test_accessibility_basics(self) -> None:
        self.assertIn('class="skip-link"', self.html)
        self.assertIn('prefers-reduced-motion: reduce', self.css)
        self.assertIn('@media (forced-colors: active)', self.css)
        self.assertNotIn('target="_blank"', self.html)
        self.assertIn("overflow-wrap: anywhere", self.css)
        self.assertIn("pre { max-width: 100%; min-width: 0; }", self.css)

    def test_site_has_no_remote_runtime_assets(self) -> None:
        self.assertNotIn("<script", self.html)
        self.assertNotIn("fonts.googleapis.com", self.html)
        self.assertNotIn("analytics", self.html.lower())


if __name__ == "__main__":
    unittest.main()
