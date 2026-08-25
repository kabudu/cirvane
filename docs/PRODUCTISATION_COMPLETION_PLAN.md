# Productisation completion plan

## Purpose and completion boundary

This plan defines the remaining work required to create Cirvane's first explicitly labelled developer release with a clean-sheet kernel candidate. It combines the original rename, authenticated OTA and release stages with two preceding novelty and kernel implementation stages. A checked item requires implemented behaviour and the named evidence; documentation, an unflashed binary or a passing host-only test is not sufficient where real-board evidence is required.

The enduring Cirvane brand identity is complete. The current firmware baseline remains implemented under historical Nucleus identifiers on ESP-IDF and FreeRTOS, authenticated OTA transport is not yet implemented, and no Cirvane release has been authorised. FreeRTOS remains the matched baseline and migration source, not the kernel of the first Cirvane release.

Completion means all five stages have passed at one release candidate commit, every stop-ship condition is closed, the accepted deferrals are stated without implying completion, and the owner has approved the exact version and repository visibility. The first release may describe Cirvane as a clean-sheet kernel implementation only when dependency inspection proves that claim. It may describe the kernel mechanism as novel only when the internal prior-art and matched-evidence gates support carefully qualified wording.

## Delivery sequence

1. Freeze a narrow kernel contribution after systematic prior-art and ESP32-C5 feasibility research.
2. Build and qualify the clean-sheet Cirvane kernel on the ESP32-C5.
3. Complete and qualify the Nucleus-to-Cirvane migration and rename on that kernel.
4. Implement and adversarially qualify authenticated OTA transport.
5. Freeze, validate and publish the initial developer release after explicit owner approval.

Each stage is delivered through a scoped feature branch, authoritative `./scripts/ci-local.sh`, reviewed pull request and squash merge to `master`. While the repository is private, hosted CI remains disabled by policy and absent hosted checks must not be described as passing.

## Stage 1: kernel novelty and feasibility freeze

### Objective

Select one narrow, falsifiable kernel mechanism that is not merely FreeRTOS replacement, a new scheduler name or a combination of familiar RTOS features. Establish that the ESP32-C5 can support the required privilege, interrupt, timer, memory-protection, flash, radio and boot boundaries without retaining FreeRTOS as the hidden execution kernel.

### Candidate contribution

The initial candidate is a **bounded recovery transaction** as a first-class kernel primitive. A recovery transaction atomically associates a service epoch, fixed resource and restart budgets, owned message slots, capability leases, a recoverable state generation and a fixed-size evidence record. On admitted fault or deadline breach, the kernel invalidates stale epoch-owned work, reclaims bounded resources, advances the service epoch, selects the declared recovery transition and emits one deterministic outcome without allowing an error, timeout or partial recovery to become healthy state.

This is a candidate contribution, not an established novelty claim. Scheduling, message passing, capabilities, watchdogs, restart supervision, journalling, rollback, memory protection and event logs are prior art and must not be claimed as Cirvane inventions.

### Differentiation hypotheses

- **NOV-01:** No reviewed MCU kernel exposes atomic recovery transactions coupling service epoch, stale-work invalidation, bounded resource reclamation, restart policy and externally inspectable outcome under fixed RAM and execution bounds. **Falsification:** systematic literature, source and patent comparison plus matched prototypes against the closest discovered systems.
- **NOV-02:** Making recovery transactions kernel-owned reduces recovery latency variance and eliminates stale post-restart work relative to equivalent application-level supervision on the preserved FreeRTOS baseline. **Falsification:** pre-registered fault workloads with identical service scope, resource ceilings and hardware.
- **NOV-03:** The mechanism remains useful after removing the Cirvane-specific shell and branding. **Falsification:** a minimal independent workload must use the primitive through a documented kernel interface without depending on product-layer conventions.

### Prior-art and platform checklist

- [x] Record systematic searches across embedded kernels, fault-tolerant systems, recovery-oriented computing, restartable services, epoch reclamation, capability revocation, transactional state recovery and deterministic event logging.
- [x] Compare at minimum FreeRTOS, Zephyr, Tock, RIOT, Apache NuttX, seL4/Microkit and relevant research kernels at identical claim scope.
- [x] Record mechanism, trust boundary, hardware assumptions, resource model, failure semantics, evaluation and exact overlap for every close system.
- [x] Search recent papers, preprints, conference programmes, patents and source repositories; retain negative and contradictory results.
- [x] Freeze the top-level kernel invariants, state machine, syscall or message ABI, trusted-computing-base ledger and forbidden failure-state collapses.
- [x] Confirm ESP32-C5 Machine/User privilege, PMP/PMA and access-control behaviour from Espressif documentation and hardware probes.
- [x] Prove boot, trap, interrupt, timer, UART, allocator-free static memory and flash access in a minimal kernel spike.
- [x] Determine whether Wi-Fi and required vendor components can run through a bounded compatibility boundary without scheduling on FreeRTOS; reject any architecture that merely hides FreeRTOS below Cirvane.
- [x] Choose implementation language and unsafe-code boundary based on measurable correctness and toolchain support, not novelty theatre.
- [x] Pre-register matched baselines, workloads, metrics, resource ceilings, statistical treatment, stopping rules and missing-data handling before performance tuning.
- [x] Write an ADR freezing the candidate contribution, clean-sheet boundary, platform dependencies, non-goals and evidence required to change the decision.

