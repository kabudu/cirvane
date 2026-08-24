# Validation

## Established evidence

The imported benchmark corpus records controlled v1 and optimized v2 restart and command-latency measurements, signed OTA rollback/confirmation, functional HIL and adversarial security checks. The current strongest supported claims are limited to the tested XIAO ESP32-C5, pinned toolchain and recorded protocol.

## Research questions

- Does Cirvane retain bounded behaviour and recovery under malformed input and service failure?
- Does a kernel-owned bounded recovery transaction eliminate stale post-restart work and reduce recovery variance relative to matched application-level supervision?
- Does the production image own scheduling and recovery without a hidden FreeRTOS runtime?
- Does authenticated OTA refuse every incomplete, stale, mismatched or unauthenticated update without changing the selected boot target?
- Does the renamed release preserve the established boot and command-latency envelope?

## Baselines and metrics

Use the preserved v1 firmware and imported Nucleus v2/FreeRTOS evidence at identical hardware, clock, compiler profile, service scope, resource ceilings, serial transport and sample procedure. Primary metrics are stale-work violations, recovery outcome, recovery latency median and tail, recovery variance, scheduler and message latency tails, boot variance, bounded RAM and flash use, and rollback outcome. Record admission refusal, missing samples, timeouts and incomparable driver paths. Energy is excluded until calibrated instrumentation exists.

## Claim discipline

No novelty, production, safety, energy, physical-security, formal-verification or independent-validation claim may be inferred from branding or from the current corpus. A qualified candidate-novelty statement requires the planned internal systematic comparison and matched falsification evidence. Negative results, refusals, timeouts and unavailable measurements remain in evidence.

## Brand identity evidence

Recovery Scar was owner-selected after two rejected geometric rounds and one creative reset. Canonical SVGs provide full-colour, small, monochrome, reversed, wordmark, horizontal, stacked and operational-icon variants. Machine validation rejects scripts, external resources, unsafe links, maturity language in canonical assets, insufficient declared text contrast, missing assets, digest drift and dimension drift.

`scripts/export-brand-assets.sh` renders the committed PNG set with librsvg. Local CI renders two independent output trees, compares their SHA-256 inventories and compares the result with the committed exports. `scripts/validate_brand.py` verifies the source inventory, XML safety, accessible naming, declared contrast pairs and `BRAND_ASSET_MANIFEST.json`.

Visual review on 2026-08-24 covered light and dark horizontal lockups, stacked lockup, colour and monochrome symbols, the 16px responsive symbol, operational icons, architecture diagram key, outcome chart and 1200 by 630 release overlay. The remaining external gates are professional trademark clearance, cultural review and owner approval of the final public surfaces.
