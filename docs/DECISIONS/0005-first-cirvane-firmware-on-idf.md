# ADR 0005: first Cirvane firmware ships on ESP-IDF/FreeRTOS

## Status

Accepted on 2026-08-28. Owner decision.

## Context

ADR 0002 made a clean-sheet Cirvane kernel a blocker for the first labelled
developer release. Stage 2 qualified a FreeRTOS-free C5 spike, but Espressif
Wi-Fi still requires FreeRTOS, and the kernel is not a complete product
stack. The owner chose to ship the reliable ESP-IDF/FreeRTOS firmware under
the Cirvane name, with an explicit substrate claim, rather than wait for a
kernel that owns radio and the operator surface.

## Decision

The first labelled Cirvane firmware identity is the existing supervised
ESP-IDF/FreeRTOS image. Current product names, the USB prompt and the
application binary use Cirvane. Release copy and README must state that this
image is powered by ESP-IDF and FreeRTOS. It must not be described as a
clean-sheet kernel.

The kernel spike remains research and a later kernel-release path. ADR 0002
still applies to any release that claims Cirvane owns scheduling without
FreeRTOS. Historical Nucleus evidence, the `benchmarks/nucleus-v1` tree and
matched-baseline wording stay Nucleus.

Authenticated OTA transport, public visibility and the other gates in
`docs/RELEASE.md` remain unwaived.

## Consequences

Rename current firmware, shell, build and test identifiers in this increment.
The NVS namespace becomes `cirvane`, which resets the preserved boot counter
on existing boards. Do not rewrite recorded HIL logs. Update ADR 0002 so the
first Cirvane firmware ship is no longer blocked on kernel ownership.
Stop-ship still applies if copy implies a clean-sheet kernel for this image.
