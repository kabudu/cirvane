# Prior art matrix

| System | Established mechanism | Boundary and overlap | Difference remaining to establish | Source |
|---|---|---|---|---|
| FreeRTOS | Embedded scheduler, tasks, queues, timers and synchronization | Implemented migration and matched-evaluation baseline | Cirvane must establish kernel-owned recovery semantics without hiding FreeRTOS | https://www.freertos.org/Documentation/00-Overview |
| ESP-IDF | ESP32 boot, drivers, networking and OTA APIs | Current hardware and update substrate | Cirvane policy and recovery sit above it | https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/ |
| Zephyr | Configurable RTOS, device model, networking and update ecosystem | Close general embedded OS category | Bounded recovery mechanism requires matched evidence | https://docs.zephyrproject.org/latest/ |
| Apache NuttX | POSIX-oriented embedded RTOS | Alternative general embedded operating system | Cirvane is not currently a POSIX competitor | https://nuttx.apache.org/docs/latest/ |
| Tock | Rust embedded OS with process isolation and capsules | Close capability, isolation and microkernel research surface | Cirvane must distinguish atomic bounded recovery semantics rather than language or MPU isolation alone | https://www.tockos.org/documentation/design/ |
| RIOT | Open-source IoT operating system | Close constrained-device and networking category | Current Cirvane scope is one ESP32-C5 board | https://www.riot-os.org/ |

This initial matrix bounds current claims; it is not a completed systematic literature or patent review.
