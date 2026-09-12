#!/usr/bin/env python3
"""Create Cirvane's canonical OTA manifest and P-256 signature."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import stat

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

RELEASE_PREFIX = "https://github.com/kabudu/cirvane/releases/download/"
EXPECTED_PUBLIC_POINT = bytes.fromhex(
    "0485257ce0676cceef8cb84209a1f7b1acbae0e0a4abc4313c588f9ecf54ca0f"
    "fb71c3211cefbfc38c540d99cf9a5f2e03f3ddace84870b5b2f41b5c66707a7f5e"
)


def load_private_key(path: Path) -> ec.EllipticCurvePrivateKey:
    info = path.stat()
    if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o600:
        raise SystemExit("release key must be owned by the current user with mode 0600")
    key = serialization.load_pem_private_key(path.read_bytes(), password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(
        key.curve, ec.SECP256R1  # gitleaks:allow - API type check, not a credential
    ):
        raise SystemExit("release key must be an unencrypted P-256 private key")
    point = key.public_key().public_bytes(
        serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint
    )
    if point != EXPECTED_PUBLIC_POINT:
        raise SystemExit("release key does not match the public key embedded in Cirvane")
    return key


def create_manifest(version: str, sequence: int, image: Path,
                    key: ec.EllipticCurvePrivateKey) -> bytes:
    if not re.fullmatch(r"v(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version):
        raise SystemExit("version must be a canonical vMAJOR.MINOR.PATCH value")
    if sequence < 1 or sequence > 2_147_483_647:
        raise SystemExit("sequence must be between 1 and 2147483647")
    data = image.read_bytes()
    if not data:
        raise SystemExit("image is empty")
    image_name = f"cirvane-{version}-esp32c5.bin"
    canonical = (
        "cirvane-update-v1\n"
        "product=cirvane\n"
        "device=seeed-xiao-esp32c5\n"
        f"version={version}\n"
        f"sequence={sequence}\n"
        f"image_size={len(data)}\n"
        f"image_sha256={hashlib.sha256(data).hexdigest()}\n"
        f"image_url={RELEASE_PREFIX}{version}/{image_name}\n"
        "key_id=release-2026-01\n"
    ).encode("ascii")
    der = key.sign(canonical, ec.ECDSA(hashes.SHA256()))
    r, s = decode_dss_signature(der)
    signature = r.to_bytes(32, "big") + s.to_bytes(32, "big")
    return canonical + f"signature={signature.hex()}\n".encode("ascii")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--sequence", required=True, type=int)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--key", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    key = load_private_key(args.key)
    manifest = create_manifest(args.version, args.sequence, args.image, key)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(manifest)
    print(f"wrote signed OTA manifest: {args.output}")


if __name__ == "__main__":
    main()
