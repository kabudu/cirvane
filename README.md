# Cirvane

![Cirvane Recovery Scar identity](assets/brand/exports/cirvane-horizontal-680.png)

Cirvane is a small, resilient device runtime for connected embedded systems. It keeps services, messages, configuration and firmware recovery predictable when software fails or input is malformed.

The first supported device is the Seeed Studio XIAO ESP32-C5. The current firmware runs on ESP-IDF and FreeRTOS.

## What Cirvane provides

- **Supervised services:** fixed service identities, health checks and bounded restart behaviour.
- **Bounded messaging:** typed messages use a fixed pool, so queues cannot grow without limit.
- **Recoverable configuration:** two CRC-protected records preserve the last valid settings after an interrupted or corrupt write.
- **Safe firmware rollback:** signed dual-slot images must boot successfully and be confirmed, or the device returns to the previous image.
- **Clear diagnostics:** a local USB shell reports device identity, service health, resource use, message drops and update state.
- **Production separation:** destructive hardware-test commands are compile-gated and excluded from production images.

## How it works

Cirvane starts a fixed set of services and monitors their health. Services communicate through bounded message slots instead of allocating unbounded work. If a service fails, Cirvane applies a limited recovery policy and reports the outcome.

Configuration changes are written to an alternate record and verified before becoming current. Firmware updates use two application slots, leaving a known-good image available when a new image fails its first-boot health check.

## Try it on supported hardware

You need a XIAO ESP32-C5, USB connection and the pinned ESP-IDF v6.0.2 toolchain.

Run the authoritative local checks:

```sh
source /path/to/esp-idf/export.sh
./scripts/ci-local.sh
```

After flashing a development image, connect to the USB serial port at 115200 baud. Useful shell commands include:

```text
info
selftest
svc
res
bus
scan
wifi connect
wifi status
wifi disconnect
ota-status
ota-update v0.1.0
```

`wifi connect` scans for nearby networks and asks you to choose one. Password entry is masked. Use `wifi connect "<ssid>"` to connect directly to a known or hidden network. Credentials stay in driver RAM for the active session and are cleared on disconnect or reboot.

`ota-update` retrieves an exact Cirvane release from GitHub, verifies its signed manifest and streams the signed firmware into the inactive slot. See [Authenticated OTA](docs/OTA_PROTOCOL.md) for the trust model and recovery workflow.

See [Operations](docs/OPERATIONS.md) for the device lifecycle and recovery workflow, and [Adoption and integration](docs/ADOPTION_AND_INTEGRATION.md) for a staged evaluation path.

## Current status

Cirvane is pre-release software under active productisation. Source access does not imply production readiness, safety certification or support for unattended deployment.

Validated on a real XIAO ESP32-C5:

- boot, shell, service supervision and Wi-Fi scanning;
- malformed-input and configuration-corruption handling;
- signed update staging, confirmation and automatic rollback;
- production removal of destructive test commands;
- repeatable build, host-test and hardware-test evidence.

Authenticated remote OTA is implemented and remains release-gated on the recorded real-board adversarial matrix. The USB shell is a privileged local interface and does not provide user authentication. Hardware Secure Boot, flash encryption, energy qualification, independent penetration testing and safety certification are outside the current release claim.

Evidence and reproducible test harnesses are stored in [`benchmarks/`](benchmarks/). `./scripts/ci-local.sh` is the authoritative validation gate; public hosted CI repeats it for every change to `master`.

## Project documentation

- [Product specification](docs/PRODUCT_SPECIFICATION.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Operations](docs/OPERATIONS.md)
- [Threat model](docs/THREAT_MODEL.md)
- [Validation](docs/VALIDATION.md)
- [Release policy](docs/RELEASE.md)
- [Implementation plan](docs/IMPLEMENTATION_PLAN.md)
- [Brand identity](docs/BRAND_IDENTITY.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Open-source readiness](docs/OPEN_SOURCE_READINESS.md)

Cirvane also contains a clean-sheet kernel research track. It is separate from the current ESP-IDF/FreeRTOS product firmware and is documented in [Kernel spike and invariants](docs/KERNEL_SPIKE.md).

## Licence

Cirvane is licensed under the [Apache License 2.0](LICENSE). Third-party material is listed in [Third-party notices](THIRD_PARTY_NOTICES.md).
