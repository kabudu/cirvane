# Cirvane kernel core

Stage 2 portable kernel. This is not a complete production kernel and not a
novelty claim.

C5 reset, BSS, trap entry, SYSTIMER, CLIC/INTMTX, USB Serial/JTAG and the
ESP-IDF second-stage bootloader remain in `kernel/spike`. The portable core
owns panic, interrupt dispatch, cooperative scheduling, typed messages,
capability leases and the frozen syscalls that wrap the recovery transaction.

## Scheduler policy

- Eight statically bound services. No heap after `cirvane_kernel_init`.
- Cooperative round-robin. One tick function per step. No preemption.
- Unbound, recovering and `FAILED` services are skipped. Missing ticks are
  skipped. An idle step returns refused.
- No priority, deadline admission or fair-share accounting in this increment.
  A tick that does not return can starve every other service (starvation is
  accepted under cooperative scheduling). Ticks must not
  call `cirvane_sched_step`.
- Work per step is one function call plus the syscalls that tick makes. The
  message pool remains 32 slots.

## Messages

24-byte payloads in a 32-slot pool. Slots are epoch-tagged. Send is refused
when the slot is not deliverable, the destination is unbound, recovering or
failed, or either end lacks `CIRVANE_CAP_MSG`. Receive returns only queued
current-epoch slots. Exhaustion, double-free and unknown syscall numbers
refuse. Admit reclaims owned slots and clears their payloads.

## Capabilities

Leases are software-only bitmasks on the service record. They are not mapped
to PMP, PMA or APM in this increment. Grant replaces the lease of a bound
service that is not recovering or failed. Revoke clears it immediately. Admit
already zeros the lease before health may become `OK` or `DEGRADED`.

## Syscall arguments

| Number | Name | a0 | a1 | a2 | Result |
|---:|---|---|---|---|---|
| 1 | `SYS_RTX_BIND` | service | budget | lease | 0 or refused |
| 2 | `SYS_RTX_ADMIT` | service | reason | unused | outcome 0-3 |
| 3 | `SYS_MSG_ALLOC` | service | unused | unused | slot or refused |
| 4 | `SYS_MSG_SEND` | slot | dest | type | 0 or refused |
| 5 | `SYS_MSG_RECV` | service | unused | unused | slot or refused |
| 6 | `SYS_MSG_FREE` | slot | unused | unused | 0 or refused |
| 7 | `SYS_CAP_GRANT` | service | lease | unused | 0 or refused |
| 8 | `SYS_CAP_REVOKE` | service | unused | unused | 0 or refused |

Unknown numbers refuse. After panic, every syscall and scheduler step refuses.

## Panic

Unexpected traps set a sticky panic reason and wait in `wfi`. They do not
reprint success, restart the faulting instruction as a healthy path, or clear
recovery evidence. Host tests cover the sticky refuse behaviour. The HIL image
still reprints `result=PASS` after a successful probe sequence so capture can
see the marker; that loop is idle, not panic.

## Not in this increment

Transactional configuration, signed dual-slot rollback, UART/GPIO/watchdog
drivers beyond the spike probes, live user-mode `mret`, radio/Wi-Fi, matched
FreeRTOS evaluation and adversarial kernel qualification remain unchecked.
