# Cirvane

![Cirvane Recovery Scar identity](assets/brand/source/cirvane-horizontal.svg)

Cirvane is a bounded, supervised embedded operating-system project for constrained connected devices. Its first supported vertical is the Seeed Studio XIAO ESP32-C5.

The imported implementation currently uses ESP-IDF and FreeRTOS beneath a static service supervisor, bounded message bus, capability checks, transactional configuration, operational telemetry and signed dual-slot rollback. It is now the preserved migration and matched-evaluation baseline. The first Cirvane developer release is planned to use a clean-sheet ESP32-C5 kernel whose candidate contribution is a bounded recovery transaction; novelty remains a hypothesis until the repository's internal research and matched-evidence gates pass.

## Status

Cirvane is private and under productisation. The enduring Recovery Scar identity is complete and source-controlled. The current firmware baseline is implemented and hardware-tested under its former Nucleus development identity. Kernel novelty research has a systematic prior-art matrix, a frozen recovery-transaction hypothesis, a portable kernel core with host tests, a two-slot configuration journal persisted in a reserved flash window, a fail-closed dual-slot rollback policy, a fail-closed UART/GPIO/timer/flash-read/erase/write/watchdog/entropy HAL, HIL and production spike compile profiles, and a quiet production `cirvane>` shell with frozen crash and evidence lines. Radio, live `otadata` adapters, real-board adversarial qualification, matched evaluation, service migration and rename, authenticated OTA transport and the initial developer release remain separately gated.

Cirvane is not currently claimed to have an implemented or proven novel kernel, to be production-ready, physically secure, independently penetration-tested, energy-qualified, formally verified or suitable for safety-critical deployment.

## Evidence already established

- Real ESP32-C5 boot, shell, service supervision, Wi-Fi scan and rollback tests pass.
- Controlled measurements show a 39.05% lower restart median than the preserved v1 baseline and substantially lower restart variance.
- Adversarial input, corrupted configuration and corrupted OTA-image tests pass on hardware.
- The production profile excludes destructive HIL commands and keeps periodic telemetry clear of the interactive prompt.

Raw evidence and reproducible harnesses live under `benchmarks/`. The authoritative private-repository gate is `./scripts/ci-local.sh`; hosted CI is disabled by policy until the owner explicitly approves it at the public-opening gate.

## Documentation

- [Product specification](docs/PRODUCT_SPECIFICATION.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Implementation plan](docs/IMPLEMENTATION_PLAN.md)
- [Productisation completion plan](docs/PRODUCTISATION_COMPLETION_PLAN.md)
- [Threat model](docs/THREAT_MODEL.md)
- [Brand identity](docs/BRAND_IDENTITY.md)
- [Brand asset usage](docs/brand/ASSET_USAGE.md)
- [Validation](docs/VALIDATION.md)
- [Release policy](docs/RELEASE.md)
- [Prior art and claim boundary](docs/NOVELTY.md)
- [Kernel spike and invariants](docs/KERNEL_SPIKE.md)
- [Matched evaluation protocol](docs/MATCHED_EVALUATION.md)

## Name audit

`Cirvane` was selected on 2026-08-24 after exact-name checks across general web search, GitHub, npm, PyPI, crates.io, ESP component search and RDAP. No material same-market collision was found. This point-in-time audit is not trademark clearance or a permanent reservation; formal UK, EU and US legal review remains a public-release gate.
