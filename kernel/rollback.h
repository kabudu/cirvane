/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Kernel-owned dual-slot rollback policy. Never select a boot slot until the
 * injected image verifier succeeds. Signature and ESP image parsing stay in
 * a vendor adapter; this module owns fail-closed selection.
 */

#pragma once

#include <stdint.h>

#define CIRVANE_OTA_SLOTS 2
#define CIRVANE_OTA_BLOCK 1024

#define CIRVANE_OTA_OK 0
#define CIRVANE_OTA_REFUSED -1

#define CIRVANE_OTA_EMPTY 0
#define CIRVANE_OTA_STAGING 1
#define CIRVANE_OTA_PENDING 2
#define CIRVANE_OTA_VALID 3
#define CIRVANE_OTA_INVALID 4
#define CIRVANE_OTA_ABORTED 5

typedef int (*cirvane_ota_verify_fn)(uint8_t slot, uint32_t length);

typedef struct {
    uint8_t boot_slot;
    uint8_t running_slot;
    uint8_t target_slot;
    uint8_t staging;
    uint8_t verify_ok;
    uint8_t states[CIRVANE_OTA_SLOTS];
    uint32_t expected_len;
    uint32_t written;
} cirvane_ota_t;

void cirvane_ota_reset(cirvane_ota_t *ota, uint8_t running);
int cirvane_ota_begin(cirvane_ota_t *ota, uint8_t target, uint32_t image_len);
int cirvane_ota_write(cirvane_ota_t *ota, uint32_t length);
int cirvane_ota_abort(cirvane_ota_t *ota);
int cirvane_ota_end(cirvane_ota_t *ota, cirvane_ota_verify_fn verify);
int cirvane_ota_select(cirvane_ota_t *ota);
int cirvane_ota_reboot(cirvane_ota_t *ota);
int cirvane_ota_confirm(cirvane_ota_t *ota);
int cirvane_ota_rollback(cirvane_ota_t *ota);
uint8_t cirvane_ota_boot_slot(const cirvane_ota_t *ota);
uint8_t cirvane_ota_state(const cirvane_ota_t *ota, uint8_t slot);
