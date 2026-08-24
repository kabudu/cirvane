# Architecture

## Minimal system

The current baseline is an ESP-IDF application using FreeRTOS as its scheduler and hardware integration substrate. Cirvane owns the service model, supervisor, bounded message bus, capability checks, configuration journal, power policy, shell and OTA policy. ESP-IDF owns boot, drivers, networking primitives, partitions and low-level update APIs. This baseline is implemented and preserved for migration and matched evaluation.

ADR 0002 changes the target architecture for the first release. The target is a clean-sheet Cirvane kernel that owns reset-to-runtime scheduling, typed messaging, capability decisions and bounded recovery transactions. Vendor ROM, HAL, boot, radio and cryptographic code may remain only behind an enumerated boundary that does not schedule on FreeRTOS or reinterpret kernel outcomes. The target architecture is planned, not implemented.

## Current baseline components and invariants

| Component | Owner | Invariant | Failure path | Bound |
|---|---|---|---|---|
| Service manager | Cirvane | Stable service identity and explicit phase | Bounded restart, backoff, then degraded state | 8 services |
| Message bus | Cirvane | Typed messages are returned to the static pool | Drop is counted and visible | 32 slots, 24-byte payload |
| Configuration journal | Cirvane over NVS | Highest valid CRC generation wins | Fall back to other slot or defaults | 2 records |
| OTA policy | Cirvane over ESP-IDF | No boot selection before complete verification | Abort staging and keep current boot target | 2 app slots, 1 KiB copy block |
| Shell | Cirvane and ESP console | Bounded parsing and explicit errors | Reject malformed or unsupported input | ESP console line bound |
| Vendor substrate | ESP-IDF/FreeRTOS | Hardware and scheduler services match pinned SDK | Build or runtime error, never silently accepted | Pinned v6.0.2 baseline |

## Target release boundary

The target trusted computing base includes the Cirvane kernel and services, the enumerated Espressif ROM, boot, HAL, radio and cryptographic components, partition data and configured verification keys. FreeRTOS is excluded. Machine-mode kernel code owns isolation and recovery; user-mode services receive only declared capabilities and mapped resources. The exact hard-isolation claim remains bounded by the implemented PMP/PMA/APM configuration and vendor component audit. The local USB shell is privileged and unauthenticated.

## Resource and concurrency model

Services are statically registered and use fixed stacks and kernel-owned message slots. Work is periodic or explicitly queued; queues do not grow. Recovery transactions bind epochs, resource reclamation, capability leases and fixed-size evidence. Wi-Fi initialisation remains lazy if the feasibility gate admits it. OTA writes use bounded blocks. Retry and recovery decisions are kernel-owned and observable.

## Compatibility boundary

ESP-IDF v6.0.2 and the ESP32-C5 remain the baseline compatibility surface. The target kernel may preserve selected Espressif ROM, HAL or driver components through a narrow adapter, but must enumerate their privilege, memory, callback and scheduler assumptions. No clean-sheet kernel or compatibility boundary is implemented yet.
