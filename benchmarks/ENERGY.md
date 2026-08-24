# Energy measurement status

Electrical energy was not measurable in the 2026-08-23 run because no
calibrated inline current monitor was connected. The macOS USB inventory
exposed only the ESP32-C5 USB Serial/JTAG device. USB descriptor current, CPU
runtime, radio-active time, and timing results are not substitutes for measured
current, so no v1-versus-v2 energy claim is made.

## Reproducible follow-up protocol

1. Power the board through a calibrated, logging current monitor with the USB
   data connection isolated from the measured supply path.
2. Record supply voltage and current at 1 kHz or faster for at least five
   minutes per firmware after a 60-second warm-up.
3. Use the same cable, voltage, Wi-Fi environment, LED mode, radio state, and
   command workload for v1 and v2; alternate firmware order between runs.
4. Report mean current, p95 current, energy per `info` command, and integrated
   joules over the fixed workload, including raw timestamped samples and the
   instrument model/calibration date.
5. Separately measure v2 watchdog and sentinel budgets. Do not compare those
   modes with v1 active mode as if they were equivalent workloads.
