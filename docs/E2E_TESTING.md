# End-to-end testing

Tests exercise public device workflows through USB shell, build artefacts and OTA inputs rather than mutating internal state as a substitute.

## Required scenarios

- Clean production build, flash, boot, `info`, `selftest`, service/resource inspection and quiet prompt.
- Link-map and runtime inspection proving the production scheduler and recovery path contain no FreeRTOS runtime dependency. The Stage 1 spike source must also be FreeRTOS-free.
- Host models for recovery transactions, malformed syscalls, invalid capability, stale epoch, exhausted message pool, restart-budget refusal and panic sticky-refuse.
- Trap, timer, malformed syscall, invalid capability, stale epoch, exhausted message pool and restart-budget refusal paths on the real board.
- Recovery transaction success and fault paths proving stale work cannot execute after an epoch advance.
- Supported and malformed shell inputs, oversized lines and unsupported commands.
- Service fault, bounded recovery and degraded-state reporting in the explicit HIL profile.
- Configuration commit, torn/corrupt slot fallback and redundancy restoration.
- Authenticated OTA success, wrong manifest key, wrong product, stale version, replay, wrong length, digest mismatch, image-signature failure, truncation, timeout and redirect refusal.
- Pending-image boot confirmation, failure rollback and confirmed-image persistence.
- Removal by reflashing an alternative image and restoration from a known-good Cirvane image.

## Oracles and flake policy

Host cryptographic tests use ESP-IDF verification tooling independently of device shell strings. HIL tests compare selected partitions before and after rejection. Every wait is bounded and reports its observed tail. A transient serial transport failure is infrastructure failure, not product success; reruns are recorded rather than replacing failed evidence silently.
