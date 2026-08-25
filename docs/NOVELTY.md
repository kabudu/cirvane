# Novelty and claim boundary

Cirvane currently claims no implemented or proven novel kernel. Its implemented
baseline combines established ESP-IDF/FreeRTOS facilities with a bounded
service supervisor, static message bus, cooperative capability checks,
transactional configuration and signed rollback policy. That baseline is
preserved for migration and matched evaluation, not planned as the kernel of
the first Cirvane release.

The candidate contribution is a bounded recovery transaction as a first-class
kernel primitive: one atomic transition coupling a service epoch, stale-work
invalidation, fixed resource reclamation, restart budget, recoverable state
generation and fixed-size observable outcome. The hypothesis is narrow and
falsifiable. Stage 1 recorded a systematic comparison (see
`PRIOR_ART_MATRIX.md`), a portable model with host tests, and a FreeRTOS-free
ESP32-C5 spike. Hubris remains the closest reviewed MCU OS. Novelty remains a
hypothesis until matched fault workloads (NOV-02) and any closer prior art
review complete the Stage 2 gates.

## Candidate kernel research

NOV-01 asks whether a reviewed MCU kernel already exposes that atomic recovery
coupling under fixed RAM and execution bounds.

**Falsification:** the Stage 1 matrix plus later matched prototypes against the
closest systems, especially Hubris supervisor restart with generations and
leases. Absence of an identical name is not proof.

NOV-02 asks whether kernel ownership reduces recovery latency variance and
eliminates stale post-restart work relative to the preserved FreeRTOS
baseline. **Falsification:** `docs/MATCHED_EVALUATION.md`.

NOV-03 asks whether the primitive remains usable without Cirvane shell or
branding. **Falsification:** an independent workload against the frozen ABI.

## Non-claims

Cirvane does not claim invention of scheduling, message passing, capability
systems, epoch tagging, watchdog supervision, restart policies, resource
reclamation, transactional storage, event logging, signed updates, rollback or
ESP32 support. It does not claim definitive worldwide novelty, formal
soundness, verified correctness, hard isolation, safety or physical security.

Independent novelty challenge and clean-room reproduction are optional
post-release assurance stages. Their deferral must stay visible.
