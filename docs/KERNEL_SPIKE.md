# Kernel spike, invariants and ABI freeze

Stage 1 artefact. The production Cirvane kernel is not implemented here.

## What the spike proves

`kernel/spike` is a RAM-resident ESP32-C5 image with no FreeRTOS, no ESP-IDF
app_main and no allocator. The existing second-stage bootloader loads it. The
image includes two flash-mapped stub segments (app descriptor + IROM dummy)
because ESP32-C5 `unpack_load_app` requires exactly two MMU mappings; the spike
body still runs from SRAM. It must demonstrate:

- Reset entry, BSS clear and a deterministic panic/idle path (`wfi` loop).
- Trap handling via `ecall`.
- A CLIC/INTMTX software interrupt.
- SYSTIMER monotonic advance.
- USB Serial/JTAG console output on the XIAO operator port.
- Static `.data` presence (allocator-free).
- ROM SPI flash read of the bootloader image magic.
- CSR probes of `misa`, `mstatus`, `pmpcfg0` and `pmpaddr0`. User-mode presence
  is reported from `misa.U`; a live `mret` into U-mode is Stage 2 work.
- One recovery-transaction demonstration using the portable model.

Machine-readable HIL evidence lives in `benchmarks/results/kernel-spike.json`.
A `result` of `pass` is required before Stage 1 spike checkboxes are closed. A
`blocked` or `fail` record is not qualification. Flashing uses app offset
`0x20000` (`ota_0` in `partitions_two_ota_large.csv`) and replaces the running
application; restore the Nucleus image afterwards.

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

## Syscall and message ABI (frozen, not implemented as syscalls in the spike)

The spike calls the portable C model directly. Stage 2 must preserve these
numbers and fail-closed results.

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
| Cirvane reset, trap, spike C | Machine | Owns probes and recovery demo | None; single hart, no scheduler |
| Portable recovery model | Machine (linked in) | State machine | None |
| ESP32-C5 ROM second-stage caller + SPI flash ROM | ROM | Load image, optional flash read | None |
| USB Serial/JTAG MMIO | Machine MMIO | Operator console | None |
| SYSTIMER, CLIC, INTMTX, INTPRI, TIMG/LP WDT | Machine MMIO | Time, interrupt, watchdog mute | None |
| ESP-IDF second-stage bootloader already in flash | Boot | Loads the app image | Not linked into the spike |
| FreeRTOS | Absent | n/a | Must remain absent |
| ESP-IDF Wi-Fi/libnet80211 | Not linked | n/a | Requires FreeRTOS if used later |

## Language and unsafe boundary

C11 for the recovery model and spike body. RISC-V assembly only in
`kernel/spike/start.S` for stack, BSS, `mtvec` and trap entry. MMIO is volatile
register access in `kernel/spike/kernel.c`. No Rust in Stage 1: the ESP-IDF
GCC toolchain is already present and is the measurable correctness path.

## Wi-Fi feasibility

See `docs/DEPENDENCY_INVENTORY.md`. Vendor Wi-Fi cannot run without FreeRTOS
scheduling through the published ESP-IDF adapter. That is an owner decision
gate for Stage 2, not a hidden compatibility layer.
