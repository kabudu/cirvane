# Engineering review

## Public source-opening review (2026-08-30)

The public-readiness review corrected a root-licence mismatch in README and three project SPDX identifiers, documented the vendored Espressif console example, and added contribution, DCO, conduct, governance, support, private security disclosure, citation, issue and pull-request policies. Gitleaks 8.30.1 scanned all 34 commits through `e49e4b2` with zero findings. No build tree, signing key, SDK state or `.DS_Store` is tracked.

Source visibility is deliberately separated from a versioned developer release. Authenticated OTA hardware qualification, hardware security provisioning, energy qualification, independent penetration testing, formal verification and the clean-sheet kernel remain incomplete and retain their existing claim limits. Remaining visibility decisions are professional or explicit owner acceptance of name, mark and AI-assisted asset rights risk; approval of the exact public commit; and immediate enablement of private vulnerability reporting after the repository becomes public. Hosted CI remains disabled until separately approved.

## Scope and current finding

The imported firmware has established hardware evidence for bounded service operation, configuration recovery, signed rollback, malformed input rejection and quiet production-shell behaviour. The Stage 3 forensic review found and corrected four material baseline defects: generic HIL builds ran the rebooting matched-evaluation campaign, the configuration journal used the undocumented `cirvane_v2` namespace instead of the accepted `cirvane` namespace, generation selection was not wrap-safe, and power/OTA failure paths could report success or dereference an unavailable partition. Material residual findings are authenticated OTA absence, software-only update trust without hardware provisioning, privileged unauthenticated USB shell, single-device reproducibility and remaining productisation gates.

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
Renamed Cirvane functional, security, signed rollback and performance evidence
is recorded under `benchmarks/results/cirvane-*`. Each new hardware record is
tied to the tested firmware digest. Generic HIL and matched-evaluation boot
profiles are now separate. Authenticated OTA and developer release remain incomplete. Stage 1 novelty
remains a provisional hypothesis.
Energy, irreversible hardware provisioning, independent novelty challenge,
clean-room reproduction, formal verification and independent penetration testing
are accepted release deferrals and must remain visible limitations.
