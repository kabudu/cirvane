# Kernel tree

Stage 1 only.

- `recovery/` portable bounded recovery transaction model and host tests.
- `spike/` FreeRTOS-free ESP32-C5 SRAM image.

Build the spike with `scripts/build-kernel-spike.sh`. Capture board evidence
with `benchmarks/tools/kernel_spike_hil.py --flash`. Flashing replaces the
current application slot; restore the Nucleus baseline afterwards.
