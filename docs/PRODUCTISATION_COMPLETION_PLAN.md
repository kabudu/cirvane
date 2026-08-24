# Productisation completion plan

## Purpose and completion boundary

This plan defines the remaining work required to create Cirvane's first explicitly labelled developer release. It converts the remaining implementation milestones into three sequential, evidence-gated stages. A checked item requires implemented behaviour and the named evidence; documentation, an unflashed binary or a passing host-only test is not sufficient where real-board evidence is required.

The enduring Cirvane brand identity is complete. The current firmware baseline remains implemented under historical Nucleus identifiers, authenticated OTA transport is not yet implemented, and no Cirvane release has been authorised.

Completion means all three stages have passed at one release candidate commit, every stop-ship condition is closed, the accepted deferrals are stated without implying completion, and the owner has approved the exact version and repository visibility.

## Delivery sequence

1. Complete and qualify the Nucleus-to-Cirvane rename.
2. Implement and adversarially qualify authenticated OTA transport.
3. Freeze, validate and publish the initial developer release after explicit owner approval.

Each stage is delivered through a scoped feature branch, authoritative `./scripts/ci-local.sh`, reviewed pull request and squash merge to `master`. While the repository is private, hosted CI remains disabled by policy and absent hosted checks must not be described as passing.

## Stage 1: complete Cirvane rename

### Objective

Make Cirvane the consistent current product identity across firmware, source, operator surfaces, documentation and build artefacts while retaining Nucleus only where it truthfully identifies historical evidence.

### Implementation checklist

- [ ] Rename the ESP-IDF project, application metadata, binary names and current build identifiers from Nucleus to Cirvane.
- [ ] Rename current source symbols, component-facing identifiers, log tags, shell prompt, help text and operator commands where compatibility does not require an alias.
- [ ] Define any temporary command or configuration aliases, their warnings and their removal version; do not preserve accidental compatibility silently.
- [ ] Update current documentation, scripts, tests, fixtures and release surfaces to use Cirvane consistently.
- [ ] Preserve benchmark and hardware evidence captured under Nucleus with explicit historical provenance rather than rewriting the recorded identity.
- [ ] Add a repository-wide identity scan that rejects unintended current-product Nucleus references while allowing declared historical paths and quotations.
- [ ] Build the signed production profile and confirm HIL-only destructive commands remain absent.
- [ ] Flash the renamed image to the supported Seeed Studio XIAO ESP32-C5 and capture functional, rollback, security and quiet-shell evidence.
- [ ] Re-run the performance checks required to show that the rename did not regress the established runtime envelope.

### Required evidence

- Authoritative local-CI result at the merged rename commit.
- Signed production binary whose metadata and operator-visible identity are Cirvane.
- Real-board functional evidence covering boot, shell, service supervision, configuration, Wi-Fi scan and normal restart behaviour.
- Real-board rollback and security evidence covering rejected corrupt configuration and rejected corrupt update paths.
- Quiet-shell evidence proving periodic telemetry does not obscure `cirvane>`.
- Repository identity-scan result and an inventory of intentionally retained historical Nucleus references.

### Exit gate

Stage 1 passes only when the supported board boots and operates entirely as Cirvane, all required real-board evidence is tied to the merged commit, historical evidence remains truthful, and no material runtime or rollback regression is open.

## Stage 2: authenticated OTA transport

### Objective

Add a bounded, fail-closed transport and manifest layer so no remotely retrieved image can become the selected boot image before authenticity, identity, freshness, completeness and policy checks pass.

### Protocol and trust decisions

- [ ] Freeze a versioned manifest schema and canonical encoding suitable for deterministic signature verification.
- [ ] Define product and device identity fields, image version ordering, downgrade policy, image length and digest fields, key identifiers and expiry or freshness semantics.
- [ ] Define replay protection, interrupted-download recovery, duplicate-request behaviour and the exact boundary between transport success and update acceptance.
- [ ] Define signing-key rotation and revocation metadata without storing private signing keys in firmware or the repository.
- [ ] Record the protocol and trust-boundary decision in repository documentation and an ADR when the decision is material.

### Implementation checklist

- [ ] Implement bounded HTTPS retrieval using certificate verification, explicit connection and read timeouts, bounded redirects and a maximum manifest and image size.
- [ ] Reject scheme downgrade, untrusted certificates, disallowed origins, redirect loops, cross-origin credential forwarding and ambiguous content length.
- [ ] Verify canonical manifest signature and key status before accepting manifest claims.
- [ ] Verify product identity, supported device identity, version policy, declared length and final image digest before changing the selected boot target.
- [ ] Stream into the inactive slot using bounded buffers and abort safely on truncation, timeout, disconnect, overflow or storage failure.
- [ ] Preserve the current selected image and clean partial staging state after every rejected or interrupted attempt.
- [ ] Expose operator-visible reason codes and bounded telemetry without storing credentials or complete manifests by default.
- [ ] Add deterministic host tests and a public-workflow real-board harness for successful update, rejection and rollback behaviour.

### Adversarial acceptance matrix

