# Cirvane kernel core

Stage 2 portable kernel. This is not a complete production kernel and not a
novelty claim.

C5 reset, BSS, trap entry, SYSTIMER, CLIC/INTMTX, USB Serial/JTAG and the
ESP-IDF second-stage bootloader remain in `kernel/spike`. The portable core
owns panic, interrupt dispatch, cooperative scheduling, typed messages,
capability leases, frozen recovery syscalls, the two-slot configuration
journal and dual-slot rollback selection policy. The HAL API is portable; UART,
GPIO, timer, flash read, watchdog mute and entropy are implemented for C5 in
the spike. Radio is excluded.

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

## Configuration journal

Two 24-byte CRC-protected records. Highest valid generation wins. Commit writes
the next generation into `generation & 1`, then verifies the stored record
before publishing. Out-of-range heartbeat, supervisor period, LED mode or
schema refuses without writing. A corrupt CRC on the newest slot falls back to
the other valid slot or to compile-time defaults. The journal is flash-backed
at two 4096-byte sectors starting at `0x7FE000`, after the large dual-OTA
partitions. Commit erases one sector, writes the 24-byte record and publishes
only after a matching read-back. Out-of-window erase and write refuse.
Corrupt-slot injection is compile-gated (`CIRVANE_HIL_SPIKE` /
`CIRVANE_CFG_TEST`). Live ESP `otadata` selection is the enumerated adapter
in `kernel/otadata.c`.

## Dual-slot rollback policy

Two application slots. Copies are bounded to 1024-byte writes. `end` requires
the declared length and a successful injected verifier. `select` is refused
unless verification succeeded. Abort, truncation, oversize chunks, nested begin
and same-slot staging refuse and leave the boot slot unchanged. Confirm marks a
pending running image valid. Rollback of an unconfirmed pending image returns
to the previous valid slot.

Image signature parsing stays outside this module. Live ESP `otadata`
selection is an enumerated adapter: it writes one 32-byte
`esp_ota_select_entry_t` with ROM CRC32 of `ota_seq`, then HIL restores the
backup. Cirvane never selects a slot when the verify adapter fails.

## Hardware abstraction

The HAL is fail-closed and allocator-free. Empty UART writes, GPIO pins at or
above 32, levels other than 0/1, flash reads of length 0, unaligned length,
oversize copies or out-of-range offsets refuse. Flash erase and write are
refused unless they stay inside the two-sector config window or the 8 KB
otadata window, stay sector or 4-byte aligned as required, and do not cross a
sector on write. The C5 port additionally restricts GPIO to pin 27 (XIAO user
LED), requires 4-byte-aligned flash length and destination, and uses ROM SPI
read, erase, write and unlock, USB Serial/JTAG TX, SYSTIMER, TIMG/LP watchdog
mute and LPPERI RNG. Radio is excluded (ADR 0004). Host tests link
`hal_host.c`; the spike links `spike/hal_c5.c`.

## Panic

Unexpected traps set a sticky panic reason and wait in `wfi`. They do not
reprint success, restart the faulting instruction as a healthy path, or clear
recovery evidence. Host tests cover the sticky refuse behaviour. The HIL image
still reprints `result=PASS` after a successful probe sequence so capture can
see the marker; that loop is idle, not panic.

## Compile profiles

`scripts/build-kernel-spike.sh` accepts `hil` (default), `production` or
`eval`. HIL defines `CIRVANE_HIL_SPIKE` and includes corrupt-config injection
plus the `result=PASS` reprint loop. Production defines
`CIRVANE_SPIKE_PRODUCTION`, prints `cirvane boot=ok`, then a quiet `cirvane>`
USB shell with no heartbeat. Eval defines `CIRVANE_SPIKE_EVAL` and emits
`cirvane-eval` recovery samples. Inject symbols and PASS reprint must be
absent from production and eval images. None of these is a complete product
kernel.

## Crash, evidence and quiet shell

Crash lines are `cirvane crash reason=<u32> mcause=<8 hex> mepc=<8 hex>`.
Evidence lines are `cirvane evidence svc=<0-7> epoch= gen= out= reason= rec=
health=` from the 16-byte record. Info is `cirvane info panic= services= slots=`.
Lines are at most 96 bytes. The production USB shell prompt is `cirvane>`.
Commands are `help`, `info`, `crash` and `evidence <0-7>`. Unknown, oversize
and out-of-range input print `cirvane refuse`. There is no periodic heartbeat
on that path. The shell is privileged and unauthenticated.

## Not in this increment

Power-cycle cold-start variance of the spike `boot=ok` path is in
`kernel-spike.json` (`boot_cold_kind=usb_power_cycle`, n=5, about 0.41-0.47 s)
beside JTAG warm resets (`boot_kind=jtag_warm_reset`, n=11, about 0.44 s).
Matched Nucleus class-1 n=30 wall-ms samples are in `matched-eval.json`
(`source=jtag_noinit`, median 2999, p95 4000) beside Cirvane eval ticks
(`stale_total=0`). Message and interrupt latency tails are in
`kernel-spike.json`. Radio/Wi-Fi and live user-mode `mret` remain open.
