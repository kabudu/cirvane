# Release policy

## Private repository policy

`./scripts/ci-local.sh` is authoritative while this repository is private. Hosted CI is disabled and must not be added or described as passing. Hosted CI may be considered only at the public-opening gate after explicit user approval.

## Public source-opening gate

Public source access is separate from a versioned developer release. It may expose the pre-release source, documentation and evidence without publishing binaries or strengthening deployment claims.

Before changing visibility:

- Make the root licence, project SPDX identifiers, README and third-party notices consistent.
- Publish contribution, conduct, governance, support, security-disclosure, issue and pull-request guidance.
- Run `./scripts/ci-local.sh`, `scripts/validate_public_readiness.py` and a full-history Gitleaks scan at the exact reviewed commit.
- Confirm no key, credential, local SDK state, generated build tree, private prompt or sensitive device/network identifier is tracked or present in history.
- Review public claims, name or mark results, dependency provenance and AI-assisted brand rights. Record professional review or explicit owner risk acceptance where independent legal review is unavailable.
- Obtain explicit owner approval for the exact commit and visibility change.
- Keep hosted CI disabled unless the owner separately authorizes it. Public visibility is not implicit hosted-CI approval.

Immediately after a public visibility change, enable and verify GitHub private vulnerability reporting, then inspect the repository while signed out. Do not create a tag, GitHub Release or binary merely because the source becomes public.

## Initial developer-release gates

- Consistent Cirvane firmware, shell, documentation and asset identity.
- Honest substrate claim: the first labelled firmware is powered by ESP-IDF and FreeRTOS (ADR 0005). A clean-sheet kernel claim is a later kernel-release gate (ADR 0002) and a stop-ship if used for this image.
- Internally challenged recovery-transaction hypothesis retained as research; matched FreeRTOS baseline evidence with regressions retained.
- Passing deterministic local CI at the release commit.
- Signed production build and real-board functional, security and rollback evidence.
- Authenticated OTA transport with fail-closed adversarial acceptance matrix.
- Complete enduring brand assets, provenance, licences, accessibility and deterministic exports.
- Curated release notes with a release title and rendered preview at desktop and narrow widths.
- Explicit limitation notice: energy unmeasured, hardware Secure Boot/flash encryption/eFuse provisioning deferred, independent penetration testing deferred.
- Explicit limitation notice: independent novelty challenge, clean-room reproduction and formal verification deferred.
- Owner approval for the exact release and any visibility change.

Stop ship on secret exposure, ambiguous build identity, a clean-sheet kernel claim for the ESP-IDF/FreeRTOS firmware, hidden FreeRTOS runtime beneath a later kernel-labelled claim, violated recovery-transaction invariants, selected-boot change after any rejected update, broken rollback, missing local-CI evidence, incomplete licence decision, unresolved material name collision, misleading novelty or assurance claims, or accidental public visibility.

## Presentation contract

The public title format is `Cirvane vX.Y.Z: <theme>`. Curated notes open with one outcome paragraph, contain three to five material changes, state claim and compatibility boundaries, provide one primary install path and link detailed evidence. Each paragraph and list item occupies one physical source line; do not hard-wrap release notes. Release automation must fail closed when curated title or body is absent or mismatched to the tag. The rendered canonical release page must be inspected at desktop and narrow widths after publication.

`CHANGELOG.md` remains detailed package history and is linked rather than pasted into release notes.
