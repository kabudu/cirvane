#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$repo_root/assets/brand/source"
master="$source_dir/cirvane-symbol-dimensional.png"
destination="${1:-$source_dir/cirvane-symbol-mono-dimensional.png}"

if ! command -v magick >/dev/null 2>&1; then
  echo "ImageMagick is required to generate brand derivatives" >&2
  exit 1
fi

work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

magick "$master" -alpha extract "$work_dir/master-alpha.png"
magick "$master" -alpha off -fx 'g>r*1.08&&b>r*1.05?0:1' "$work_dir/colour-mask.png"
magick "$work_dir/colour-mask.png" -threshold 50% -morphology Close Disk:2 -morphology Open Disk:1 "$work_dir/colour-mask-clean.png"
magick "$work_dir/colour-mask-clean.png" "$work_dir/master-alpha.png" -compose Multiply -composite "$work_dir/mask-clean.png"
magick "$work_dir/mask-clean.png" -alpha copy -channel RGB -fill '#1F2326' -colorize 100 \
  -strip -define png:exclude-chunks=date,time "$destination"

echo "Generated deterministic monochrome derivative at $destination"
