# Requirements traceability

| ID | Requirement | Design owner | Primary verification | Release evidence | State |
|---|---|---|---|---|---|
| REL-01 | Bounded services and messages | Service manager and bus | Contract plus functional HIL | Functional JSON | Verified baseline |
| SEC-01 | Transactional configuration recovery | Configuration journal | Corruption HIL | Security HIL JSON | Verified Nucleus baseline; kernel journal host-tested and flash-backed on spike at `0x7FE000` |
| SEC-02 | Signed image rejection and rollback | OTA policy | Host crypto plus rollback HIL | Security/rollback JSON; kernel-spike.json otadata | Verified Nucleus baseline; Cirvane fail-closed select plus live otadata restore; not signed-image-equal |
| UX-01 | Interactive prompt remains readable | Logging and shell | Quiet-interval hardware test | PR #2 evidence | Verified baseline |
| REL-03 | Consistent Cirvane identity | Product and brand | Current-product identity scan plus board capture | `cirvane.bin`, `cirvane>` prompt, `cirvane-identity-hil.json` | Verified for current-product surfaces and identity boot; rollback/security transcripts remain historical Nucleus |
| SEC-03 | Authenticated OTA transport | OTA adapter and policy | Adversarial public-workflow E2E | OTA evidence matrix | Planned |
| REL-02 | Explicit developer-release limitations | Release owner | Claim/deferred-state scan | Curated release notes | Planned |
| PERF-01 | Preserve performance envelope | Runtime owners | Matched controlled measurement | matched-eval.json | Verified class-1 pair; classes 2-5, tails and Wi-Fi incomparable |
| NOV-01 | Bounded recovery transaction differs from reviewed kernel mechanisms | Research and kernel owners | Systematic matrix plus matched prototype | Novelty claim matrix | Candidate |
| SEM-01 | Recovery atomically invalidates stale epoch-owned work and emits one bounded outcome | Kernel | Model tests plus adversarial HIL | kernel-spike.json adv markers | Implemented; HIL adversarial line pass |
| REL-04 | Kernel-labelled image owns scheduling without FreeRTOS runtime | Kernel and build | Link-map, symbol and runtime inspection | kernel-spike.json `freertos=absent` | Spike verified; not required of the first Cirvane firmware (ADR 0005) |
| HAL-01 | Bounded UART, GPIO, timer, flash-read/erase/write, watchdog and entropy without radio | Kernel HAL | Host refuse tests plus spike HIL | kernel-spike.json | Verified on spike; erase/write limited to config and otadata windows; radio excluded (ADR 0004) |
| REL-05 | Kernel HIL diagnostics compile-gated out of production profile | Spike build | Dual-profile ci-local strings/nm | HIL kernel-spike.json; production strings check | Compile profiles implemented; eval profile is the matched kernel image |
| UX-02 | Stable crash/evidence lines and quiet kernel shell | Kernel obs | Host refuse tests plus production HIL | kernel-shell.json | Implemented on production spike; unauthenticated USB |

States are implemented, verified, candidate, planned or deferred. Documentation alone cannot advance a behavioural requirement to implemented or verified.
