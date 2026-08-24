# Risk register

| ID | Category | Risk | Probability | Impact | Owner | Trigger | Mitigation and contingency |
|---|---|---|---:|---:|---|---|---|
| R1 | Security | Physical attacker bypasses software-only verification | High | High | Security owner | Device leaves controlled development use | Keep developer-only status; provision separate production hardware later |
| R2 | Security | OTA transport accepts stale, partial or mismatched content | Medium | Critical | Engineering owner | Any negative OTA case selects boot target | Fail-closed manifest protocol and adversarial HIL stop-ship gate |
| R3 | Reliability | Single board failure blocks reproduction | Medium | High | Product owner | Board becomes unavailable | Preserve host tests and artefacts; acquire second board before production claims |
| R4 | Performance | Rename or OTA work regresses boot/latency | Medium | Medium | Engineering owner | Controlled result exceeds recorded envelope | Matched baseline rerun and rollback increment |
| R5 | Legal/name | Cirvane collides with a protected mark | Low/unknown | High | Product/legal owner | Legal search finds same-market similarity | Stop public launch and use audited fallback before public exposure |
| R6 | Brand | Visual identity implies assurance not established | Medium | Medium | Brand owner | Comprehension review reads mark/copy as safety guarantee | Reject direction and enforce prohibited-claim scan |
| R7 | Release | Energy performance is inferred without measurement | Medium | Medium | Release owner | Public copy mentions efficiency | Keep energy explicitly deferred and scan release copy |
| R8 | Security | Signing key leaks through source or evidence | Low | Critical | Security owner | Secret scan or remote history match | Stop release, rotate key, purge affected unpublished history |
| R9 | Architecture | Vendor Wi-Fi or radio components require FreeRTOS scheduling | High/unknown | Critical | Kernel owner | Minimal kernel spike cannot operate required radio workflow without FreeRTOS | Isolate a genuinely non-kernel adapter, explicitly narrow the first release or stop the kernel release path |
| R10 | Correctness | Clean-sheet kernel introduces trap, interrupt, memory or recovery corruption | High | Critical | Kernel and security owners | Adversarial model or HIL finds state corruption, stale work or silent health | Keep kernel minimal, bound unsafe code, model recovery transitions and stop release until deterministic real-board evidence passes |
| R11 | Research | Recovery transaction is anticipated by prior art or fails to differentiate | Medium/unknown | High | Research owner | Close mechanism or matched result falsifies NOV-01 or NOV-02 | Narrow or retract novelty wording while retaining useful engineering results; do not rename familiar mechanisms |
| R12 | Schedule | Kernel work delays the initial developer release materially | High | Medium | Product owner | Feasibility or driver milestone exceeds its pre-registered stop rule | Use staged stop rules, preserve the working baseline and require explicit owner decision before widening scope |

Risks are reviewed at every milestone exit and release candidate.
