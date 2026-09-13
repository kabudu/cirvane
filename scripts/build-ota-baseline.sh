#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
key_path="${CIRVANE_RELEASE_SIGNING_KEY:-}"
build_dir="$repo_dir/build/ota-baseline"

if [[ -z "$key_path" || ! -f "$key_path" ]]; then
  echo "CIRVANE_RELEASE_SIGNING_KEY must identify the owner-held P-256 key" >&2
  exit 2
fi
if [[ -z "${IDF_PATH:-}" || ! -f "$IDF_PATH/tools/cmake/project.cmake" ]]; then
  echo "source the pinned ESP-IDF environment before building" >&2
  exit 1
fi

mkdir -p "$build_dir"
cp "$key_path" "$build_dir/signing-key.pem"
chmod 600 "$build_dir/signing-key.pem"
cleanup() { rm -f "$build_dir/signing-key.pem"; }
trap cleanup EXIT INT TERM

idf.py -B "$build_dir" \
  -D SDKCONFIG="$build_dir/sdkconfig" \
  -D PROJECT_VER=ota-baseline \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ota-baseline.defaults' build

echo "OTA qualification baseline built at $build_dir"
