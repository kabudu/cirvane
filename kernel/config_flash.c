/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 */

#include "config_flash.h"
#include "hal.h"

static void copy_bytes(void *dst, const void *src, unsigned len)
{
    uint8_t *out = dst;
    const uint8_t *in = src;
    unsigned i;
    for (i = 0; i < len; i++) {
        out[i] = in[i];
    }
}

static int same_bytes(const void *a, const void *b, unsigned len)
{
    const uint8_t *x = a;
    const uint8_t *y = b;
    unsigned i;
    for (i = 0; i < len; i++) {
        if (x[i] != y[i]) {
            return 0;
        }
    }
    return 1;
}

static uint32_t slot_offset(unsigned slot)
{
    return CIRVANE_CFG_FLASH_BASE + slot * CIRVANE_FLASH_SECTOR;
}

int cirvane_cfg_flash_load(cirvane_cfg_journal_t *journal, cirvane_cfg_t *out)
{
    unsigned slot;

    if (journal == 0 || out == 0) {
        return CIRVANE_CFG_REFUSED;
    }
    for (slot = 0; slot < CIRVANE_CFG_SLOTS; slot++) {
        if (cirvane_hal_flash_read(slot_offset(slot), &journal->slots[slot],
                                   sizeof(journal->slots[slot])) !=
            CIRVANE_HAL_OK) {
            return CIRVANE_CFG_INVALID;
        }
    }
    return cirvane_cfg_load(journal, out);
}

int cirvane_cfg_flash_commit(cirvane_cfg_journal_t *journal,
                             const cirvane_cfg_t *cfg)
{
    cirvane_cfg_journal_t working;
    cirvane_cfg_record_t verify;
    unsigned slot;
    uint32_t offset;
    int rc;

    if (journal == 0) {
        return CIRVANE_CFG_REFUSED;
    }
    copy_bytes(&working, journal, sizeof(working));
    rc = cirvane_cfg_commit(&working, cfg);
    if (rc != CIRVANE_CFG_OK) {
        return rc;
    }
    slot = working.generation & 1u;
    offset = slot_offset(slot);
    if (cirvane_hal_flash_erase(offset, CIRVANE_FLASH_SECTOR) != CIRVANE_HAL_OK) {
        return CIRVANE_CFG_INVALID;
    }
    if (cirvane_hal_flash_write(offset, &working.slots[slot],
                                sizeof(working.slots[slot])) != CIRVANE_HAL_OK) {
        return CIRVANE_CFG_INVALID;
    }
    if (cirvane_hal_flash_read(offset, &verify, sizeof(verify)) !=
            CIRVANE_HAL_OK ||
        !same_bytes(&verify, &working.slots[slot], sizeof(verify))) {
        return CIRVANE_CFG_INVALID;
    }
    copy_bytes(journal, &working, sizeof(*journal));
    return CIRVANE_CFG_OK;
}

#if defined(CIRVANE_HIL_SPIKE) || defined(CIRVANE_CFG_TEST)
int cirvane_cfg_flash_inject_corrupt(cirvane_cfg_journal_t *journal)
{
    cirvane_cfg_t original;
    cirvane_cfg_t loaded;
    uint32_t original_generation;
    cirvane_cfg_record_t bad;
    unsigned slot;
    uint32_t offset;
    unsigned i;

    if (journal == 0) {
        return CIRVANE_CFG_REFUSED;
    }
    original_generation = journal->generation;
    copy_bytes(&original, &journal->live, sizeof(original));
    slot = (original_generation + 1u) & 1u;
    offset = slot_offset(slot);
    for (i = 0; i < sizeof(bad); i++) {
        ((uint8_t *)&bad)[i] = 0;
    }
    bad.generation = original_generation + 1u;
    copy_bytes(&bad.config, &original, sizeof(bad.config));
    bad.crc = 0xFFFFFFFFu;
    if (cirvane_hal_flash_erase(offset, CIRVANE_FLASH_SECTOR) != CIRVANE_HAL_OK) {
        return CIRVANE_CFG_INVALID;
    }
    if (cirvane_hal_flash_write(offset, &bad, sizeof(bad)) != CIRVANE_HAL_OK) {
        return CIRVANE_CFG_INVALID;
    }
    if (cirvane_cfg_flash_load(journal, &loaded) != CIRVANE_CFG_OK) {
        return CIRVANE_CFG_INVALID;
    }
    if (journal->generation != original_generation ||
        loaded.heartbeat_ms != original.heartbeat_ms ||
        loaded.supervisor_ms != original.supervisor_ms ||
        loaded.default_led_mode != original.default_led_mode) {
        return CIRVANE_CFG_INVALID;
    }
    return cirvane_cfg_flash_commit(journal, &original);
}
#endif