- [ ] Valid manifest and image complete successfully and boot pending is selected only after every check passes.
- [ ] Corrupt image, wrong digest, wrong signing key, revoked key and malformed signature are rejected.
- [ ] Wrong product, wrong device, stale version, forbidden downgrade, expired metadata and replayed metadata are rejected.
- [ ] Truncated body, oversized body, inconsistent length, timeout, disconnect and storage failure leave the current boot target unchanged.
- [ ] HTTP downgrade, untrusted certificate, redirect loop, excessive redirects and disallowed redirect origin are rejected.
- [ ] A boot-pending image that fails the health-confirmation window rolls back to the prior verified image.
- [ ] Logs, evidence and test fixtures contain no private keys, bearer credentials or unredacted secret material.

### Required evidence

- Frozen protocol specification and trust-boundary decision.
- Unit and integration results for canonical encoding, signature verification, version policy and failure classification.
- Real-board OTA evidence matrix tied to the merged implementation commit.
- Successful update and failed-health rollback traces through the normal operator workflow.
- Secret scan, production binary inspection and authoritative local-CI result.

### Exit gate

Stage 2 passes only when every acceptance-matrix case has a deterministic verdict, no rejected path changes the selected boot image, successful and rollback paths pass on the supported board, resource bounds are enforced, and no material security finding remains open.

## Stage 3: initial developer release

### Objective

Freeze one evidence-backed commit as Cirvane's first developer release without overstating hardware security, energy performance, independent assurance, platform support or kernel novelty.

### Release-candidate checklist

- [ ] Select the release version and theme using the title format `Cirvane vX.Y.Z: <theme>`.
- [ ] Freeze one release candidate commit containing the completed rename, authenticated OTA transport, documentation, manifests and release metadata.
- [ ] Run authoritative `./scripts/ci-local.sh` at that exact commit in the documented ESP-IDF environment.
- [ ] Rebuild the signed production image from the release commit and record its digest, size, toolchain and configuration identity.
- [ ] Repeat all supported real-board functional, security, OTA, rollback and quiet-shell gates against that exact image.
- [ ] Verify deterministic brand exports, asset-manifest integrity, licence inventory and consistent Cirvane identity across release surfaces.
- [ ] Prepare curated release notes with one outcome paragraph, three to five material changes, compatibility and claim boundaries, one primary installation path and evidence links.
- [ ] Render and inspect release notes at desktop and narrow widths, checking hierarchy, wrapping, links, code blocks and placeholder text.
- [ ] Document upgrade, installation, rollback, recovery and removal procedures for the supported board.
- [ ] Complete the licence and name or mark checks appropriate to the chosen private or public release channel.
- [ ] Obtain explicit owner approval for the exact commit, version, release notes, artefacts and repository visibility.
- [ ] Create and verify the release only after all preceding items pass.

### Accepted initial-release deferrals

The following are explicitly outside the first developer-release completion boundary and must appear as deferred, not passed:

- Energy measurement and any energy-superiority claim, because calibrated external instrumentation is unavailable.
- Hardware Secure Boot, flash encryption and irreversible eFuse provisioning on the only development board.
- Independent penetration testing.
- Additional hardware ports, adoption cohorts and practitioner studies.
- A novel custom kernel and any associated novelty, isolation or matched-performance claim.

These deferrals do not waive software signing, authenticated OTA, rollback, secret handling, production-profile or claim-discipline requirements.

### Release evidence bundle

- Release commit and annotated version tag.
- Curated release-notes source and approved rendered previews.
- Signed production artefact digests and reproducible build identity.
- Authoritative local-CI output and real-board evidence bundle.
- OTA adversarial matrix and rollback evidence.
- Licence, provenance, limitation and deferred-work notices.
- Owner approval record and verified repository visibility.

### Exit gate

Stage 3 passes only when every non-deferred release requirement is satisfied at the same commit, the published claims match the evidence, the owner explicitly authorises the release and visibility, and the canonical release surface has been inspected after publication.

## Stop-ship conditions

Do not release while any of the following is present:

- A secret, private signing key, credential or sensitive manifest is exposed.
- Firmware, shell, binary or documentation identity is materially ambiguous between Nucleus and Cirvane.
- Any rejected or interrupted OTA path can change the selected boot image.
- Functional boot, health confirmation or rollback is broken on the supported board.
- A production image exposes HIL-only destructive commands.
- Local CI or required real-board evidence is missing for the release commit.
- Licence or provenance is incomplete, or a material same-market name or mark collision remains unresolved for the intended channel.
- Release copy implies production readiness, physical security, energy qualification, independent penetration testing, a novel kernel or support beyond the tested board.
- Repository visibility differs from the owner's explicit approval.

## Completion record

| Stage | State | Completion evidence |
|---|---|---|
| 1. Cirvane rename | Planned | Pending merged rename PR and real-board qualification bundle |
| 2. Authenticated OTA | Planned | Pending merged protocol implementation and adversarial evidence matrix |
| 3. Developer release | Planned | Pending release-candidate evidence, owner approval and verified release |

States are planned, in progress, implemented, verified, deferred or blocked. The completion record advances only after its named evidence exists.
