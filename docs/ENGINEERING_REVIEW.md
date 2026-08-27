# Engineering review

## Scope and current finding

The imported firmware has established hardware evidence for bounded service operation, configuration recovery, signed rollback, malformed input rejection and quiet production-shell behaviour. Material residual findings are authenticated OTA absence, software-only update trust without hardware provisioning, privileged unauthenticated USB shell, single-device reproducibility and incomplete release identity/productisation.

## Simplicity decisions

- Preserve ESP-IDF/FreeRTOS as the migration and matched-evaluation baseline; require a FreeRTOS-free Cirvane kernel for the initial release under ADR 0002.
- Use a single self-contained firmware image and no managed runtime service.
- Keep OTA retrieval as a thin adapter outside verification policy.
- Keep private CI local and repository-owned until public-opening approval.

## Completion audit state

The private repository identity work is complete. Kernel novelty research, the
portable kernel core with host tests, a RAM-backed configuration journal, a
fail-closed dual-slot rollback policy, a fail-closed UART, GPIO, timer, flash-read,
watchdog and entropy HAL (radio excluded), HIL and production spike compile
profiles, and a FreeRTOS-free ESP32-C5 spike with real-board
`benchmarks/results/kernel-spike.json` evidence are in. Durable
flash/`otadata` adapters, radio, adversarial qualification, matched evaluation,
migration and rename, authenticated OTA and developer release remain
incomplete. Stage 1 novelty remains a provisional hypothesis.
Energy, irreversible hardware provisioning, independent novelty challenge,
clean-room reproduction, formal verification and independent penetration testing
are accepted release deferrals and must remain visible limitations.
