# Threat model

## Assets and adversaries

Assets are firmware authenticity, update policy, selected boot partition, valid configuration, runtime availability, Wi-Fi credentials, signing-key confidentiality and operator control. Adversaries include a malicious OTA origin, network attacker, malformed local operator, compromised integration, corrupted storage and a person with physical access to an unprovisioned development board.

## Security invariants

- Network or adapter success never substitutes for manifest and image verification.
- Unsupported, partial, timed-out or malformed updates fail closed.
- Update work, memory, redirects, retries and download length remain bounded.
- The selected boot partition changes only after complete policy and cryptographic verification.
- Secrets and private signing keys never enter firmware source, logs, fixtures or evidence.
- Wi-Fi passwords never enter shell arguments, command history, Cirvane NVS, logs or evidence; driver storage is RAM-only and is cleared on disconnect.

## Existing controls

Strict numeric parsing, static resource bounds, strong stack protection, transactional CRC configuration, ECDSA-signed images, dual-slot rollback, signed canonical OTA manifests, monotonic replay protection, fixed HTTPS origins, bounded redirects and response sizes, masked Wi-Fi password entry, RAM-only Wi-Fi credentials, bounded connection waits, compile-gated HIL diagnostics and adversarial hardware tests are implemented. The OTA transport's real-board acceptance matrix remains a release gate until recorded at the release candidate.

## Residual risks and deferrals

Hardware Secure Boot, flash encryption, eFuse/JTAG/download restrictions and anti-rollback fuses are deferred because only one development device exists and provisioning is irreversible. A physical attacker can replace the software trust anchor on an unprovisioned board. The USB shell is privileged and unauthenticated, and a debugger or physical-memory attacker can inspect live driver RAM. Physical fault injection, RF fuzzing, side-channel analysis and independent penetration testing are not complete. Wi-Fi and authenticated OTA are implemented only on the ESP-IDF/FreeRTOS product firmware; radio on the clean-sheet kernel path remains excluded by ADR 0004. Live ESP `otadata` selection in the kernel research spike is backup/write/restore with fail-closed policy, not signed-image verification. The spike production compile omits HIL inject and PASS reprint; it is not a product image. Stage 2 software capability leases are not PMP/PMA mapped. The spike HAL covers UART, GPIO 27, timer, flash read, config-window and otadata erase/write, watchdog mute and entropy only (ADR 0004). Image verification in the kernel spike is an injected adapter; Cirvane refuses selection when that adapter fails.
