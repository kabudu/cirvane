# Agent Guidelines

Cirvane is a private, evidence-gated embedded operating-system project. Never describe documentation as implementation or strengthen claims beyond recorded evidence.

## Delivery loop

1. Start from a clean, current `master` branch.
2. Select one unchecked roadmap increment and define behavioural acceptance evidence.
3. Create a scoped `codex/<increment>` branch.
4. Update code, tests, evidence, documentation, traceability and risks together.
5. Run `./scripts/ci-local.sh` and perform a Lazarus-mode self-review.
6. Commit only intended files, push, and open a pull request against `master`.
7. Inspect the remote diff, all review feedback and mergeability; resolve every material finding.
8. Rerun `./scripts/ci-local.sh` at the reviewed head and record the result in the PR.
9. Squash-merge, fast-forward local `master`, verify, and delete the merged local branch.

Hosted CI is prohibited while the repository is private. Local CI is authoritative. Do not add GitHub Actions or other hosted workflows unless the owner explicitly approves hosted CI at the documented public-opening gate.

## Product invariants

- Keep runtime memory, queue depth, retries, timeouts and work bounded.
- Fail closed on unsupported, malformed, unsigned, stale or partially verified updates.
- Never select an OTA image until transport, manifest, identity, version and image verification succeed.
- Keep HIL-only destructive commands compile-gated and absent from production images.
- Preserve signed dual-slot rollback and transactional configuration recovery.
- Treat the USB shell as a privileged local interface until authentication is implemented.
- Never commit secrets, signing keys, local SDK state, generated build trees or credentials.
- Do not use the Unicode U+2014 em dash in tracked text or release metadata.

## Validation

Every increment must use `./scripts/ci-local.sh`. Hardware-affecting increments additionally require the relevant real-board harness and committed machine-readable evidence. Public releases require curated release notes, rendered preview review and all stop-ship gates in `docs/RELEASE.md`.
