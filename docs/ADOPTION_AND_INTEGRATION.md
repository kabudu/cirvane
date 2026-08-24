# Adoption and integration

## Personas

- Firmware developers evaluating resilient service orchestration on an ESP32-C5.
- Maintainers diagnosing field behaviour through bounded local telemetry.
- Security reviewers assessing update and recovery boundaries.

## Staged adoption

1. **Observe:** flash a development board, inspect `info`, `svc`, `res`, `bus` and `selftest` without enabling remote update transport.
2. **Evaluate:** run preserved baseline benchmarks and HIL tests on supported hardware.
3. **Adopt selectively:** move one device workflow behind a Cirvane service and retain a known recovery image.
4. **Require:** allow Cirvane policy to control update selection only after authenticated OTA and rollback gates pass.

There is no claim of measured adoption, market fit or reduced onboarding effort. Interviews and adoption cohorts are optional post-release evidence.

## Lifecycle

Installation uses ESP-IDF build and USB flashing. Upgrade must preserve a known-good slot, validate the manifest and signed image, and require post-boot confirmation. Downgrade is refused by policy unless explicitly authorized for recovery. Removal is a normal reflash of another compatible image; configuration and evidence formats remain documented and exportable.

## Integration boundary

OTA origins, discovery and future management tools are adapters. They may supply bytes and signed metadata but may not reinterpret Cirvane verification results. Credentials must remain outside source control and be scoped to retrieval, not firmware signing.
