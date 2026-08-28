# ADR 0002: clean-sheet kernel release boundary

## Status

Accepted for planning on 2026-08-24. ADR 0005 supersedes this decision for the
first labelled Cirvane firmware ship: that image is ESP-IDF/FreeRTOS with an
honest substrate claim. This ADR still gates any release that claims a
clean-sheet Cirvane kernel with no hidden FreeRTOS runtime.

## Context

ADR 0001 planned the first Cirvane developer release on ESP-IDF and FreeRTOS, with a clean-sheet kernel deferred. The owner has changed the release mandate: the first release should run on a Cirvane-owned kernel on the Seeed Studio XIAO ESP32-C5. The preserved Nucleus/FreeRTOS implementation provides migration behaviour and a matched baseline.

ESP32-C5 exposes RISC-V Machine and User privilege levels, PMP/PMA and Espressif access controls, but ESP-IDF system components are deeply integrated with FreeRTOS. A credible clean-sheet claim therefore requires Cirvane to own reset-to-runtime scheduling and recovery semantics, enumerate vendor dependencies, and prove that FreeRTOS is absent from the production runtime. Replacing only public APIs or hiding FreeRTOS behind a compatibility layer is insufficient.

## Decision

Superseded for the first labelled Cirvane firmware by ADR 0005. For any release that claims a clean-sheet Cirvane kernel: make that kernel a blocker. Begin with a narrow bounded recovery transaction hypothesis, systematic prior-art comparison and a minimal FreeRTOS-free hardware spike. Only then implement the production kernel, migrate the existing services, add authenticated OTA and prepare the kernel-labelled release.

Independent penetration testing, independent novelty challenge, clean-room reproduction and formal verification are optional post-release assurance stages. Internal prior-art diligence, matched baseline evaluation, real-board adversarial tests and truthful claim wording remain release blockers.

## Consequences

This decision supersedes ADR 0001 only where ADR 0001 selects ESP-IDF/FreeRTOS as the first release substrate. It substantially increases schedule, driver, radio, toolchain and correctness risk. Wi-Fi support is a named feasibility gate because vendor components may assume FreeRTOS. If the required workflow cannot be supported without a hidden FreeRTOS scheduler, the owner must explicitly narrow the first release, approve a genuinely isolated radio boundary or stop the kernel release path.

The project must not call the kernel novel merely because it is clean-sheet. Novelty wording remains gated by the comparison and evidence recorded in `NOVELTY.md` and `PRODUCTISATION_COMPLETION_PLAN.md`.
