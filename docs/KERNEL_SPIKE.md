# Kernel spike, invariants and ABI freeze

Stage 1 artefact retained as the ESP32-C5 port and HIL image. Syscalls 1-8 are
implemented by the portable Stage 2 core in `kernel/kernel.c`. That core is
still not a complete production kernel.

## What the spike proves

`kernel/spike` is a RAM-resident ESP32-C5 image with no FreeRTOS, no ESP-IDF
app_main and no allocator. The existing second-stage bootloader loads it. The
image includes two flash-mapped stub segments (app descriptor + IROM dummy)
because ESP32-C5 `unpack_load_app` requires exactly two MMU mappings; the spike
body still runs from SRAM. It must demonstrate:

- Reset entry, BSS clear and a deterministic panic/idle path (`wfi` on
  unexpected traps; USB reprint loop only after a successful probe sequence).
- Trap handling via `ecall`.
- A CLIC/INTMTX software interrupt dispatched through the kernel IRQ table.
- SYSTIMER monotonic advance.
- USB Serial/JTAG console output on the XIAO operator port.
- Static `.data` presence (allocator-free).
- ROM SPI flash read of the bootloader image magic.
- CSR probes of `misa`, `mstatus`, `pmpcfg0` and `pmpaddr0`. User-mode presence
  is reported from `misa.U`; a live `mret` into U-mode remains deferred.
- One recovery-transaction demonstration through `SYS_RTX_BIND`,
  `SYS_MSG_ALLOC` and `SYS_RTX_ADMIT`.
- Cooperative scheduling, typed send/recv and a software capability check.
- Two-slot CRC configuration fallback after a compile-gated corrupt injection.
- Dual-slot rollback policy refusing select after injected verify failure.
- Bounded HAL: GPIO 27 output readback, LPPERI entropy changing or non-zero,
  watchdog flashboot mute, ROM flash read and USB Serial/JTAG TX.

Machine-readable HIL evidence lives in `benchmarks/results/kernel-spike.json`.
A `result` of `pass` is required before spike checkboxes are closed. A
`blocked` or `fail` record is not qualification. Flashing uses app offset
`0x20000` (`ota_0` in `partitions_two_ota_large.csv`) and replaces the running
application; restore the Nucleus image afterwards. While the spike holds USB
Serial/JTAG, `esptool` RTS/hard-reset may not enter the stub. Recorded captures
use OpenOCD `program_esp` of `cirvane-spike.bin` at `0x20000`, then
`kernel_spike_hil.py` capture without `--flash`.

## Kernel invariants

1. Service count, message slots, evidence records, restart budgets and payload
   sizes are compile-time constants. No heap after boot.
2. A recovery transaction is the only path from admitted fault or deadline
   breach to a new service epoch.
3. While `recovering` is set, service health is `RECOVERING` and must not be
   reported as `OK`.
4. Timeout, nested admit, malformed bind/admit and pool exhaustion refuse or
   fail. They never become healthy success.
5. After a completed transaction, no in-use slot owned by that service may
   carry the previous epoch. Stale work is not deliverable.
6. Capability leases bound to the old epoch are revoked before health may
   become `OK` or `DEGRADED`.
7. Restart budget exhaustion yields `FAILED`, not `OK`.
8. Evidence is one 16-byte record per service, replaced in place, never grown.
9. FreeRTOS symbols and scheduler objects are absent from the spike and from
   any later image that claims clean-sheet kernel ownership.

## Forbidden failure-state collapses

- `FAULT -> OK` without epoch advance and reclaim.
- `RECOVERING -> OK` while slots from the old epoch remain deliverable.
- Nested admit interpreted as success.
- Budget exhaustion interpreted as restart.
- Transport or ROM success interpreted as recovery success.

## Syscall and message ABI

The C5 image calls the portable kernel. Numbers and fail-closed results stay
frozen. Argument layout is in `docs/KERNEL.md`.

| Number | Name | Effect |
|---:|---|---|
| 1 | `SYS_RTX_BIND` | Bind service, budget and initial lease. Refuse malformed. |
| 2 | `SYS_RTX_ADMIT` | Admit fault or deadline. Run the atomic transaction. |
| 3 | `SYS_MSG_ALLOC` | Allocate a slot tagged with the current epoch. Refuse if recovering, failed or exhausted. |
| 4 | `SYS_MSG_SEND` | Send only if the slot epoch matches the live service epoch. |
| 5 | `SYS_MSG_RECV` | Receive only current-epoch slots. |
| 6 | `SYS_MSG_FREE` | Return a slot to the static pool. |
| 7 | `SYS_CAP_GRANT` | Grant a lease to a bound service that is not recovering. |
| 8 | `SYS_CAP_REVOKE` | Revoke immediately; required internally during admit. |

Messages are 24-byte payloads in a 32-slot pool, matching the preserved
baseline ceilings unless a later ADR changes them with evidence.

## Trusted computing base ledger (spike)

| Component | Privilege | Role | Scheduling assumption |
|---|---|---|---|
| Cirvane reset, trap, spike C | Machine | Owns probes and recovery demo | Cooperative RR after probes; no preemption |
| Portable kernel and recovery model | Machine (linked in) | Syscalls, scheduler, panic, IRQ table | Cooperative; ticks must return |
| ESP32-C5 ROM second-stage caller + SPI flash ROM | ROM | Load image, optional flash read | None |
| USB Serial/JTAG MMIO | Machine MMIO | Operator console | None |
| SYSTIMER, CLIC, INTMTX, INTPRI, TIMG/LP WDT, GPIO, IO MUX, LPPERI RNG | Machine MMIO | Time, interrupt, watchdog mute, LED, entropy | None |
| ESP32-C5 ROM USB TX | ROM | Operator console character TX | None |
| ESP-IDF second-stage bootloader already in flash | Boot | Loads the app image | Not linked into the spike |
| FreeRTOS | Absent | n/a | Must remain absent |
| ESP-IDF Wi-Fi/libnet80211 | Not linked | n/a | Requires FreeRTOS if used later |

## Language and unsafe boundary

C11 for the portable kernel, recovery model and spike body. RISC-V assembly
only in `kernel/spike/start.S` for stack, BSS, `mtvec` and trap entry. MMIO is
volatile register access in `kernel/spike/kernel.c` and `kernel/spike/hal_c5.c`.
No Rust: the ESP-IDF GCC toolchain is already present and is the measurable
correctness path.

## Wi-Fi feasibility

See `docs/DEPENDENCY_INVENTORY.md`. Vendor Wi-Fi cannot run without FreeRTOS
scheduling through the published ESP-IDF adapter. That remains an owner
decision gate, not a hidden compatibility layer.
