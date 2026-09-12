# Public continuous integration

Owner authorised hosted CI and GitHub Pages on 2026-09-12 as part of the explicit request to complete Cirvane's public release. Local CI remains the authoritative reproducibility gate; hosted CI independently repeats it on public changes.

## Workflows

- `ci.yml` runs the complete local gate in the pinned ESP-IDF v6.0.2 container. It explicitly trusts only GitHub's runner-owned checkout path, has read-only repository permissions, a bounded timeout and no secrets.
- `secret-scan.yml` scans full reachable Git history with a commit-pinned Gitleaks action. It has read-only repository permissions and no secrets.
- `pages.yml` builds the dependency-free static site, uploads only `build/site` and deploys through the GitHub Pages environment. Its deployment job alone receives the Pages and OIDC permissions required by GitHub.

Pull requests run validation but never deploy Pages. Release signing stays local: the owner-held private key is never stored in GitHub Actions, repository secrets or release automation.
