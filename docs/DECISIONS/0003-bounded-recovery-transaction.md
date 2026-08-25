# ADR 0003: bounded recovery transaction and clean-sheet spike boundary

## Status

Accepted for planning on 2026-08-25. Implementation of the production kernel
remains unverified. Novelty wording is explicitly provisional.

## Context

ADR 0002 made a clean-sheet Cirvane kernel a blocker for the first developer
release and named a bounded recovery transaction as the candidate contribution.
Stage 1 required a systematic comparison, a FreeRTOS-free ESP32-C5 spike, an
enumerated vendor boundary, frozen invariants and a pre-registered matched
evaluation before Stage 2 kernel implementation.

Hubris, VirtuosoNext, seL4/Microkit, Tock and Minix 3 already provide restart,
isolation or supervisor recovery. ESP-IDF Wi-Fi requires FreeRTOS adapter
primitives. Language choice and the radio path therefore had to be decided from
toolchain and vendor evidence, not from novelty theatre.

## Decision

1. Freeze the candidate kernel mechanism as a **bounded recovery transaction**
   with the invariants in `docs/KERNEL_SPIKE.md`. Claims stay hypothetical.
2. Implement Stage 1 in C11 plus a RISC-V assembly trap/reset path, using the
   already bootstrapped `riscv32-esp-elf` GCC from ESP-IDF v6. Unsafe code is
   limited to reset, trap entry and MMIO. The portable recovery model contains
   no inline assembly.
3. Treat ESP-IDF second-stage bootloader, ROM functions and USB Serial/JTAG
   MMIO as enumerated vendor dependencies of the spike. Do not link FreeRTOS.
4. Record that the ESP-IDF Wi-Fi library schedules on FreeRTOS. The first
   clean-sheet release may not claim Wi-Fi until the owner chooses a narrower
   release without Wi-Fi, a genuinely isolated non-kernel radio boundary, or
   stopping the kernel path.
5. Pre-register matched evaluation in `docs/MATCHED_EVALUATION.md` before any
   performance tuning of recovery.

## Consequences

Stage 2 may implement the production kernel only against this frozen ABI and
these fail-closed rules. If matched evaluation or closer prior art falsifies
NOV-01 or NOV-02, retract or narrow the wording and keep any useful engineering
result. Do not rename Hubris-style supervisor restart as a Cirvane invention.

Changing this ADR requires new prior-art evidence, a revised dependency
inventory, and owner agreement if the radio or language boundary moves.