### Feasibility and claim gate

Stage 1 passes only when a minimal ESP32-C5 kernel spike runs without FreeRTOS, the vendor dependency boundary is enumerated, the candidate mechanism survives an initial systematic comparison, the matched evaluation is pre-registered, and the claim wording remains explicitly provisional. If Wi-Fi cannot be supported without retaining FreeRTOS, the owner must explicitly choose between a narrower first release without Wi-Fi, a genuinely bounded non-kernel radio coprocessor boundary, or stopping the kernel release path.

### Primary research anchors

- Espressif documents ESP32-C5 Machine and User privilege levels plus PMP, PMA and access-permission controls: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/security/tee/tee-advanced.html>.
- ESP-IDF documents FreeRTOS as an integrated system component and exposes the current scheduling baseline: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/api-reference/system/freertos_idf.html>.
- Tock already combines a small Rust kernel, language-isolated capsules and hardware-isolated processes: <https://tockos.org/documentation/design/>.
- Zephyr already provides priority scheduling, userspace and hardware-backed memory domains: <https://docs.zephyrproject.org/latest/kernel/services/scheduling/index.html> and <https://docs.zephyrproject.org/latest/kernel/usermode/memory_domain.html>.
- seL4 already establishes a minimal capability-oriented microkernel and formal-verification benchmark: <https://docs.sel4.systems/projects/sel4/index.html>.

These anchors start the comparison; they do not complete literature or patent diligence.

## Stage 2: clean-sheet Cirvane kernel implementation

### Objective

Implement the smallest kernel that realizes the frozen recovery semantics, boots directly on the ESP32-C5, owns scheduling and isolation decisions, and supports the first release workflow without FreeRTOS in the runtime dependency graph.

### Kernel implementation checklist

- [ ] Implement reset entry, linker layout, stack initialization, trap vectors, interrupt dispatch, monotonic timer and deterministic panic path.
- [ ] Implement a fixed-capacity task or service table with explicit lifecycle states and no unbounded allocation after boot.
- [ ] Implement a scheduler with documented admission, priority, deadline, fairness and starvation semantics; keep the policy no more complex than the candidate mechanism requires.
- [ ] Implement typed fixed-capacity message transport with ownership, epoch tagging, bounded copy or transfer semantics and visible refusal on exhaustion.
- [ ] Implement capability grants and revocation with a machine-checkable mapping to PMP/PMA or documented software-only limits.
- [ ] Implement recovery transactions, stale-work invalidation, bounded reclamation, restart budgets, backoff and fixed-size evidence records as kernel-owned semantics.
- [ ] Implement transactional configuration and dual-slot signed rollback without weakening the proven fail-closed behaviour.
- [ ] Implement the minimum UART, GPIO, timer, flash, watchdog, entropy and radio or network boundaries required by the supported workflow.
- [ ] Keep vendor ROM, HAL and binary dependencies behind an enumerated adapter boundary; record licence, privilege, memory, callback and scheduling assumptions for each.
- [ ] Add deterministic host models for state machines plus emulator or simulator coverage where the target boundary permits it.
- [ ] Add production and HIL profiles; destructive diagnostics must remain compile-gated out of production images.
- [ ] Define stable crash and evidence formats, bounded observability and a quiet interactive shell path.

### Kernel verification matrix

- [ ] Boot determinism and boot-time variance are measured across controlled cold and warm starts.
- [ ] Scheduler latency, wake latency, message latency, interrupt latency and recovery latency are measured with tails and variance, not averages alone.
- [ ] Static RAM, stack high-water, flash size, message capacity and worst-case bounded work are recorded.
- [ ] Queue exhaustion, invalid capability, stale epoch, restart-budget exhaustion, timer wrap, malformed syscall and nested fault paths fail closed.
- [ ] One service fault cannot corrupt kernel state, silently become healthy or leave stale work executable after recovery.
- [ ] Transactional configuration and signed rollback retain or improve the preserved baseline verdicts.
- [ ] FreeRTOS symbols, scheduler objects and runtime dependencies are absent from the production image and link map.
- [ ] Matched FreeRTOS and Cirvane trials use identical board, clock, service scope, workload, resource ceilings and instrumentation.
- [ ] Any metric that is worse, missing or incomparable remains visible and constrains the release claim.

### Exit gate

Stage 2 passes only when the kernel boots and owns execution on the real ESP32-C5, the recovery transaction invariants pass adversarial tests, the required hardware workflow is supported, FreeRTOS is absent from the runtime kernel boundary, resource use is bounded, and matched evidence supports at least one differentiated result without hiding regressions. Formal proof and independent reproduction are not initial-release blockers, but unsupported proof, safety, hard-isolation and universal-superiority claims remain prohibited.

