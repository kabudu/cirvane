#!/usr/bin/env python3
"""Test manifest signing without retaining private test material."""

from pathlib import Path
import importlib.util
import tempfile
import unittest

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "sign_ota_manifest", ROOT / "scripts/sign_ota_manifest.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class OtaManifestTests(unittest.TestCase):
    def test_canonical_manifest_signature(self):
        key = ec.generate_private_key(ec.SECP256R1())
        with tempfile.TemporaryDirectory(prefix="cirvane-manifest-") as directory:
            image = Path(directory) / "image.bin"
            image.write_bytes(b"synthetic firmware fixture")
            manifest = MODULE.create_manifest("v1.2.3", 7, image, key)
        canonical, signature_line = manifest.rsplit(b"signature=", 1)
        raw = bytes.fromhex(signature_line.strip().decode("ascii"))
        der = encode_dss_signature(
            int.from_bytes(raw[:32], "big"), int.from_bytes(raw[32:], "big")
        )
        key.public_key().verify(der, canonical, ec.ECDSA(hashes.SHA256()))
        self.assertIn(b"sequence=7\n", canonical)
        self.assertIn(b"image_size=26\n", canonical)
        self.assertTrue(manifest.endswith(b"\n"))

    def test_rejects_noncanonical_versions(self):
        key = ec.generate_private_key(ec.SECP256R1())
        with tempfile.TemporaryDirectory(prefix="cirvane-manifest-") as directory:
            image = Path(directory) / "image.bin"
            image.write_bytes(b"x")
            for version in ("1.2.3", "v01.2.3", "v1.2", "v1.2.3-rc1"):
                with self.assertRaises(SystemExit):
                    MODULE.create_manifest(version, 1, image, key)


if __name__ == "__main__":
    unittest.main(verbosity=2)
