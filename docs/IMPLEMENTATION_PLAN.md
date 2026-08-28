# Implementation plan

Checkboxes represent implementation plus required evidence, never documentation alone.

The detailed path from the current productised identity to the first developer release is maintained in [PRODUCTISATION_COMPLETION_PLAN.md](PRODUCTISATION_COMPLETION_PLAN.md).

## M0: source baseline and private delivery

- [x] Select Cirvane through an initial collision audit and owner approval.
- [x] Import the verified Nucleus firmware and benchmark evidence without local build state or secrets.
- [x] Create and verify the private `kabudu/cirvane` repository with `master` and local-CI policy.

## M1: enduring product identity

- [x] Produce three distinct visual directions and record non-leading evaluation.
- [x] Select one direction and complete the accessible logo, colour, type, icon, diagram and chart systems.
- [x] Add deterministic export tooling, design tokens, licences and asset manifest verification.

## M2: kernel novelty and feasibility

- [x] Complete the systematic prior-art matrix and freeze the bounded recovery transaction hypothesis.
- [x] Build a minimal ESP32-C5 kernel spike without FreeRTOS and enumerate every vendor dependency.
- [x] Freeze kernel invariants, clean-sheet boundary, pre-registered matched evaluation and governing ADR.

## M3: clean-sheet Cirvane kernel

- [x] Implement boot, traps, interrupts, timer, static service scheduling, typed messages and capability enforcement.
- [x] Implement kernel-owned recovery transactions, bounded reclamation, transactional configuration and signed rollback.
- [ ] Pass real-board adversarial qualification and matched FreeRTOS baseline evaluation without hidden regressions.

## M4: Cirvane identity on ESP-IDF/FreeRTOS

- [x] Rename project identifiers, firmware metadata, shell prompt, commands, source symbols and documentation.
- [x] Preserve baseline evidence provenance while labelling historical Nucleus records accurately.
- [x] Flash the renamed image and capture identity, functional and quiet-shell evidence with prompt `cirvane>`.
- [ ] Recapture rollback and security hardware gates with the renamed image.

## M5: authenticated OTA transport

- [ ] Freeze the manifest schema, canonical encoding, key identifiers, version policy and replay rules.
- [ ] Implement bounded HTTPS retrieval with certificate verification and strict redirects/timeouts.
- [ ] Verify manifest signature, device/product identity, version, length and image digest before selection.
- [ ] Add key rotation and revocation metadata without embedding private keys.
- [ ] Pass success, corruption, wrong-key, stale-version, replay, truncation, timeout, redirect and rollback tests.

## M6: initial developer release

- [ ] Pass authoritative local CI and all supported real-board gates at the release commit.
- [ ] Produce curated release notes and verify desktop and narrow rendering.
- [ ] Record energy measurement, hardware security provisioning, formal verification, independent reproduction, independent novelty challenge and independent penetration testing as deferred, not passed.
- [ ] Complete licence and trademark gates appropriate to the chosen private or public release channel.
- [ ] Create the initial developer release only after explicit owner authorization.

## Optional post-release evidence

- [ ] Practitioner interviews and comprehension study.
- [ ] Additional hardware ports and adoption cohort.
- [ ] Independent novelty challenge, clean-room reproduction and formal verification.
- [ ] Independent penetration testing and external security review.
