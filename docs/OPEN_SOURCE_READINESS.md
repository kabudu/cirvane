# Open-source readiness

## Scope

This gate prepares the source repository for public visibility. It does not create a versioned developer release, publish binaries, enable hardware production security or claim that the clean-sheet kernel research is the current product image.

## Engineering and repository evidence

| Gate | Evidence | State |
|---|---|---|
| Licence consistency | Root MIT licence, MIT project SPDX identifiers, `THIRD_PARTY_NOTICES.md`, brand manifest and Figtree OFL | Ready |
| Contribution governance | `CONTRIBUTING.md`, DCO sign-off, `CODE_OF_CONDUCT.md`, `GOVERNANCE.md`, templates and CODEOWNERS | Ready |
| Security disclosure | `SECURITY.md` and private-reporting link | Prepared; GitHub private vulnerability reporting must be enabled immediately after the repository becomes public |
| Support boundary | `SUPPORT.md`, README status and named ESP32-C5 support | Ready |
| Secret hygiene | Gitleaks 8.30.1 scanned all 35 commits on the readiness branch on 2026-08-30 with zero findings; tracked-file audit found no key, credential, SDK state or build tree | Ready on the reviewed branch; rerun on the exact history selected for publication |
| Evidence privacy | Current hardware evidence is deterministically redacted and validated; earlier commits contain local network and device identifiers | Blocked until a clean public snapshot is created or the existing history is rewritten with explicit owner approval |
| Source provenance | Single repository author identity; Espressif console source retains `Unlicense OR CC0-1.0`; brand source and Figtree provenance recorded | Ready, subject to the professional AI-assisted asset rights review below |
| Validation | `./scripts/ci-local.sh` remains authoritative and hosted CI remains disabled | Must pass at the final reviewed commit |
| Public claims | README separates the ESP-IDF/FreeRTOS product firmware from kernel research and lists security limitations | Ready except for the owner decision on the unqualified word `Secure` in the GitHub About text |

## Name and mark audit

The 2026-08-30 refresh covered general web search, GitHub repository and user search, npm, PyPI, crates.io where accessible, and RDAP for common domains. It found this repository as the only exact GitHub repository name and no npm or PyPI package. Automated crates.io access was inconclusive. Exact-name web results included unrelated lighting and appliance products, personal-name uses and an unrelated trading company; no same-market embedded-firmware result was found.

This search is not trademark clearance and does not reserve a name. Professional UK, EU and US name or mark review remains unresolved. AI-assisted brand-source rights review is also unresolved. The owner must either obtain those reviews or explicitly accept and record the residual legal risk before changing repository visibility.

## Public-opening procedure

1. Choose and approve a history-safe publication path: create a clean squashed public snapshot while retaining the private repository as an archive, or explicitly authorize a destructive rewrite of the existing repository history and remote references.
2. Freeze a clean reviewed commit and rerun `./scripts/ci-local.sh`, Gitleaks history scanning and `scripts/validate_public_readiness.py`.
3. Review the GitHub About wording against `docs/BRAND_IDENTITY.md`; prefer capability-focused language over an unqualified assurance claim.
4. Obtain or explicitly accept the remaining name, mark and AI-assisted asset rights risks.
5. Obtain explicit owner approval for the exact public commit and visibility change.
6. Publish only the approved history, without publishing a tag, binary or GitHub Release.
7. Immediately enable GitHub private vulnerability reporting and verify the Security tab presents a private report form.
8. Verify the public README, licence, contribution guide, security policy, issue forms, pull-request template, citation metadata, About text and default branch from a signed-out view.
9. Decide separately whether to authorize hosted CI. Until then, local CI remains authoritative and no workflow should be added.

## Gates that do not block source visibility

Authenticated remote OTA, hardware Secure Boot, flash encryption, eFuse provisioning, energy qualification, safety certification, independent penetration testing, clean-room reproduction, formal verification and a versioned developer release remain incomplete. They block stronger deployment or release claims, not an accurately described public source repository.
