# Operations

## Lifecycle and recovery

The preserved comparison build is deterministic against its pinned ESP-IDF baseline. Release builds use a separately pinned Cirvane kernel toolchain, an enumerated vendor dependency set and explicit production or HIL configuration. Production images exclude destructive diagnostics. Operators inspect device identity, services, resources, configuration and OTA state through explicit shell commands. A failed pending image rolls back; an accepted image requires explicit confirmation.

## Capacity and observability

Service count, stacks, message pool, payload size and OTA buffers are compile-time bounded. Queue drops, restarts, health, stack high-water marks and heap state are visible. Routine heartbeat telemetry remains below the default shell log level.

## Incident response

For update or runtime failure: preserve serial output and evidence, do not confirm a suspect image, allow rollback, capture `info`, `svc`, `res`, `bus` and `ota-status`, reproduce with the same build identity, and classify unsupported or unavailable evidence separately from pass. Key compromise requires revocation metadata and a new trusted key path before further OTA use.
