# Architecture

## Minimal system

The current implementation is an ESP-IDF application using FreeRTOS as its scheduler and hardware integration substrate. Cirvane owns the service model, supervisor, bounded message bus, capability checks, configuration journal, power policy, shell and OTA policy. ESP-IDF owns boot, drivers, networking primitives, partitions and low-level update APIs.

This is the least complex design that preserves ESP32-C5 radio and USB support while making failure and resource behaviour explicit. A future custom kernel must enter through a separate architecture decision and matched validation; it is not implied by the current layering.

## Components and invariants

| Component | Owner | Invariant | Failure path | Bound |
|---|---|---|---|---|
| Service manager | Cirvane | Stable service identity and explicit phase | Bounded restart, backoff, then degraded state | 8 services |
| Message bus | Cirvane | Typed messages are returned to the static pool | Drop is counted and visible | 32 slots, 24-byte payload |
| Configuration journal | Cirvane over NVS | Highest valid CRC generation wins | Fall back to other slot or defaults | 2 records |
| OTA policy | Cirvane over ESP-IDF | No boot selection before complete verification | Abort staging and keep current boot target | 2 app slots, 1 KiB copy block |
| Shell | Cirvane and ESP console | Bounded parsing and explicit errors | Reject malformed or unsupported input | ESP console line bound |
| Vendor substrate | ESP-IDF/FreeRTOS | Hardware and scheduler services match pinned SDK | Build or runtime error, never silently accepted | Pinned v6.0.2 baseline |

## Trust boundary

The trusted computing base includes Cirvane firmware, ESP-IDF/FreeRTOS, Espressif ROM and radio components, bootloader, partition data and configured verification keys. Capability masks constrain cooperative service behaviour but do not isolate arbitrary native code. The local USB shell is privileged and unauthenticated.

## Resource and concurrency model

Services are statically registered and use static task stacks and mailboxes. Work is periodic or explicitly queued; queues do not grow. Wi-Fi initialisation is lazy. OTA writes use bounded blocks. Retry and recovery decisions are supervisor-owned and observable.

## Compatibility boundary

ESP-IDF v6.0.2 and the ESP32-C5 are the current compatibility surface. A future kernel may preserve selected ESP-IDF drivers through a narrow compatibility enclave, but no such design is implemented or claimed.
