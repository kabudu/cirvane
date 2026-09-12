# ADR 0006: authenticated OTA uses signed canonical manifests

## Status

Accepted on 2026-09-12 for the initial developer release.

## Context

Cirvane already verifies ECDSA-P256 application signatures and uses two application slots with bootloader rollback. It lacked a remote transport and a signed policy envelope for product identity, target identity, release ordering, image length and image digest. HTTPS alone would make the hosting provider part of the firmware trust root and would not prevent a valid but stale asset from being replayed.

The initial public distribution surface is GitHub Releases. GitHub redirects release downloads to its release-asset domain, so a production client must permit that specific redirect while refusing arbitrary origins and scheme changes.

## Decision

Use the line-oriented `cirvane-update-v1` canonical manifest defined in `docs/OTA_PROTOCOL.md`. Sign its canonical bytes with ECDSA-P256 and SHA-256. Embed only the public verification key in firmware. Use a monotonically increasing release sequence as the freshness and replay boundary because the supported device has no independently trusted real-time clock.

The production client accepts initial URLs only below the Cirvane GitHub Releases path. It permits at most three HTTPS redirects and only to the same release path or GitHub's dedicated release-asset origin. It requires an unambiguous positive `Content-Length`, bounds manifests to 1536 bytes, bounds images to the inactive partition and applies a ten-second network-operation timeout.

The client verifies manifest encoding, product, device, requested version, release sequence, image URL, key identifier and signature before erasing the inactive slot. It streams the image through a fixed HTTP buffer, hashes it while writing, and calls ESP-IDF image verification. It persists the highest accepted sequence before changing the selected boot target. Any failure before the final boot-selection call leaves the current boot target unchanged.

The initial key identifier is `release-2026-01`. The owner holds its private key outside the repository with mode `0600`. The firmware contains its uncompressed public point. A rotation release must include both the old active public key and the new active public key and be signed by the old key. A later release may mark the old identifier revoked. Devices do not accept remotely supplied trust keys or revocation state.

## Consequences

GitHub may distribute bytes but cannot authorize firmware without the owner-held key. A compromised signing key remains a critical incident and requires a recovery image or a rotation release trusted by an uncompromised embedded key. Losing the only private key prevents future remote updates for devices that trust only that key.

Release order is global for the supported product and device pair. A sequence is consumed before boot selection, so a storage-success and selection-failure edge case can require a later sequence rather than retrying the same release. This fails closed at the cost of availability.

The release process must keep the manifest key and application-signing key under owner control, increment `CONFIG_CIRVANE_RELEASE_SEQUENCE`, build locally, publish immutable artifacts and verify the public asset digests. GitHub Actions does not receive either private key.
