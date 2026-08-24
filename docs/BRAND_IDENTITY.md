# Brand identity

## Productisation decision

The owner approved Cirvane as the enduring product identity on 2026-08-24. The identity must remain valid for the current supervised ESP-IDF runtime and a possible future custom kernel. Product maturity belongs in separately versioned release copy, badges and notices, never in the canonical name, mark, palette, tagline or category.

## Name and category

- **Name:** Cirvane
- **Pronunciation:** sir-vane
- **Capitalisation:** Cirvane in prose; `cirvane` in commands, packages and machine identifiers
- **Canonical category:** bounded embedded operating system
- **Tagline:** Recover visibly.

The point-in-time name audit checked exact and case-folded general-web use, GitHub names and users, npm, PyPI, crates.io, the ESP component surface and RDAP. No material same-market exact collision was found. This is not trademark clearance; UK, EU and US legal review remains mandatory before public opening.

## Brand platform

- **Purpose:** make constrained-device behaviour understandable under failure.
- **Promise:** bounded control and visible recovery for embedded systems.
- **Principles:** explicit limits, observable decisions, reversible change, evidence before claims.
- **Personality:** calm, precise, capable, resilient, quietly unconventional.
- **Anti-traits:** militaristic, magical, invulnerable, opaque, frivolous, sterile corporate technology.
- **Value proposition:** Cirvane helps embedded developers understand what the device is doing, what failed, what was refused and how it recovered without adding an unbounded general-purpose runtime.
- **Reasons to believe:** compile-time resource bounds, supervised services, transactional configuration recovery, signed dual-slot rollback, explicit shell diagnostics and committed hardware evidence. Each reason is limited to the version and hardware for which evidence exists.

## Messaging system

Permanent identity and maturity overlay are separate layers.

### Permanent descriptions

- **Short:** Cirvane is a bounded embedded operating system built to make recovery visible.
- **Medium:** Cirvane is a bounded embedded operating system for constrained devices. It supervises a fixed service set, records operational state and makes rejection, rollback and recovery explicit.
- **Technical:** Cirvane is an ESP-class embedded operating system profile with statically bounded services and messages, transactional configuration, signed dual-slot firmware rollback, typed asynchronous work and a privileged local operator shell.

### Voice by context

| Context | Voice | Required behaviour |
|---|---|---|
| Developer guidance | Direct, exact, practical | Name commands, prerequisites, observable result and recovery path. |
| Research | Falsifiable, sourced, qualified | Separate prior art, hypothesis and measured evidence. |
| Security | Calm, explicit, non-reassuring | State boundary, threat, consequence, mitigation and residual risk. |
| Buyer or adopter | Outcome-led, scoped | Name supported hardware and avoid extrapolating from prototypes. |
| Incident | Terse, chronological, actionable | Lead with status, impact, containment and next verified action. |

Do not call failures surprises, magic or impossible states. Prefer refused, degraded, rolled back, unverified, unknown and recovered when those states are actually evidenced.

## Claim vocabulary

Allowed current terms include bounded, supervised, rollback-tested, signed-update capable, developer release and hardware-tested on the named ESP32-C5 board. Prohibited current terms include production-ready, secure, unbreakable, novel kernel, safety-certified, energy-efficient, independently validated and universally compatible.

## Recovery Scar mark

The owner selected Recovery Scar on 2026-08-24 after rejecting two rounds of flat geometric marks. The mark is an irregular recovered artefact divided by a living mineral-teal seam. The disruption is not hidden: repair becomes the most recognisable feature.

The canonical master is `assets/brand/source/cirvane-symbol.svg`. It was manually reconstructed as deterministic SVG geometry from an AI-assisted concept and contains no traced raster, font, external resource or third-party icon.

### Construction and responsive use

- Use the full-colour symbol at 32px or larger.
- Use `cirvane-symbol-small.svg` from 16px through 31px.
- Use the monochrome symbol when colour reproduction, forced colour or print constraints would collapse the seam.
- Use the reversed symbol and lockup only on aubergine, graphite or imagery dark enough to maintain contrast.
- Minimum horizontal lockup width is 136px; minimum stacked width is 86px.
- Clear space on every side is one quarter of the symbol's rendered width.
- Never rotate, mirror, outline, add effects, recolour individual masses, close the scar, place content inside the scar or animate the pieces as an explosion.

