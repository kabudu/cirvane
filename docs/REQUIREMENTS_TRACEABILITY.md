# Requirements traceability

| ID | Requirement | Design owner | Primary verification | Release evidence | State |
|---|---|---|---|---|---|
| REL-01 | Bounded services and messages | Service manager and bus | Contract plus functional HIL | Functional JSON | Verified baseline |
| SEC-01 | Transactional configuration recovery | Configuration journal | Corruption HIL | Security HIL JSON | Verified Nucleus baseline; kernel journal host-tested and flash-backed on spike at `0x7FE000` |
| SEC-02 | Signed image rejection and rollback | OTA policy | Host crypto plus rollback HIL | Security/rollback JSON | Verified Nucleus baseline; kernel policy host-tested; ESP verify adapter not wired |
| UX-01 | Interactive prompt remains readable | Logging and shell | Quiet-interval hardware test | PR #2 evidence | Verified baseline |
| REL-03 | Consistent Cirvane identity | Product and brand | Current-product identity scan plus board capture | `cirvane.bin`, `cirvane>` prompt, `cirvane-identity-hil.json` | Verified for current-product surfaces and identity boot; rollback/security transcripts remain historical Nucleus |
| SEC-03 | Authenticated OTA transport | OTA adapter and policy | Adversarial public-workflow E2E | OTA evidence matrix | Planned |
| REL-02 | Explicit developer-release limitations | Release owner | Claim/deferred-state scan | Curated release notes | Planned |
| PERF-01 | Preserve performance envelope | Runtime owners | Matched controlled measurement | New summary JSON | Planned |
| NOV-01 | Bounded recovery transaction differs from reviewed kernel mechanisms | Research and kernel owners | Systematic matrix plus matched prototype | Novelty claim matrix | Candidate |
| SEM-01 | Recovery atomically invalidates stale epoch-owned work and emits one bounded outcome | Kernel | Model tests plus adversarial HIL | Kernel qualification bundle | Implemented as kernel syscalls; spike HIL demonstrates one transaction; adversarial matrix pending |
| REL-04 | Kernel-labelled image owns scheduling without FreeRTOS runtime | Kernel and build | Link-map, symbol and runtime inspection | Dependency inventory | Spike board evidence recorded; not required of the first Cirvane firmware (ADR 0005) |
| HAL-01 | Bounded UART, GPIO, timer, flash-read/erase/write, watchdog and entropy without radio | Kernel HAL | Host refuse tests plus spike HIL | kernel-spike.json | Implemented on spike; erase/write limited to config window; radio excluded (R9) |
| REL-05 | Kernel HIL diagnostics compile-gated out of production profile | Spike build | Dual-profile ci-local strings/nm | HIL kernel-spike.json; production strings check | Compile profiles implemented; production image is idle boot plus quiet shell, not a product |
| UX-02 | Stable crash/evidence lines and quiet kernel shell | Kernel obs | Host refuse tests plus production HIL | kernel-shell.json | Implemented on production spike; unauthenticated USB |

States are implemented, verified, candidate, planned or deferred. Documentation alone cannot advance a behavioural requirement to implemented or verified.
