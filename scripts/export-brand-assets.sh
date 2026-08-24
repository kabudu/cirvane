#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_dir="$repo_root/assets/brand/source"
template_dir="$repo_root/assets/brand/templates"
output_dir="${1:-$repo_root/assets/brand/exports}"

if ! command -v rsvg-convert >/dev/null 2>&1; then
  echo "rsvg-convert is required; install librsvg before exporting brand assets" >&2
  exit 1
fi

"$repo_root/scripts/generate-brand-derivatives.sh"

mkdir -p "$output_dir"

render() {
  local source="$1"
  local width="$2"
  local height="$3"
  local destination="$4"
  rsvg-convert --keep-aspect-ratio --width "$width" --height "$height" \
    --output "$output_dir/$destination" "$source"
}

render "$source_dir/cirvane-symbol-small.svg" 16 16 cirvane-symbol-16.png
render "$source_dir/cirvane-symbol-small.svg" 32 32 cirvane-symbol-32.png
render "$source_dir/cirvane-symbol.svg" 64 64 cirvane-symbol-64.png
render "$source_dir/cirvane-symbol.svg" 128 128 cirvane-symbol-128.png
render "$source_dir/cirvane-symbol.svg" 512 512 cirvane-symbol-512.png
render "$source_dir/cirvane-symbol-mono.svg" 512 512 cirvane-symbol-mono-512.png
render "$source_dir/cirvane-symbol-reversed.svg" 512 512 cirvane-symbol-reversed-512.png
render "$source_dir/cirvane-wordmark.svg" 432 128 cirvane-wordmark-432.png
render "$source_dir/cirvane-horizontal.svg" 680 192 cirvane-horizontal-680.png
render "$source_dir/cirvane-horizontal-reversed.svg" 680 192 cirvane-horizontal-reversed-680.png
render "$source_dir/cirvane-stacked.svg" 432 416 cirvane-stacked-432.png
render "$source_dir/cirvane-icons.svg" 1024 256 cirvane-icons-1024.png
render "$template_dir/diagram-key.svg" 1280 480 cirvane-diagram-key-1280.png
render "$template_dir/chart-palette.svg" 1280 640 cirvane-chart-palette-1280.png
render "$template_dir/overlays/release-social-card.svg" 1200 630 cirvane-release-card-1200.png

echo "Exported deterministic brand assets to $output_dir"
