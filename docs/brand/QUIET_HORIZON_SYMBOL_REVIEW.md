# Quiet Horizon symbol review

## Decision state

Cirvane and the Quiet Horizon direction are owner-approved. The palette is owner-approved. The three marks in `assets/brand/candidates/` are candidates, not canonical assets. Selection and legal clearance remain open.

## Method

The review used manually authored `128 x 96` SVG masters, automated SVG safety and palette validation, full-size colour rendering, one-colour rendering and a 16-pixel raster test. Visual collision checks on 2026-08-24 covered the official Zephyr, FreeRTOS, Eclipse ThreadX and RIOT brand surfaces, a broader embedded-OS logo survey, and searches for offset, staggered, gate and transition-bar marks.

The search found no close exact match within the reviewed embedded-OS set. It did find widespread use of parallel and staggered bars across technology and workflow brands, including generic icon libraries and unrelated commercial marks. That makes geometry, not palette, the primary distinctiveness risk. This review is neither a reverse-image search nor trademark clearance. A professional clearance search remains a public-opening gate.

Primary category references:

- [Zephyr branding](https://www.zephyrproject.org/branding/)
- [FreeRTOS](https://www.freertos.org/)
- [Eclipse ThreadX](https://threadx.io/)
- [RIOT branding](https://www.riot-os.org/branding.html)

## Non-leading scorecard

Scores use a five-point scale where five is strongest. Ambiguity scores describe resistance to unintended meaning, so higher is better.

| Candidate | Comprehension | Distinctiveness | 16px | Monochrome | Ambiguity resistance | Enduring fit | Total |
|---|---:|---:|---:|---:|---:|---:|---:|
| A: Bounded Gate | 4 | 3 | 4 | 5 | 2 | 4 | 22 |
| B: Controlled Shift | 4 | 3 | 5 | 5 | 3 | 4 | 24 |
| C: Recovery Channel | 3 | 4 | 2 | 4 | 3 | 4 | 20 |

## Candidate findings

### A: Bounded Gate

The explicit inner gates communicate constraint and supervision. The silhouette remains legible in one colour and at 16px. Its material weakness is unintended symbolism: it can read as a pause control, bridge, or letter H. It is viable only if further optical refinement materially reduces those readings.

### B: Controlled Shift

The stable upper and lower states plus one bounded transition express controlled change most directly. It has the strongest small-size and monochrome result. Its material weakness is category breadth: step, transfer and integration motifs are common, so legal and reverse-image clearance deserve particular attention.

### C: Recovery Channel

The converging paths give the clearest recovery narrative and the most unusual full-size silhouette. Its central detail collapses at 16px and its directional form can suggest networking or synchronization. The current construction fails the small-size gate and cannot be selected without simplification.

## Accessibility and claim review

- Every candidate retains its silhouette in one colour; colour is not the only identifier.
- The approved aubergine and mineral-teal palette remains separate from status semantics. Success, warning and failure states must use text or icon redundancy.
- The marks contain no lifecycle stage, security badge, certification mark or performance claim.
- Human cultural and localisation review remains incomplete.

## Reproduction

Run:

```sh
python3 scripts/validate_brand_candidates.py
scripts/render-brand-candidates.sh
```

Generated previews are written under ignored `build/brand-candidates/`. The SVG sources are the decision artefacts.
