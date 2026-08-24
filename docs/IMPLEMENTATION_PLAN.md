# Implementation plan

Checkboxes represent implementation plus required evidence, never documentation alone.

## M0: source baseline and private delivery

- [x] Select Cirvane through an initial collision audit and owner approval.
- [x] Import the verified Nucleus firmware and benchmark evidence without local build state or secrets.
- [ ] Create and verify the private `kabudu/cirvane` repository with `master` and local-CI policy.

## M1: enduring product identity

- [ ] Produce three distinct visual directions and record non-leading evaluation.
- [ ] Select one direction and complete the accessible logo, colour, type, icon, diagram and chart systems.
- [ ] Add deterministic export tooling, design tokens, licences and asset manifest verification.

## M2: complete Cirvane rename

- [ ] Rename project identifiers, firmware metadata, shell prompt, commands, source symbols and documentation.
- [ ] Preserve baseline evidence provenance while labelling historical Nucleus records accurately.
- [ ] Rebuild, flash and pass functional, rollback, security and quiet-shell hardware gates.

## M3: authenticated OTA transport

- [ ] Freeze the manifest schema, canonical encoding, key identifiers, version policy and replay rules.
- [ ] Implement bounded HTTPS retrieval with certificate verification and strict redirects/timeouts.
- [ ] Verify manifest signature, device/product identity, version, length and image digest before selection.
- [ ] Add key rotation and revocation metadata without embedding private keys.
- [ ] Pass success, corruption, wrong-key, stale-version, replay, truncation, timeout, redirect and rollback tests.

## M4: initial developer release

- [ ] Pass authoritative local CI and all supported real-board gates at the release commit.
- [ ] Produce curated release notes and verify desktop and narrow rendering.
- [ ] Record energy measurement, hardware security provisioning and independent penetration testing as deferred, not passed.
- [ ] Complete licence and trademark gates appropriate to the chosen private or public release channel.
- [ ] Create the initial developer release only after explicit owner authorization.

## Optional post-release evidence

- [ ] Practitioner interviews and comprehension study.
- [ ] Additional hardware ports and adoption cohort.
- [ ] Novel-kernel research ADR, prototype and matched baseline evaluation.
