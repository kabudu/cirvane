# Operations

## Lifecycle and recovery

The current Cirvane firmware is deterministic against its pinned ESP-IDF baseline. Kernel spike overlays are destructive app-slot captures used only for research; operators must restore a known Cirvane ESP-IDF/FreeRTOS image afterwards. Historical Nucleus images remain comparison artefacts, not the current product. After this identity increment the NVS namespace is `cirvane`, which resets the preserved boot counter on boards that still hold the `nucleus` namespace. Production images exclude destructive diagnostics. Operators inspect device identity, services, resources, configuration and OTA state through explicit shell commands. A failed pending image rolls back; an accepted image requires explicit confirmation.

## Capacity and observability

Service count, stacks, message pool, payload size and OTA buffers are compile-time bounded. Queue drops, restarts, health, stack high-water marks and heap state are visible. Routine heartbeat telemetry remains below the default shell log level.

## Wi-Fi connectivity

Run `wifi connect` to scan for nearby networks, select a numbered result and enter its password at the masked prompt. Run `wifi connect "<ssid>"` for a known or hidden network. A connection attempt waits for association and DHCP for at most 15 seconds. `wifi status` reports the selected network and assigned IPv4 address; `wifi disconnect` disconnects and clears the driver's RAM-only station configuration.

Cirvane does not persist Wi-Fi credentials. A reboot therefore returns to the disconnected state. Passwords are not accepted as command arguments, printed, logged or written into committed evidence. The USB shell remains a privileged physical interface; an operator with device or debugger access is inside the current trust boundary.

## Incident response

For update or runtime failure: preserve serial output and evidence, do not confirm a suspect image, allow rollback, capture `info`, `svc`, `res`, `bus` and `ota-status`, reproduce with the same build identity, and classify unsupported or unavailable evidence separately from pass. Key compromise requires revocation metadata and a new trusted key path before further OTA use.
