# Kernel tree

Stage 2 portable core plus the C5 spike port.

- `kernel.h`, `kernel.c`, `kernel_test.c` panic, IRQ table, cooperative
  scheduler, typed messages, capability leases and frozen syscalls.
- `config.c` two-slot CRC journal. `rollback.c` dual-slot fail-closed
  selection policy. Image verify remains an injected adapter.
- `hal.h` plus `hal_host.c` (host mock) and `spike/hal_c5.c` (C5 MMIO/ROM).
  UART, GPIO 27, timer, flash read, watchdog mute and entropy. No radio.
- `recovery/` bounded recovery transaction model used by those syscalls.
- `spike/` FreeRTOS-free ESP32-C5 SRAM image with `hil` and `production`
  compile profiles. `hil` is the HIL probe image. `production` omits inject
  and `result=PASS` reprint. Neither is a complete product kernel.

Build the HIL spike with `scripts/build-kernel-spike.sh`. Build the production
profile with a second `production` argument. Capture board evidence with
`benchmarks/tools/kernel_spike_hil.py` after OpenOCD `program_esp` of the HIL
image. Flashing replaces the current application slot; restore the Nucleus
baseline afterwards. Scheduler policy, journal, rollback and HAL rules are in
`docs/KERNEL.md`.
