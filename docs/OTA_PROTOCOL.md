# Authenticated OTA protocol

## Operator workflow

Connect Cirvane to Wi-Fi, then request an exact release:

```text
wifi connect
ota-update v0.1.0
restart
ota-status
ota-confirm
```

`ota-update` never accepts a password, token or arbitrary URL. It constructs the public manifest URL for the requested Cirvane release. A successful command means every transport, manifest, signature and image check passed and the inactive image is selected for the next boot. It does not reboot without the operator's separate `restart` command.

The new image enters ESP-IDF's pending-verification state. `ota-confirm` commits a healthy image. Restarting again without confirmation causes bootloader rollback to the previously verified image.

## Canonical manifest v1

The manifest is ASCII, uses LF line endings, ends with one LF and contains fields in this exact order:

```text
cirvane-update-v1
product=cirvane
device=seeed-xiao-esp32c5
version=v0.1.0
sequence=1
image_size=1314816
image_sha256=<64 lowercase hexadecimal characters>
image_url=https://github.com/kabudu/cirvane/releases/download/v0.1.0/cirvane-v0.1.0-esp32c5.bin
key_id=release-2026-01
signature=<128 lowercase hexadecimal characters>
```

The example sequence above is illustrative; a real manifest contains only decimal digits. Blank lines, CRLF, duplicate fields, reordered fields, unknown fields, leading-zero integers, NUL bytes and trailing content are rejected. The signature is the raw 32-byte `r` value followed by the raw 32-byte `s` value from ECDSA-P256 over the SHA-256 digest of every byte preceding the `signature=` line.

## Verification order

1. Require a canonical `vMAJOR.MINOR.PATCH` request and construct its fixed GitHub Releases manifest URL.
2. Verify TLS through the ESP-IDF certificate bundle, with a ten-second operation timeout and no credentials.
3. Require status 200, a positive unambiguous `Content-Length`, no more than three redirects and no body larger than 1536 bytes.
4. Parse the exact manifest grammar and require the Cirvane product and Seeed Studio XIAO ESP32-C5 target identities.
5. Require the manifest version to equal the requested version and the image URL to equal the canonical asset URL for that version.
6. Require the release sequence to exceed both the sequence compiled into the running image and the highest sequence accepted in NVS.
7. Require an active embedded key identifier and verify the manifest signature.
8. Open only the inactive application slot. Require the declared image length to fit it and the HTTP length to equal the declared length.
9. Stream through a 2048-byte HTTP buffer while hashing and writing. Reject timeout, disconnect, overflow, short body, long body or flash error.
10. Require the computed SHA-256 digest to equal the manifest and require `esp_ota_end` to accept the signed application image.
11. Persist the new highest sequence. Only then select the inactive slot for the next boot.

No rejected path calls `esp_ota_set_boot_partition`. A partially written inactive slot is not selected and may be overwritten by a later, higher-sequence update.

## Trust and key lifecycle

The initial trust anchor is `release-2026-01`. Its private key is generated and retained outside Git and GitHub. `scripts/sign_ota_manifest.py` refuses keys with the wrong public point, owner or filesystem mode. `scripts/build-release.sh` copies the key into an ignored build directory only for the build and removes that copy on exit.

To rotate a healthy key, first ship a release signed by the current key that embeds the new public key as active. After that release is deployed, a later release may be signed by the new key and mark the old key revoked. A key identifier absent from the compiled table is rejected. Revocation data is never accepted from an unsigned or merely TLS-authenticated response.

If the private key is suspected compromised, stop releases, remove affected assets, publish a security advisory and assess whether a still-trusted embedded key can authorize recovery. If no uncompromised embedded key remains, affected devices require a trusted local recovery flash.

## Hosting boundary

GitHub Releases is the initial hosting service, not the authorization root. The manifest signature and application signature remain decisive if hosting is compromised. The v1 production allowlist accepts `github.com/kabudu/cirvane/releases/download/` as the initial origin and `release-assets.githubusercontent.com` only as a redirect destination.

A future owner-controlled CDN can be introduced only through a firmware release that changes the compiled allowlist. The protocol and signatures do not otherwise depend on GitHub.
