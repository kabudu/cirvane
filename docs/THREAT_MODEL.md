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

Hardware Secure Boot, flash encryption, eFuse/JTAG/download restrictions and anti-rollback fuses are deferred because only one development device exists and provisioning is irreversible. The USB shell is privileged and unauthenticated. Physical fault injection, RF fuzzing, side-channel analysis and independent penetration testing are not complete. Authenticated OTA transport, durable flash-backed config, live ESP `otadata` selection and a complete production kernel (drivers and isolation) remain planned. Stage 2 software capability leases are not PMP/PMA mapped. Image verification is an injected adapter; Cirvane refuses selection when that adapter fails. Stage 1 enumerates privileged trap, PMP, unsafe-code, vendor binary, radio callback, capability-revocation and stale-epoch surfaces in `docs/KERNEL_SPIKE.md` and `docs/KERNEL.md`; they must still be adversarially tested.
