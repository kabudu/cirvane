# Kernel tree

Stage 2 portable core plus the C5 spike port.

- `kernel.h`, `kernel.c`, `kernel_test.c` panic, IRQ table, cooperative
  scheduler, typed messages, capability leases and frozen syscalls.
- `config.c` two-slot CRC journal. `rollback.c` dual-slot fail-closed
  selection policy. Image verify remains an injected adapter.
- `recovery/` bounded recovery transaction model used by those syscalls.
- `spike/` FreeRTOS-free ESP32-C5 SRAM image: reset, trap entry, timer, USB
  console and HIL probes that exercise the portable core.

Build the spike with `scripts/build-kernel-spike.sh`. Capture board evidence
with `benchmarks/tools/kernel_spike_hil.py --flash`. Flashing replaces the
current application slot; restore the Nucleus baseline afterwards. Scheduler
policy, journal and rollback rules are in `docs/KERNEL.md`.
