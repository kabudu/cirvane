# Product specification

## Problem and users

Cirvane serves embedded developers who need a small device runtime that remains understandable and recoverable under service failure, malformed operator input and interrupted updates. The first supported device is the Seeed Studio XIAO ESP32-C5; broader hardware support is not yet claimed.

## Product result

Cirvane boots into a privileged local shell, starts a fixed set of statically allocated services, exposes bounded operational telemetry, performs asynchronous device work through typed messages, persists configuration transactionally and supports signed dual-slot firmware rollback.

## Behavioural invariants

- Registered services and message storage never exceed compile-time bounds.
- A service fault cannot silently become a healthy result.
- Configuration commits either verify in the alternate slot or leave the previous valid generation recoverable.
- An update cannot become the selected boot image before all configured authenticity and policy checks pass.
- Bytes already exposed through a public boundary are never retrospectively reclassified as verified.
- Production builds contain no destructive HIL commands.

## Supported workflow

1. Build a signed production image with ESP-IDF v6.0.2.
2. Flash through USB Serial/JTAG.
3. Operate and inspect the device through the Cirvane shell.
4. Diagnose services, resources, configuration and OTA state through explicit commands.
5. Stage authenticated updates when OTA transport is implemented, boot pending, confirm health or roll back.

## Non-goals for the initial developer release

- A novel kernel, hard real-time certification or safety certification.
- Hardware-rooted Secure Boot, flash encryption or irreversible eFuse provisioning on the only development board.
- Energy superiority claims without calibrated external instrumentation.
- Remote shell administration, multi-user authorization or general-purpose process execution.
- Independent penetration-test or formal-verification claims.

## Initial release success

The initial developer release requires consistent Cirvane identity, passing local CI, a signed production build, real-board functional and rollback evidence, authenticated OTA transport with adversarial tests, complete release documentation and explicit deferral notices for energy, hardware provisioning and independent penetration testing.
