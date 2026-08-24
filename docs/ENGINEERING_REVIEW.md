# Engineering review

## Scope and current finding

The imported firmware has established hardware evidence for bounded service operation, configuration recovery, signed rollback, malformed input rejection and quiet production-shell behaviour. Material residual findings are authenticated OTA absence, software-only update trust without hardware provisioning, privileged unauthenticated USB shell, single-device reproducibility and incomplete release identity/productisation.

## Simplicity decisions

- Retain ESP-IDF/FreeRTOS for the initial release; a custom kernel is a separately falsifiable research increment.
- Use a single self-contained firmware image and no managed runtime service.
- Keep OTA retrieval as a thin adapter outside verification policy.
- Keep private CI local and repository-owned until public-opening approval.

## Completion audit state

The private repository, rename, brand system, authenticated OTA and developer release remain incomplete until their roadmap evidence is present. Energy, irreversible hardware provisioning and independent penetration testing are accepted release deferrals and must remain visible limitations.
