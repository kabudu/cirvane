# Pre-registered matched evaluation

Registered on 2026-08-25, before Stage 2 implementation or recovery-latency
tuning. Changing a metric, workload or stopping rule after seeing results
requires a dated amendment; silent retuning is prohibited.

## Question

Does a kernel-owned bounded recovery transaction eliminate stale post-restart
work and reduce recovery latency variance relative to equivalent
application-level supervision on the preserved FreeRTOS/Nucleus baseline?

This is NOV-02. NOV-01 remains a prior-art hypothesis and is not a performance
claim.

## Hardware and identity

- Board: Seeed Studio XIAO ESP32-C5, same unit as the imported baseline where
  possible.
- Clock, compiler profile, serial transport and USB cable: match the recorded
  v2 baseline procedure in `benchmarks/README.md`.
- Cirvane image: production kernel profile once Stage 2 exists. The current
  `production` spike compile boots a quiet `cirvane>` shell; it is not the
  matched workload image. The HIL spike is feasibility evidence only and is
  **not** a matched performance trial.
- Baseline image: preserved Nucleus v2/FreeRTOS build at the recorded digest.

## Workload

Identical service scope: one heartbeat-class service and one injectable fault
service. Resource ceilings remain 8 services, 32 message slots, 24-byte
payload. Injected faults:

1. Service-reported fault with in-flight messages.
2. Deadline breach with in-flight messages.
3. Restart-budget exhaustion.
4. Nested fault during recovery (must refuse).
5. Message pool exhaustion then fault.

No Wi-Fi in the matched pair until the owner radio decision lands; the
baseline Wi-Fi scan path is otherwise incomparable.

## Metrics

Primary:

- Stale-work violations after recovery (count; target 0 on Cirvane).
- Recovery outcome (`restart`, `backoff`, `failed`, `refused`) vs oracle.
- Recovery latency median, p95 and population standard deviation.
- Scheduler, message and interrupt latency tails as supporting metrics.
- Boot-time variance, static RAM, stack high-water, flash size.

Secondary: bounded work per recovery (slots reclaimed, no retry loop).

Energy remains excluded until calibrated instrumentation exists.

## Resource ceilings

Do not raise slot, service, stack or copy-buffer ceilings to improve a
headline number. If a ceiling must change, record it as a protocol amendment
and rerun both sides.

## Statistics and stopping rules

- Warm-up: discard 3 recoveries per image, then collect at least 30 recoveries
  per fault class, counterbalanced A,B,B,A across images.
- Report raw samples, median, p95, population SD, toolchain, image digest,
  port and timestamp.
- Stopping rule: halt a class if 3 consecutive infrastructure serial failures
  occur; classify as infrastructure, not product pass. Halt the comparison if
  Cirvane records a stale-work violation (NOV-02 fail for that class).
- Missing-data: timeouts and refused admissions stay in the corpus. Do not
  impute. If a driver path exists on only one image, mark the metric
  incomparable.

## Claim rule

A differentiated Cirvane result may be stated only for a pre-registered metric
with both images present. Worse, missing or incomparable metrics remain
visible and constrain release copy. Stage 1 does not publish performance
comparisons from the spike.

## Amendment 2026-08-27 (before Stage 2 sample collection)

Single-board OpenOCD programming makes per-sample A,B,B,A image switching
impractical. Collection is image-blocked: Cirvane `eval` profile first, then
Nucleus HIL `svcfail` of service 0 (`led-heartbeat`). Warm-up 3 and n>=30 stay
in force for every class that both images can run.

Cirvane classes 2 to 5 (deadline, budget exhaustion, nested refuse, pool
exhaustion) have no matching Nucleus injector on this board. Those classes are
Cirvane-only and incomparable. Scheduler, message and interrupt tails have no
matched Nucleus probes. No Wi-Fi samples are collected on either image
(ADR 0004). Cirvane recovery latency is raw ESP32-C5 SYSTIMER ticks; Nucleus
class-1 latency is host wall milliseconds around supervisor backoff. Units are
not converted. Cirvane stale-work must remain 0 on every kept sample or NOV-02
fails for that class.

The Cirvane matched image is the `eval` compile, not the quiet production
shell. The HIL spike remains feasibility evidence and is not this trial.

## Amendment 2026-08-28 (after Nucleus class-1 capture)

Nucleus class-1 samples were taken on the same XIAO ESP32-C5 after a chip RST
that mapped app IROM. The HIL image recorded 33 wall-ms recoveries in
`s_cirvane_matched` (magic `0xC1455E01`); JTAG dumped the record; warmup 3
were discarded. `matched-eval.json` `result=pass` with `stale_total=0` and
Nucleus `stats_ms.n=30`. The 2s/3s/4s cycle is the Nucleus 1s-shifted
supervisor backoff. Cirvane class-1 remains SYSTIMER ticks. Units are not
converted. Classes 2-5, tails versus FreeRTOS, and Wi-Fi stay incomparable.
