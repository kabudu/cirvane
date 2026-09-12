#!/usr/bin/env python3
"""Validate the dependency-free GitHub Pages output."""

from html.parser import HTMLParser
from pathlib import Path
import sys
from urllib.parse import urlparse


class PageParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.ids: set[str] = set()
        self.links: list[tuple[str, str]] = []
        self.images: list[tuple[str, str | None]] = []
        self.title = ""
        self._in_title = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if values.get("id"):
            self.ids.add(values["id"] or "")
        if tag == "a" and values.get("href"):
            self.links.append((tag, values["href"] or ""))
        if tag == "link" and values.get("href"):
            self.links.append((tag, values["href"] or ""))
        if tag == "img":
            self.images.append((values.get("src") or "", values.get("alt")))
        if tag == "title":
            self._in_title = True

    def handle_endtag(self, tag: str) -> None:
        if tag == "title":
            self._in_title = False

    def handle_data(self, data: str) -> None:
        if self._in_title:
            self.title += data


def fail(message: str) -> None:
    raise SystemExit(f"site validation failed: {message}")


root = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else Path("build/site").resolve()
index = root / "index.html"
if not index.is_file():
    fail("index.html is missing")

for page in sorted(root.glob("*.html")):
    parser = PageParser()
    parser.feed(page.read_text(encoding="utf-8"))
    if not parser.title.strip():
        fail(f"{page.name} has no title")
    for src, alt in parser.images:
        if alt is None:
            fail(f"{page.name} image {src!r} has no alt attribute")
        if not src or not (page.parent / src).resolve().is_relative_to(root):
            fail(f"{page.name} has unsafe image path {src!r}")
        if not (page.parent / src).is_file():
            fail(f"{page.name} references missing image {src!r}")
    for _, href in parser.links:
        parsed = urlparse(href)
        if parsed.scheme in {"http", "https", "mailto"}:
            continue
        if parsed.scheme or href.startswith("//"):
            fail(f"{page.name} has unsupported link {href!r}")
        if href.startswith("#"):
            if href[1:] not in parser.ids:
                fail(f"{page.name} has missing fragment {href!r}")
            continue
        target = (page.parent / parsed.path).resolve()
        if not target.is_relative_to(root):
            fail(f"{page.name} link escapes site root: {href!r}")
        if parsed.path.endswith("/"):
            target /= "index.html"
        if not target.is_file():
            fail(f"{page.name} references missing file {href!r}")

print("site validation passed")
