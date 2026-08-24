# Validation

## Established evidence

The imported benchmark corpus records controlled v1 and optimized v2 restart and command-latency measurements, signed OTA rollback/confirmation, functional HIL and adversarial security checks. The current strongest supported claims are limited to the tested XIAO ESP32-C5, pinned toolchain and recorded protocol.

## Research questions

- Does Cirvane retain bounded behaviour and recovery under malformed input and service failure?
- Does authenticated OTA refuse every incomplete, stale, mismatched or unauthenticated update without changing the selected boot target?
- Does the renamed release preserve the established boot and command-latency envelope?

## Baselines and metrics

Use the preserved v1 firmware and imported Nucleus v2 evidence at identical hardware, compiler profile, serial transport and sample procedure. Primary metrics are restart median and p95, restart variance, shell command latency, recovery outcome and rollback outcome. Energy is excluded until calibrated instrumentation exists.

## Claim discipline

No novelty, production, safety, energy, physical-security or independent-validation claim may be inferred from branding or from the current corpus. Negative results, refusals, timeouts and unavailable measurements remain in evidence.
