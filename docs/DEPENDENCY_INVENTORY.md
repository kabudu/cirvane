# Dependency inventory

Enumerated vendor and Cirvane dependencies for the Stage 1 kernel spike and the
planned production kernel. Absence of a row means the component is not approved
for a clean-sheet image.

## Spike runtime (linked or MMIO)

| Component | Source | Licence | Privilege | Memory | Callbacks | Scheduler |
|---|---|---|---|---|---|---|
| Cirvane spike + kernel core | this repository | MIT | Machine | Internal SRAM only | Trap handler | Cooperative RR; ticks must return |
| `riscv32-esp-elf` GCC 15.2 / newlib not linked | Espressif toolchain | toolchain | Build only | n/a | n/a | n/a |
| ESP32-C5 ROM `esp_rom_spiflash_read` 0x40000160, `erase_sector` 0x40000154, `write` 0x4000015c, `unlock` 0x40000164 | Espressif ROM | ROM binary | ROM | ROM | None from spike besides the call | None |
| Cirvane `otadata.c` adapter | this repository | MIT | Machine | Two 4 KB copies at `0xF000` | None | None. Writes only after rollback policy; HIL restores backup |
| USB Serial/JTAG, SYSTIMER, CLIC, INTMTX, INTPRI, TIMG WDT, LP WDT, GPIO, IO MUX, LPPERI RNG | ESP32-C5 MMIO | hardware | Machine | Peripheral window | Interrupt via CLIC | None |
| ESP32-C5 ROM USB TX (`usb_serial_device_tx_one_char` 0x40000ac4, flush 0x40000ac0) | Espressif ROM | ROM binary | ROM | ROM | None besides the call | None |
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

**Owner decision (2026-08-27, ADR 0004):** Stage 2 uses a first workflow without
Wi-Fi. See `docs/DECISIONS/0004-stage2-workflow-without-wifi.md`. Radio remains
excluded from the kernel image. A later isolated radio boundary needs a new
ADR and inventory row.

**Owner decision (ADR 0005, 2026-08-28):** the first labelled Cirvane firmware
keeps the FreeRTOS baseline, including Wi-Fi. The clean-sheet kernel remains
research without on-chip ESP-IDF Wi-Fi until a later owner decision on a
narrower kernel release or an isolated radio boundary.

The spike does not implement a hidden FreeRTOS compatibility scheduler.

## Production kernel (planned, not implemented)

May retain ROM, bootloader, enumerated HAL, cryptographic ROM and, only after
the owner decision above, a radio boundary that does not schedule on FreeRTOS.
Each retained binary must gain a row in this inventory before it is linked.
