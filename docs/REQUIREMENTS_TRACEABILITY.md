# Requirements traceability

| ID | Requirement | Design owner | Primary verification | Release evidence | State |
|---|---|---|---|---|---|
| REL-01 | Bounded services and messages | Service manager and bus | Contract plus functional HIL | `cirvane-functional-hil.json` | Verified Cirvane firmware |
| SEC-01 | Transactional configuration recovery | Configuration journal | Corruption HIL | `cirvane-security-hil.json` | Verified Cirvane firmware; kernel journal separately host-tested and flash-backed on spike at `0x7FE000` |
| SEC-02 | Signed image rejection and rollback | OTA policy | Host crypto plus rollback HIL | `cirvane-security-host.json`, `cirvane-security-hil.json`, `cirvane-ota-rollback-hil.json` | Verified Cirvane firmware; authenticated remote transport remains SEC-03 |
| UX-01 | Interactive prompt remains readable | Logging and shell | Quiet-interval hardware test | PR #2 evidence | Verified baseline |
| REL-03 | Consistent Cirvane identity | Product and brand | Current-product identity scan plus board capture | `cirvane.bin`, `cirvane>` prompt, `cirvane-functional-hil.json` | Verified across current firmware, functional, security and rollback evidence; historical Nucleus records retained |
| SEC-03 | Authenticated OTA transport | OTA adapter and policy | Adversarial public-workflow E2E | OTA evidence matrix | Planned |
| REL-02 | Explicit developer-release limitations | Release owner | Claim/deferred-state scan | Curated release notes | Planned |
| PERF-01 | Preserve performance envelope | Runtime owners | Controlled shell and restart measurement | `cirvane-stage3-performance.json`, `matched-eval.json` | Verified for renamed ESP-IDF/FreeRTOS firmware; kernel classes 2-5, units and Wi-Fi remain incomparable |
| NOV-01 | Bounded recovery transaction differs from reviewed kernel mechanisms | Research and kernel owners | Systematic matrix plus matched prototype | Novelty claim matrix | Candidate |
| SEM-01 | Recovery atomically invalidates stale epoch-owned work and emits one bounded outcome | Kernel | Model tests plus adversarial HIL | kernel-spike.json adv markers | Implemented; HIL adversarial line pass |
| REL-04 | Kernel-labelled image owns scheduling without FreeRTOS runtime | Kernel and build | Link-map, symbol and runtime inspection | kernel-spike.json `freertos=absent` | Spike verified; not required of the first Cirvane firmware (ADR 0005) |
| HAL-01 | Bounded UART, GPIO, timer, flash-read/erase/write, watchdog and entropy without radio | Kernel HAL | Host refuse tests plus spike HIL | kernel-spike.json | Verified on spike; erase/write limited to config and otadata windows; radio excluded (ADR 0004) |
| REL-05 | Kernel HIL diagnostics compile-gated out of production profile | Spike build | Dual-profile ci-local strings/nm | HIL kernel-spike.json; production strings check | Compile profiles implemented; eval profile is the matched kernel image |
| UX-02 | Stable crash/evidence lines and quiet kernel shell | Kernel obs | Host refuse tests plus production HIL | kernel-shell.json | Implemented on production spike; unauthenticated USB |
| NET-01 | Secure, bounded and usable Wi-Fi station connectivity | Product Wi-Fi service | Host policy tests plus connect/disconnect/reboot HIL | Wi-Fi HIL JSON | Implemented; real-board evidence pending |

States are implemented, verified, candidate, planned or deferred. Documentation alone cannot advance a behavioural requirement to implemented or verified.
