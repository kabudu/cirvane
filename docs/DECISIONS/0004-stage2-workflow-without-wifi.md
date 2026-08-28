# ADR 0004: Stage 2 workflow without Wi-Fi

## Status

Accepted on 2026-08-27. This records the owner decision required by ADR 0003
and risk R9.

## Context

ESP-IDF Wi-Fi on ESP32-C5 schedules on FreeRTOS. Linking that adapter would
hide FreeRTOS under a clean-sheet kernel. The Stage 2 exit gate requires a
supported hardware workflow. Completing Stage 2 therefore required an explicit
choice among: a first workflow without Wi-Fi, an isolated non-kernel radio
boundary, or stopping the kernel path.

## Decision

The Stage 2 supported workflow is UART (USB Serial/JTAG), GPIO 27, SYSTIMER,
flash read, config-window and otadata erase/write, watchdog mute and LPPERI
entropy. Wi-Fi and other radio are excluded until a later owner decision
implements a genuinely isolated non-kernel radio boundary.

This does not claim a product without networking is complete. It narrows the
Stage 2 hardware workflow so the combined HAL checkbox can close without
linking FreeRTOS. Matched evaluation collects no Wi-Fi samples on either
image.

## Consequences

Stage 3+ may restore radio only with a new ADR, inventory row and evidence.
Release copy must not imply Wi-Fi on the Cirvane kernel. Risk R9 remains open
for any later radio increment and is closed as a Stage 2 blocker.
