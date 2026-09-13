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

    def test_code_colours_meet_wcag_aa_contrast(self) -> None:
        def channel(value: int) -> float:
            component = value / 255
            return component / 12.92 if component <= 0.04045 else ((component + 0.055) / 1.055) ** 2.4

        def luminance(colour: str) -> float:
            red, green, blue = (int(colour[index:index + 2], 16) for index in (1, 3, 5))
            return 0.2126 * channel(red) + 0.7152 * channel(green) + 0.0722 * channel(blue)

        def contrast(foreground: str, background: str) -> float:
            lighter, darker = sorted((luminance(foreground), luminance(background)), reverse=True)
            return (lighter + 0.05) / (darker + 0.05)

        self.assertRegex(self.css, r"code \{ color: var\(--aubergine\)")
        self.assertRegex(self.css, r"pre code \{ color: #e7ecec; \}")
        self.assertGreaterEqual(contrast("#3a2748", "#f8f6ef"), 4.5)
        self.assertGreaterEqual(contrast("#e7ecec", "#111416"), 4.5)

    def test_site_has_no_remote_runtime_assets(self) -> None:
        self.assertNotIn("<script", self.html)
        self.assertNotIn("fonts.googleapis.com", self.html)
        self.assertNotIn("analytics", self.html.lower())


if __name__ == "__main__":
    unittest.main()
