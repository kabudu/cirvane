# Engineering review

## Scope and current finding

The imported firmware has established hardware evidence for bounded service operation, configuration recovery, signed rollback, malformed input rejection and quiet production-shell behaviour. Material residual findings are authenticated OTA absence, software-only update trust without hardware provisioning, privileged unauthenticated USB shell, single-device reproducibility, rollback/security recapture under the Cirvane prompt, and remaining productisation gates.

## Simplicity decisions

- Preserve ESP-IDF/FreeRTOS as the first labelled Cirvane firmware under ADR 0005; require a FreeRTOS-free Cirvane kernel only for a later kernel-labelled release under ADR 0002.
- Use a single self-contained firmware image and no managed runtime service.
- Keep OTA retrieval as a thin adapter outside verification policy.
- Keep private CI local and repository-owned until public-opening approval.

## Completion audit state

The private repository identity work is complete. Current firmware, USB prompt
and application binary use the Cirvane name on ESP-IDF/FreeRTOS. Kernel novelty
research, the
portable kernel core with host tests, a flash-backed two-slot configuration
journal, a fail-closed dual-slot rollback policy, a fail-closed UART, GPIO,
timer, flash-read/erase/write, watchdog and entropy HAL (radio excluded), HIL
and production spike compile profiles, a quiet production `cirvane>` shell
with frozen crash and evidence lines, and a FreeRTOS-free ESP32-C5 spike with
real-board `benchmarks/results/kernel-spike.json` evidence are in. Live
`otadata` backup/write/restore is on the HIL image. Radio is excluded from the
kernel by ADR 0004. Adversarial HIL markers pass. Cirvane eval samples are in
`matched-eval.json` (`result=pass`, Nucleus class-1 n=30, Cirvane `stale_total=0`).
Matched evaluation still lists incomparable classes, units and Wi-Fi.
Authenticated OTA and developer release remain incomplete. Stage 1 novelty
remains a provisional hypothesis.
Energy, irreversible hardware provisioning, independent novelty challenge,
clean-room reproduction, formal verification and independent penetration testing
are accepted release deferrals and must remain visible limitations.
