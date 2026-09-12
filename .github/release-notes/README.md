# Curated release notes

Each official tag has a matching `vMAJOR.MINOR.PATCH.md` file. The first line is the GitHub Release title in the form `Cirvane vX.Y.Z: <theme>`. The remaining source is the exact release body.

Run `python3 scripts/validate_release_metadata.py --tag vX.Y.Z` before publishing. Release notes summarise outcomes and boundaries; `CHANGELOG.md` retains the detailed history.
