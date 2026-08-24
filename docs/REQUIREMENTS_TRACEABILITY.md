# Requirements traceability

| ID | Requirement | Design owner | Primary verification | Release evidence | State |
|---|---|---|---|---|---|
| REL-01 | Bounded services and messages | Service manager and bus | Contract plus functional HIL | Functional JSON | Verified baseline |
| SEC-01 | Transactional configuration recovery | Configuration journal | Corruption HIL | Security HIL JSON | Verified baseline |
| SEC-02 | Signed image rejection and rollback | OTA policy | Host crypto plus rollback HIL | Security/rollback JSON | Verified baseline |
| UX-01 | Interactive prompt remains readable | Logging and shell | Quiet-interval hardware test | PR #2 evidence | Verified baseline |
| REL-03 | Consistent Cirvane identity | Product and brand | Repository-wide identity scan | Brand manifest and release preview | Planned |
| SEC-03 | Authenticated OTA transport | OTA adapter and policy | Adversarial public-workflow E2E | OTA evidence matrix | Planned |
| REL-02 | Explicit developer-release limitations | Release owner | Claim/deferred-state scan | Curated release notes | Planned |
| PERF-01 | Preserve performance envelope | Runtime owners | Matched controlled measurement | New summary JSON | Planned |

States are implemented, verified, candidate, planned or deferred. Documentation alone cannot advance a behavioural requirement to implemented or verified.
