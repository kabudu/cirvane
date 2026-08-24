# Brand asset usage

## Selection guide

| Context | Preferred asset |
|---|---|
| Repository header or website navigation | `cirvane-horizontal.svg` |
| Dark navigation or dark photography | `cirvane-horizontal-reversed.svg` |
| Square avatar or app icon at 32px and above | `cirvane-symbol.svg` |
| Favicon, compact menu or device list | `cirvane-symbol-small.svg` |
| One-colour print, embossing or forced colour | `cirvane-symbol-mono.svg` |
| Centred cover, poster or presentation | `cirvane-stacked.svg` |
| Text-only constrained header | `cirvane-wordmark.svg` |

Never use candidate assets outside design review. The `assets/brand/candidates/` tree records rejected and exploratory work and is excluded from the canonical manifest. The approved concept has been promoted by copying it into `assets/brand/source/cirvane-symbol-dimensional.png`; consumers must use the canonical source or exports, not the candidate copy.

## Plain-text fallback

Use `Cirvane` in prose, `cirvane>` for the device shell prompt and `[CIRVANE]` where an uppercase terminal label is required. Do not approximate the symbol with Unicode art.

## Release overlays

Lifecycle and assurance language belongs in a separately named overlay instance. Start from the release-card template, replace the protected message area with truthful version-specific copy, and save the result outside canonical source assets. Never add alpha, beta, preview, production-ready or certification wording to the symbol, wordmark or core lockups.

## Handoff checklist

1. Select the correct responsive and contrast variant.
2. Preserve clear space and aspect ratio.
3. Provide accessible naming when the mark conveys identity.
4. Use semantic icons and labels for operational states.
5. Regenerate rather than editing an exported PNG or resizing the candidate master independently.
6. Verify the asset digest against `BRAND_ASSET_MANIFEST.json`.
