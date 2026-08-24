# Engineering review

## Scope and current finding

The imported firmware has established hardware evidence for bounded service operation, configuration recovery, signed rollback, malformed input rejection and quiet production-shell behaviour. Material residual findings are authenticated OTA absence, software-only update trust without hardware provisioning, privileged unauthenticated USB shell, single-device reproducibility and incomplete release identity/productisation.

## Simplicity decisions

- Preserve ESP-IDF/FreeRTOS as the migration and matched-evaluation baseline; require a FreeRTOS-free Cirvane kernel for the initial release under ADR 0002.
- Use a single self-contained firmware image and no managed runtime service.
- Keep OTA retrieval as a thin adapter outside verification policy.
- Keep private CI local and repository-owned until public-opening approval.

## Completion audit state

The private repository, kernel novelty research, clean-sheet kernel, migration and rename, authenticated OTA and developer release remain incomplete until their roadmap evidence is present. Energy, irreversible hardware provisioning, independent novelty challenge, clean-room reproduction, formal verification and independent penetration testing are accepted release deferrals and must remain visible limitations.
