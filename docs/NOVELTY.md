# Novelty and claim boundary

Cirvane currently claims no novel kernel. Its implemented baseline combines established ESP-IDF/FreeRTOS facilities with a bounded service supervisor, static message bus, cooperative capability checks, transactional configuration and signed rollback policy. The combination may be useful and differentiated as a product, but combination alone is not a research novelty claim.

## Candidate future research

NOV-01 asks whether a small RISC-V embedded kernel can make bounded scheduling, service reconstruction, resource capabilities and OTA-aware recovery first-class semantics while retaining vendor radio compatibility through a narrow enclave.

**Falsification:** systematic prior-art comparison, an independently reviewed specification, matched implementations against FreeRTOS, Zephyr and Tock, adversarial fault workloads and evidence that the mechanism improves a pre-registered property without weakening the claim boundary.

## Non-claims

Cirvane does not claim invention of scheduling, message passing, capability systems, watchdog supervision, transactional storage, signed updates, rollback or ESP32 support. It does not claim formal soundness, hard isolation, safety or physical security.

## Search protocol

Before any novelty claim, search current academic databases, conference programmes, preprints, official RTOS documentation, standards, open-source hosts and patent databases. Record queries, dates, inclusion decisions, trust boundaries, implementations, metrics and negative findings. External expert challenge and clean-room reproduction remain mandatory claim gates.
