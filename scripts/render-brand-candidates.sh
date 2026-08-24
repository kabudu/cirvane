#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
candidate_dir="$repo_root/assets/brand/candidates"
output_dir="$repo_root/build/brand-candidates"

if ! command -v magick >/dev/null 2>&1; then
  echo "ImageMagick is required to render brand candidates" >&2
  exit 1
fi

python3 "$repo_root/scripts/validate_brand_candidates.py"
mkdir -p "$output_dir/colour" "$output_dir/monochrome" "$output_dir/small"

for source in "$candidate_dir"/*.svg; do
  name="$(basename "$source" .svg)"
  magick -background none "$source" -strip "$output_dir/colour/$name.png"
  sed -e 's/#0E7C78/#1F2326/g' -e 's/#3A2748/#1F2326/g' "$source" \
    | magick -background none svg:- -strip "$output_dir/monochrome/$name.png"
  magick -background none "$source" -resize 16x16 -gravity center -extent 16x16 -strip \
    "$output_dir/small/$name.png"
done

magick "$output_dir/colour"/*.png +append -background '#F8F6EF' -alpha background -strip \
  "$output_dir/comparison-colour.png"
magick "$output_dir/monochrome"/*.png +append -background '#F8F6EF' -alpha background -strip \
  "$output_dir/comparison-monochrome.png"
magick "$output_dir/small"/*.png +append -filter point -resize 800% \
  -background '#F8F6EF' -alpha background -strip "$output_dir/comparison-16px.png"

echo "Rendered brand candidates to $output_dir"
