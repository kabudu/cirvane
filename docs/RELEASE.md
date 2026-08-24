# Release policy

## Private repository policy

`./scripts/ci-local.sh` is authoritative while this repository is private. Hosted CI is disabled and must not be added or described as passing. Hosted CI may be considered only at the public-opening gate after explicit user approval.

## Initial developer-release gates

- Consistent Cirvane firmware, shell, documentation and asset identity.
- Clean-sheet Cirvane kernel ownership with no hidden FreeRTOS runtime and an enumerated vendor dependency boundary.
- Internally challenged recovery-transaction hypothesis and matched FreeRTOS baseline evidence with regressions retained.
- Passing deterministic local CI at the release commit.
- Signed production build and real-board functional, security and rollback evidence.
- Authenticated OTA transport with fail-closed adversarial acceptance matrix.
- Complete enduring brand assets, provenance, licences, accessibility and deterministic exports.
- Curated release notes with a release title and rendered preview at desktop and narrow widths.
- Explicit limitation notice: energy unmeasured, hardware Secure Boot/flash encryption/eFuse provisioning deferred, independent penetration testing deferred.
- Explicit limitation notice: independent novelty challenge, clean-room reproduction and formal verification deferred.
- Owner approval for the exact release and any visibility change.

Stop ship on secret exposure, ambiguous build identity, hidden FreeRTOS runtime beneath a clean-sheet claim, violated recovery-transaction invariants, selected-boot change after any rejected update, broken rollback, missing local-CI evidence, incomplete licence decision, unresolved material name collision, misleading novelty or assurance claims, or accidental public visibility.

## Presentation contract

The public title format is `Cirvane vX.Y.Z: <theme>`. Curated notes open with one outcome paragraph, contain three to five material changes, state claim and compatibility boundaries, provide one primary install path and link detailed evidence. Each paragraph and list item occupies one physical source line; do not hard-wrap release notes. Release automation must fail closed when curated title or body is absent or mismatched to the tag. The rendered canonical release page must be inspected at desktop and narrow widths after publication.

`CHANGELOG.md` remains detailed package history and is linked rather than pasted into release notes.
