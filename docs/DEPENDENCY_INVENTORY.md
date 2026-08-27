# Dependency inventory

Enumerated vendor and Cirvane dependencies for the Stage 1 kernel spike and the
planned production kernel. Absence of a row means the component is not approved
for a clean-sheet image.

## Spike runtime (linked or MMIO)

| Component | Source | Licence | Privilege | Memory | Callbacks | Scheduler |
|---|---|---|---|---|---|---|
| Cirvane spike + kernel core | this repository | MIT | Machine | Internal SRAM only | Trap handler | Cooperative RR; ticks must return |
| `riscv32-esp-elf` GCC 15.2 / newlib not linked | Espressif toolchain | toolchain | Build only | n/a | n/a | n/a |
| ESP32-C5 ROM `esp_rom_spiflash_read` at 0x40000160 | Espressif ROM | ROM binary | ROM | ROM | None from spike besides the call | None |
| USB Serial/JTAG, SYSTIMER, CLIC, INTMTX, INTPRI, TIMG WDT, LP WDT | ESP32-C5 MMIO | hardware | Machine | Peripheral window | Interrupt via CLIC | None |
| ESP-IDF second-stage bootloader (already flashed) | ESP-IDF v6.0.2 | Apache-2.0 | Boot | Bootloader SRAM, then released | Jumps to app entry | Not in spike image |

## Explicitly excluded from the spike

| Component | Reason |
|---|---|
| FreeRTOS | Clean-sheet ownership; symbols must be absent |
| ESP-IDF `app_main` / `esp_system` startup | Starts FreeRTOS |
| libc heap (`malloc`) | Allocator-free invariant |
| `esp_wifi`, `esp_netif`, `esp_event` | Require FreeRTOS tasks and queues |
| Newlib | Spike is `-nostdlib` |

## Wi-Fi and radio feasibility

ESP-IDF documents FreeRTOS as an integrated system component. The ESP32-C5
Wi-Fi adapter (`components/esp_wifi/esp32c5/esp_adapter.c`) binds
`xTaskCreate`, queue send/recv, event groups, `vTaskDelay` and `malloc`.
`esp_wifi_init` creates the Wi-Fi driver task. Linking that library while
claiming a FreeRTOS-free kernel would hide FreeRTOS below Cirvane.

**Finding (2026-08-25):** required Wi-Fi scan workflow cannot use the vendor
library without FreeRTOS as the scheduler. A mailbox to a radio coprocessor
that does not let vendor code schedule Cirvane outcomes is not implemented.

**Owner decision required** before Stage 2 radio work:

1. First developer release without Wi-Fi, or
2. A genuinely isolated non-kernel radio boundary with its own evidence, or
3. Stop the clean-sheet kernel path and keep the FreeRTOS baseline.

The spike does not implement a hidden FreeRTOS compatibility scheduler.

## Production kernel (planned, not implemented)

May retain ROM, bootloader, enumerated HAL, cryptographic ROM and, only after
the owner decision above, a radio boundary that does not schedule on FreeRTOS.
Each retained binary must gain a row in this inventory before it is linked.
