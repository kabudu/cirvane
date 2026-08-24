# Nucleus controlled benchmarks

This directory holds the provenance-backed Nucleus v1 fixture, host-side
measurement tools, and captured results used to compare Nucleus v1 and v2 on
the same XIAO ESP32-C5, USB cable, host, ESP-IDF v6.0.2 toolchain, and serial
settings.

The v1 behavioural source and defaults were recovered from the final Hermes
`write_file` records that produced the previously recorded `0xe0100` image,
rather than reconstructed from v2. Only comment punctuation and obsolete
ESP-IDF 6 component/Kconfig declarations and the equivalent consolidated
`cmd_system` registration call were normalised; the top-level CMake file reuses
the same vendored `cmd_system` component as v2.

## Measurement contract

- Alternate firmware order to reduce drift: v1, v2, v2, v1.
- Use at least 20 warm resets per firmware for boot-to-ready timing.
- Use at least 30 shell samples for non-mutating `info` command latency.
- Report raw samples, median, p95, population standard deviation, toolchain,
  firmware hashes, port, and timestamp.
- Treat USB enumeration as part of operator-visible boot time and use the same
  host reset/serial procedure for both versions.
- Do not infer electrical energy from CPU time, radio time, or USB descriptor
  current. Report energy only from calibrated external current measurements.
- Exercise OTA with signed images through ESP-IDF OTA APIs, including pending
  verification, explicit confirmation, and rollback after an unconfirmed boot.

Generated build trees remain ignored by the repository-wide `*/build/` rule.

## 2026-08-23 result

Raw records and the machine-generated aggregate live in `results/`. Boot was
counterbalanced `v1, v2, v2, v1`; corrected command latency was counterbalanced
`v2, v1, v1, v2`. The original block records retain an invalid 50 ms-timeout
latency field for auditability, but `summary.json` excludes it and uses only the
1 ms-timeout `*-latency-[ab].json` records.

Nucleus v2 reduced median restart-to-ready time from 1905.604 ms to 1772.185 ms
(7.00%). It did not improve variance: one 1928.780 ms v2 sample increased the
population standard deviation from 10.727 ms to 38.139 ms. Corrected median
command latency was 2.144 ms for v1 and 2.127 ms for v2, a negligible 0.79%
change. Signed OTA rollback and confirmation passed. Electrical energy remains
unmeasured for the reasons and protocol in `ENERGY.md`.

The follow-up performance pass moved Wi-Fi initialization behind the first
scan and reduced the bounded OTA copy buffer from 4 KiB to 1 KiB. A fresh
counterbalanced comparison measured v1, v2, v1, then v2 with the same default
compiler profile and 1 ms serial reader timeout. Median restart time fell from
1868.704 ms to 1139.002 ms (39.05%), and restart population deviation fell
from 11.694 ms to 0.365 ms (96.88%). Median `info` latency was transport-floor
parity at 2.2138 ms for v1 and 2.2146 ms for v2. The aggregate contains 40
restart samples per version, 180 v1 latency samples, and 240 v2 latency
samples. Signed OTA rollback/confirmation and the functional HIL suite passed.
