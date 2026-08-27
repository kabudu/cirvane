# Architecture

## Minimal system

The current baseline is an ESP-IDF application using FreeRTOS as its scheduler and hardware integration substrate. Cirvane owns the service model, supervisor, bounded message bus, capability checks, configuration journal, power policy, shell and OTA policy. ESP-IDF owns boot, drivers, networking primitives, partitions and low-level update APIs. This baseline is implemented and preserved for migration and matched evaluation.

The target architecture is a clean-sheet Cirvane kernel that owns reset-to-runtime scheduling, typed messaging, capability decisions, bounded recovery transactions, a two-slot configuration journal and fail-closed dual-slot rollback selection. Vendor ROM, HAL, boot, radio, cryptographic verification and `otadata` writes may remain only behind an enumerated boundary that does not schedule on FreeRTOS or reinterpret kernel outcomes. Stage 2 has a portable kernel core and C5 HIL image for those policies. Durable flash-backed config, live ESP image verify, drivers beyond the spike probes, live user-mode isolation and matched evaluation are not implemented. ESP-IDF Wi-Fi remains an owner decision because the vendor adapter requires FreeRTOS.

## Current baseline components and invariants

| Component | Owner | Invariant | Failure path | Bound |
|---|---|---|---|---|
| Service manager | Cirvane | Stable service identity and explicit phase | Bounded restart, backoff, then degraded state | 8 services |
| Message bus | Cirvane | Typed messages are returned to the static pool | Drop is counted and visible | 32 slots, 24-byte payload |
| Configuration journal | Cirvane over NVS | Highest valid CRC generation wins | Fall back to other slot or defaults | 2 records |
| OTA policy | Cirvane over ESP-IDF | No boot selection before complete verification | Abort staging and keep current boot target | 2 app slots, 1 KiB copy block |
| Shell | Cirvane and ESP console | Bounded parsing and explicit errors | Reject malformed or unsupported input | ESP console line bound |
| Vendor substrate | ESP-IDF/FreeRTOS | Hardware and scheduler services match pinned SDK | Build or runtime error, never silently accepted | Pinned v6.0.2 baseline |

## Stage 2 kernel policy

These rows are the portable Cirvane kernel. They do not replace the Nucleus baseline until migration.

| Component | Owner | Invariant | Failure path | Bound |
|---|---|---|---|---|
| Configuration journal | Cirvane kernel | Highest valid CRC generation wins | Fall back to other slot or defaults; refuse malformed commit | 2 records, 24 bytes, RAM-backed |
| OTA selection policy | Cirvane kernel over a verify adapter | No boot selection before complete verification | Abort staging and keep current boot target | 2 app slots, 1 KiB copy block |

## Target release boundary

The target trusted computing base includes the Cirvane kernel and services, the enumerated Espressif ROM, boot, HAL, radio and cryptographic components, partition data and configured verification keys. FreeRTOS is excluded. Machine-mode kernel code owns isolation and recovery; user-mode services receive only declared capabilities and mapped resources. The exact hard-isolation claim remains bounded by the implemented PMP/PMA/APM configuration and vendor component audit. The local USB shell is privileged and unauthenticated.

## Resource and concurrency model

Services are statically registered and use fixed stacks and kernel-owned message slots. Work is periodic or explicitly queued; queues do not grow. Recovery transactions bind epochs, resource reclamation, capability leases and fixed-size evidence. Wi-Fi initialisation remains lazy if the feasibility gate admits it. OTA writes use bounded blocks. Retry and recovery decisions are kernel-owned and observable.

## Compatibility boundary

ESP-IDF v6.0.2 and the ESP32-C5 remain the baseline compatibility surface. The target kernel may preserve selected Espressif ROM, HAL or driver components through a narrow adapter, but must enumerate their privilege, memory, callback and scheduler assumptions. The Stage 2 portable core lives under `kernel/` with the C5 port in `kernel/spike`. It is not yet a complete production kernel. The dependency inventory is `docs/DEPENDENCY_INVENTORY.md`.
