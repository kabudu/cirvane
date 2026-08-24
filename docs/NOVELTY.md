# Novelty and claim boundary

Cirvane currently claims no implemented or proven novel kernel. Its implemented baseline combines established ESP-IDF/FreeRTOS facilities with a bounded service supervisor, static message bus, cooperative capability checks, transactional configuration and signed rollback policy. That baseline is now preserved for migration and matched evaluation, not planned as the kernel of the first Cirvane release.

The candidate contribution is a bounded recovery transaction as a first-class kernel primitive: one atomic transition coupling a service epoch, stale-work invalidation, fixed resource reclamation, restart budget, recoverable state generation and fixed-size observable outcome. The hypothesis is narrow and falsifiable. It remains unsupported until systematic comparison, a FreeRTOS-free ESP32-C5 prototype and matched fault workloads establish a difference from close systems.

## Candidate kernel research

NOV-01 asks whether a small RISC-V embedded kernel can make bounded scheduling, service reconstruction, resource capabilities and OTA-aware recovery first-class semantics while retaining vendor radio compatibility through a narrow enclave.

**Falsification:** systematic prior-art comparison, an independently reviewed specification, matched implementations against FreeRTOS, Zephyr and Tock, adversarial fault workloads and evidence that the mechanism improves a pre-registered property without weakening the claim boundary.

## Non-claims

Cirvane does not claim invention of scheduling, message passing, capability systems, epoch tagging, watchdog supervision, restart policies, resource reclamation, transactional storage, event logging, signed updates, rollback or ESP32 support. It does not claim definitive worldwide novelty, formal soundness, verified correctness, hard isolation, safety or physical security.

Independent novelty challenge and clean-room reproduction are optional post-release assurance stages for the initial ESP32-C5 developer release. Their deferral must be visible and prohibits independent-validation claims; it does not replace the internal prior-art, falsification and matched-evidence gates required before any qualified novelty wording.

## Search protocol

Before any novelty claim, search current academic databases, conference programmes, preprints, official RTOS documentation, standards, open-source hosts and patent databases. Record queries, dates, inclusion decisions, trust boundaries, implementations, metrics and negative findings. The initial release may use carefully qualified candidate-novelty wording only after the internal systematic comparison and falsification gates pass. External expert challenge and clean-room reproduction remain optional post-release assurance gates and are required before any independent-validation or definitive novelty wording.