The symbol may be accompanied by accessible text or `aria-label="Cirvane"`; decorative repetitions use empty alternative text. The mark is not a status icon. Product states always use the canonical operational icons plus text.

## Colour system

| Role | Token | Value | Use |
|---|---|---|---|
| Recovery | Mineral teal | `#0E7C78` | Scar, active focus and brand accent, never success alone. |
| Memory | Aubergine | `#3A2748` | Brand ground, selected editorial fields and structural mass. |
| Trace | Sea-glass | `#CDE7E2` | Scar highlight and quiet supporting surfaces. |
| Ground | Warm off-white | `#F8F6EF` | Primary light surface and reversed mark. |
| Structure | Graphite | `#1F2326` | Primary text, dark surface and structural mass. |

Semantic success, warning, failure, information and unknown colours are separate tokens. Every semantic state requires an icon or label in addition to colour. Charts must retain failed, refused, excluded and unknown results rather than hiding them in neutral totals.

## Typography

The canonical wordmark is custom path geometry. It must never be recreated with a font. Product and documentation typography use the platform system sans stack; code, shell and evidence use the platform system monospace stack. No font file is redistributed.

- Display: 700 weight, compact leading, sentence case.
- Interface: 400 or 550 weight; use 700 only for short hierarchy labels.
- Technical evidence: monospace, tabular numerals where available, never simulated handwriting.
- Minimum interface copy is 14px; minimum supporting metadata is 12px when contrast meets WCAG AA.

## Icons, diagrams and charts

Operational icons use a 64-unit cell, four-unit rounded stroke and simple open geometry. The canonical set represents boundary, recovery, evidence and refusal. Do not substitute the product symbol for operational meaning.

Architecture diagrams flow left to right. Sea-glass nodes are bounded components, aubergine outlines are decisions, graphite nodes are committed outcomes, mineral-teal solid paths are accepted transitions and labelled failure-red dashed paths are refused or rolled-back transitions.

Charts use direct labels and retain uncertainty. Success is green, failure is red, unknown uses neutral hatching and excluded values use an outlined form. Never use mineral teal as an unlabeled synonym for success.

## Imagery and motion

Photography, when used, should show real hardware, work surfaces, instrumentation and human-scale engineering context. Illustration may use recovered surfaces, irregular boundaries and visible seams. Avoid generic circuit-board backgrounds, glowing AI brains, cyber grids, militarised hardware and anonymous stock teams.

Motion may reveal the scar through a single 360ms recovery transition after the masses are already present. It must not loop, shake, explode or imply self-healing guarantees. Respect `prefers-reduced-motion` by rendering the final static state immediately.

## Accessibility and forced colour

Required text pairs are machine-validated against WCAG AA. High-contrast and forced-colour modes use the monochrome symbol, platform colours and shape/text redundancy. `NO_COLOR` terminal output preserves labels and ASCII status words. Focus is never communicated by colour alone.

## Assets, provenance and governance

Canonical sources live in `assets/brand/source/`; deterministic exports in `assets/brand/exports/`; templates in `assets/brand/templates/`; tokens in `assets/brand/tokens/`; licences in `assets/brand/LICENSES/`; and digests plus allowed use in `assets/brand/BRAND_ASSET_MANIFEST.json`.

Product and brand ownership remains with the repository owner. Engineering owns deterministic export and manifest integrity. Security owns claim-boundary review. Accessibility owns contrast and non-colour semantics. Legal owns trademark clearance before public opening. Changes require a versioned source change, regenerated exports, validation, PR review and an updated manifest; released assets are archived rather than silently overwritten.

## Remaining external gates

The identity is technically complete only when repository validation, deterministic reproduction and visual QA pass. Public use additionally requires professional name and mark clearance, cultural review and owner approval of the rendered release surfaces. Visual polish is not product, security or production evidence.
