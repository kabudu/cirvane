# Contributing to Cirvane

Cirvane welcomes focused bug reports, documentation corrections and code contributions that preserve its evidence and claim boundaries. The current supported product image targets the Seeed Studio XIAO ESP32-C5 and runs on ESP-IDF v6.0.2 with FreeRTOS. The clean-sheet kernel is a separate research track.

## Before opening an issue

Use GitHub Issues for reproducible defects and narrowly scoped proposals. Search existing issues first. Do not post vulnerabilities, credentials, private keys, Wi-Fi details or device identifiers in a public issue; follow `SECURITY.md` instead.

A useful defect report includes the tested commit, board revision, build profile, exact command, expected result, observed result and a redacted transcript. Label simulations and host-only results honestly.

## Development workflow

1. Fork the repository and branch from current `master`.
2. Keep the change narrow and preserve bounded memory, work, retries, queues and timeouts.
3. Update code, tests, documentation, traceability, risks and machine-readable evidence together when behaviour changes.
4. Bootstrap ESP-IDF v6.0.2, source its `export.sh`, then run `./scripts/ci-local.sh`.
5. Review the diff for secrets, generated build output, local SDK state and unsupported claims.
6. Open a pull request using the repository template.

Hardware-affecting changes require the relevant real-board harness and redacted evidence. Destructive diagnostics must remain compile-gated and absent from production images. Hosted CI is not currently configured; local CI evidence must be recorded in the pull request.

## Certificate of origin

Every commit must include a `Signed-off-by` trailer created with `git commit -s`. By signing off, you certify the [Developer Certificate of Origin 1.1](https://developercertificate.org/): you have the right to submit the contribution under this repository's licence. Contributions with unclear origin or incompatible licensing will not be accepted.

## Review and acceptance

Maintainers may request smaller scope, behavioural tests, provenance records or weaker wording. Passing tests does not override a security, correctness, licensing or claim-boundary concern. There is no guarantee that a proposal will be merged or placed on a release roadmap.

By participating, you agree to `CODE_OF_CONDUCT.md`.
