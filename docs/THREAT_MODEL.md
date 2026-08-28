# Threat model

## Assets and adversaries

Assets are firmware authenticity, update policy, selected boot partition, valid configuration, runtime availability, signing-key confidentiality and operator control. Adversaries include a malicious OTA origin, network attacker, malformed local operator, compromised integration, corrupted storage and a person with physical access to an unprovisioned development board.

## Security invariants

- Network or adapter success never substitutes for manifest and image verification.
- Unsupported, partial, timed-out or malformed updates fail closed.
- Update work, memory, redirects, retries and download length remain bounded.
- The selected boot partition changes only after complete policy and cryptographic verification.
- Secrets and private signing keys never enter firmware source, logs, fixtures or evidence.

## Existing controls

Strict numeric parsing, static resource bounds, strong stack protection, transactional CRC configuration, ECDSA-signed images, dual-slot rollback, compile-gated HIL diagnostics and adversarial hardware tests are implemented.

## Residual risks and deferrals

Hardware Secure Boot, flash encryption, eFuse/JTAG/download restrictions and anti-rollback fuses are deferred because only one development device exists and provisioning is irreversible. The USB shell is privileged and unauthenticated. Physical fault injection, RF fuzzing, side-channel analysis and independent penetration testing are not complete. Authenticated OTA transport, radio and a complete production kernel remain planned. Live ESP `otadata` selection is implemented as backup/write/restore with fail-closed policy; it is not signed-image verify. The spike production compile omits HIL inject and PASS reprint; it is not a product image. Stage 2 software capability leases are not PMP/PMA mapped. The spike HAL covers UART, GPIO 27, timer, flash read, config-window and otadata erase/write, watchdog mute and entropy only (ADR 0004). Image verification is an injected adapter; Cirvane refuses selection when that adapter fails.
