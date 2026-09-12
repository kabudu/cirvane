#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="${1:-}"
sequence="${2:-}"
key_path="${CIRVANE_RELEASE_SIGNING_KEY:-}"
build_dir="$repo_dir/build/release"
artifact_dir="$repo_dir/build/release-artifacts"

if [[ ! "$version" =~ ^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$ ]]; then
  echo "usage: CIRVANE_RELEASE_SIGNING_KEY=/secure/key.pem $0 vMAJOR.MINOR.PATCH SEQUENCE" >&2
  exit 2
fi
if [[ ! "$sequence" =~ ^[1-9][0-9]*$ ]] || (( sequence > 2147483647 )); then
  echo "release sequence must be between 1 and 2147483647" >&2
  exit 2
fi
if [[ -z "$key_path" || ! -f "$key_path" ]]; then
  echo "CIRVANE_RELEASE_SIGNING_KEY must identify the owner-held P-256 key" >&2
  exit 2
fi
if [[ -n "$(git -C "$repo_dir" status --porcelain)" ]]; then
  echo "release builds require a clean worktree" >&2
  exit 1
fi
if [[ "$(git -C "$repo_dir" branch --show-current)" != "master" ]]; then
  echo "release builds require the master branch" >&2
  exit 1
fi
configured_sequence="$(sed -n 's/^CONFIG_CIRVANE_RELEASE_SEQUENCE=//p' "$repo_dir/sdkconfig.release.defaults")"
if [[ "$configured_sequence" != "$sequence" ]]; then
  echo "sdkconfig.release.defaults sequence is $configured_sequence, expected $sequence" >&2
  exit 1
fi
if [[ -z "${IDF_PATH:-}" || ! -f "$IDF_PATH/tools/cmake/project.cmake" ]]; then
  echo "source the pinned ESP-IDF environment before building a release" >&2
  exit 1
fi

mkdir -p "$build_dir"
cp "$key_path" "$build_dir/signing-key.pem"
chmod 600 "$build_dir/signing-key.pem"
cleanup() { rm -f "$build_dir/signing-key.pem"; }
trap cleanup EXIT INT TERM

idf.py -B "$build_dir" \
  -D SDKCONFIG="$build_dir/sdkconfig" \
  -D "PROJECT_VER=${version#v}" \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.release.defaults' build

rm -rf -- "$artifact_dir"
mkdir -p "$artifact_dir"
ota_name="cirvane-$version-esp32c5.bin"
full_name="cirvane-$version-xiao-esp32c5-full.bin"
manifest_name="cirvane-$version.manifest"
cp "$build_dir/cirvane.bin" "$artifact_dir/$ota_name"
python -m esptool --chip esp32c5 merge-bin \
  --flash-mode dio --flash-size 8MB --flash-freq 80m \
  --output "$artifact_dir/$full_name" \
  0x2000 "$build_dir/bootloader/bootloader.bin" \
  0x8000 "$build_dir/partition_table/partition-table.bin" \
  0xf000 "$build_dir/ota_data_initial.bin" \
  0x20000 "$build_dir/cirvane.bin"
python3 "$repo_dir/scripts/sign_ota_manifest.py" \
  --version "$version" --sequence "$sequence" \
  --image "$artifact_dir/$ota_name" --key "$key_path" \
  --output "$artifact_dir/$manifest_name"
(cd "$artifact_dir" && shasum -a 256 "$ota_name" "$full_name" "$manifest_name" > SHA256SUMS)

python3 - "$artifact_dir/release-metadata.json" "$version" "$sequence" \
  "$(git -C "$repo_dir" rev-parse HEAD)" "$IDF_PATH" <<'PY'
import json
from pathlib import Path
import subprocess
import sys

output, version, sequence, commit, idf_path = sys.argv[1:]
idf_version = subprocess.run(
    ["git", "-C", idf_path, "describe", "--tags", "--always", "--dirty"],
    check=True, capture_output=True, text=True,
).stdout.strip()
Path(output).write_text(json.dumps({
    "schema": 1,
    "version": version,
    "release_sequence": int(sequence),
    "commit": commit,
    "esp_idf": idf_version,
    "target": "Seeed Studio XIAO ESP32-C5",
    "application_signing": "ECDSA-P256, software verification",
    "manifest_signing_key_id": "release-2026-01",
}, indent=2) + "\n", encoding="utf-8")
PY

echo "release artifacts built at $artifact_dir"