## Stage 3: complete Cirvane migration and rename

### Objective

Migrate the proven Nucleus service behaviour onto the Cirvane kernel and make Cirvane the consistent current product identity across firmware, source, operator surfaces, documentation and build artefacts while retaining Nucleus only where it truthfully identifies historical evidence.

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
- [ ] Prove that migrated services use Cirvane kernel primitives rather than a FreeRTOS compatibility scheduler.

### Required evidence

- Authoritative local-CI result at the merged rename commit.
- Signed production binary whose metadata and operator-visible identity are Cirvane.
- Real-board functional evidence covering boot, shell, service supervision, configuration, Wi-Fi scan and normal restart behaviour.
- Real-board rollback and security evidence covering rejected corrupt configuration and rejected corrupt update paths.
- Quiet-shell evidence proving periodic telemetry does not obscure `cirvane>`.
- Repository identity-scan result and an inventory of intentionally retained historical Nucleus references.

### Exit gate

Stage 3 passes only when the supported board boots and operates entirely as Cirvane on the clean-sheet kernel, all required real-board evidence is tied to the merged commit, historical evidence remains truthful, and no material runtime or rollback regression is open.

## Stage 4: authenticated OTA transport

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

Stage 4 passes only when every acceptance-matrix case has a deterministic verdict, no rejected path changes the selected boot image, successful and rollback paths pass on the supported board, resource bounds are enforced, and no material security finding remains open.

## Stage 5: initial developer release

### Objective

Freeze one evidence-backed commit as Cirvane's first clean-sheet-kernel developer release without overstating hardware security, energy performance, independent assurance, platform support, formal verification or novelty scope.

### Release-candidate checklist

- [ ] Select the release version and theme using the title format `Cirvane vX.Y.Z: <theme>`.
- [ ] Freeze one release candidate commit containing the qualified clean-sheet kernel, completed migration and rename, authenticated OTA transport, documentation, manifests and release metadata.
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
- Independent penetration testing and external security review.
- Independent novelty challenge and clean-room reproduction.
- Formal verification of the kernel, recovery state machine or hardware model.
- Additional hardware ports, adoption cohorts and practitioner studies.

These deferrals do not waive clean-sheet kernel ownership, internal prior-art diligence, matched baseline evaluation, software signing, authenticated OTA, rollback, secret handling, production-profile or claim-discipline requirements. Their absence prohibits claims of independent validation, formal correctness and definitive worldwide novelty.

### Release evidence bundle

- Release commit and annotated version tag.
- Curated release-notes source and approved rendered previews.
- Signed production artefact digests and reproducible build identity.
- Authoritative local-CI output and real-board evidence bundle.
- OTA adversarial matrix and rollback evidence.
- Kernel dependency inventory, matched baseline results and novelty claim matrix.
- Licence, provenance, limitation and deferred-work notices.
- Owner approval record and verified repository visibility.

### Exit gate

Stage 5 passes only when every non-deferred release requirement is satisfied at the same commit, the published claims match the evidence, the owner explicitly authorises the release and visibility, and the canonical release surface has been inspected after publication.

## Stop-ship conditions

Do not release while any of the following is present:

- A secret, private signing key, credential or sensitive manifest is exposed.
- Firmware, shell, binary or documentation identity is materially ambiguous between Nucleus and Cirvane.
- FreeRTOS or another general-purpose scheduler remains in the production runtime while release copy describes Cirvane as a clean-sheet kernel.
- The recovery transaction invariants are undefined, untested or contradicted by an admitted failure path.
- Any rejected or interrupted OTA path can change the selected boot image.
- Functional boot, health confirmation or rollback is broken on the supported board.
- A production image exposes HIL-only destructive commands.
- Local CI or required real-board evidence is missing for the release commit.
- Licence or provenance is incomplete, or a material same-market name or mark collision remains unresolved for the intended channel.
- Release copy implies production readiness, physical security, energy qualification, independent assurance, formal verification, definitive worldwide novelty or support beyond the tested board.
- Repository visibility differs from the owner's explicit approval.

## Completion record

| Stage | State | Completion evidence |
|---|---|---|
| 1. Kernel novelty and feasibility | In progress | Prior-art matrix, ADR 0003, pre-registered evaluation and host model recorded; real-board spike evidence pending |
| 2. Clean-sheet kernel | Planned | Pending kernel implementation, matched baselines and real-board qualification |
| 3. Cirvane migration and rename | Planned | Pending migrated services, merged rename PR and real-board qualification bundle |
| 4. Authenticated OTA | Planned | Pending merged protocol implementation and adversarial evidence matrix |
| 5. Developer release | Planned | Pending release-candidate evidence, owner approval and verified release |

States are planned, in progress, implemented, verified, deferred or blocked. The completion record advances only after its named evidence exists.
