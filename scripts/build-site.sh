#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output_dir="${1:-$repo_dir/build/site}"

case "$output_dir" in
  "$repo_dir"|"$repo_dir"/) echo "refusing to replace repository root" >&2; exit 2 ;;
esac

rm -rf -- "$output_dir"
mkdir -p "$output_dir/assets"
cp -R "$repo_dir/site/." "$output_dir/"
cp "$repo_dir/assets/brand/source/cirvane-wordmark-reversed.svg" "$output_dir/assets/"
cp "$repo_dir/assets/brand/exports/cirvane-horizontal-reversed-680.png" "$output_dir/assets/"
cp "$repo_dir/assets/brand/exports/cirvane-symbol-512.png" "$output_dir/assets/"
cp "$repo_dir/assets/brand/exports/cirvane-symbol-32.png" "$output_dir/assets/"

python3 "$repo_dir/scripts/validate_site.py" "$output_dir"
echo "Cirvane site built at $output_dir"
