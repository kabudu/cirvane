/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rollback.h"

static void memzero(void *ptr, unsigned len)
{
    uint8_t *bytes = ptr;
    unsigned i;
    for (i = 0; i < len; i++) {
        bytes[i] = 0;
    }
}

void cirvane_ota_reset(cirvane_ota_t *ota, uint8_t running)
{
    memzero(ota, sizeof(*ota));
    if (running >= CIRVANE_OTA_SLOTS) {
        running = 0;
    }
    ota->running_slot = running;
    ota->boot_slot = running;
    ota->states[running] = CIRVANE_OTA_VALID;
    ota->target_slot = (uint8_t)(1u - running);
}

int cirvane_ota_begin(cirvane_ota_t *ota, uint8_t target, uint32_t image_len)
{
    if (ota == 0 || target >= CIRVANE_OTA_SLOTS || target == ota->running_slot ||
        image_len == 0 || ota->staging) {
        return CIRVANE_OTA_REFUSED;
    }
    ota->target_slot = target;
    ota->expected_len = image_len;
    ota->written = 0;
    ota->verify_ok = 0;
    ota->staging = 1;
    ota->states[target] = CIRVANE_OTA_STAGING;
    return CIRVANE_OTA_OK;
}

int cirvane_ota_write(cirvane_ota_t *ota, uint32_t length)
{
    if (ota == 0 || !ota->staging || length == 0 || length > CIRVANE_OTA_BLOCK) {
        return CIRVANE_OTA_REFUSED;
    }
    if (ota->written + length > ota->expected_len) {
        return CIRVANE_OTA_REFUSED;
    }
    ota->written += length;
    return CIRVANE_OTA_OK;
}

int cirvane_ota_abort(cirvane_ota_t *ota)
{
    uint8_t target;

    if (ota == 0 || !ota->staging) {
        return CIRVANE_OTA_REFUSED;
    }
    target = ota->target_slot;
    ota->staging = 0;
    ota->verify_ok = 0;
    ota->written = 0;
    ota->expected_len = 0;
    ota->states[target] = CIRVANE_OTA_ABORTED;
    return CIRVANE_OTA_OK;
}

int cirvane_ota_end(cirvane_ota_t *ota, cirvane_ota_verify_fn verify)
{
    uint8_t target;

    if (ota == 0 || !ota->staging || verify == 0) {
        return CIRVANE_OTA_REFUSED;
    }
    target = ota->target_slot;
    ota->staging = 0;
    if (ota->written != ota->expected_len || verify(target, ota->written) != 0) {
        ota->verify_ok = 0;
        ota->states[target] = CIRVANE_OTA_INVALID;
        return CIRVANE_OTA_REFUSED;
    }
    ota->verify_ok = 1;
    ota->states[target] = CIRVANE_OTA_PENDING;
    return CIRVANE_OTA_OK;
}

int cirvane_ota_select(cirvane_ota_t *ota)
{
    if (ota == 0 || !ota->verify_ok ||
        ota->states[ota->target_slot] != CIRVANE_OTA_PENDING) {
        return CIRVANE_OTA_REFUSED;
    }
    ota->boot_slot = ota->target_slot;
    return CIRVANE_OTA_OK;
}

int cirvane_ota_reboot(cirvane_ota_t *ota)
{
    if (ota == 0 || ota->staging) {
        return CIRVANE_OTA_REFUSED;
    }
    ota->running_slot = ota->boot_slot;
    ota->verify_ok = 0;
    return CIRVANE_OTA_OK;
}

int cirvane_ota_confirm(cirvane_ota_t *ota)
{
    uint8_t running;

    if (ota == 0) {
        return CIRVANE_OTA_REFUSED;
    }
    running = ota->running_slot;
    if (ota->states[running] == CIRVANE_OTA_VALID) {
        return CIRVANE_OTA_OK;
    }
    if (ota->states[running] == CIRVANE_OTA_PENDING) {
        ota->states[running] = CIRVANE_OTA_VALID;
        return CIRVANE_OTA_OK;
    }
    return CIRVANE_OTA_REFUSED;
}

int cirvane_ota_rollback(cirvane_ota_t *ota)
{
    uint8_t previous;

    if (ota == 0 || ota->states[ota->running_slot] != CIRVANE_OTA_PENDING) {
        return CIRVANE_OTA_REFUSED;
    }
    previous = (uint8_t)(1u - ota->running_slot);
    if (ota->states[previous] != CIRVANE_OTA_VALID) {
        return CIRVANE_OTA_REFUSED;
    }
    ota->states[ota->running_slot] = CIRVANE_OTA_INVALID;
    ota->boot_slot = previous;
    ota->running_slot = previous;
    ota->verify_ok = 0;
    return CIRVANE_OTA_OK;
}

uint8_t cirvane_ota_boot_slot(const cirvane_ota_t *ota)
{
    return ota == 0 ? 0 : ota->boot_slot;
}

uint8_t cirvane_ota_state(const cirvane_ota_t *ota, uint8_t slot)
{
    if (ota == 0 || slot >= CIRVANE_OTA_SLOTS) {
        return CIRVANE_OTA_INVALID;
    }
    return ota->states[slot];
}
