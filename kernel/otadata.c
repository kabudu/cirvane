/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 */

#include "otadata.h"
#include "hal.h"

static uint32_t crc32_le_n(uint32_t crc, const uint8_t *data, unsigned len)
{
    unsigned i;
    unsigned bit;

    /* Match esp_rom_crc32_le: invert, update, invert. */
    crc = ~crc;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xedb88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

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

uint32_t cirvane_otadata_seq_crc(uint32_t seq)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)seq;
    bytes[1] = (uint8_t)(seq >> 8);
    bytes[2] = (uint8_t)(seq >> 16);
    bytes[3] = (uint8_t)(seq >> 24);
    return crc32_le_n(0xffffffffu, bytes, 4);
}

static int entry_valid(const cirvane_ota_select_t *e)
{
    return e->ota_seq != 0xffffffffu &&
           e->crc == cirvane_otadata_seq_crc(e->ota_seq) &&
           e->ota_state != 3u && e->ota_state != 4u;
}

static uint32_t entry_offset(unsigned idx)
{
    return CIRVANE_OTA_FLASH_BASE + idx * CIRVANE_FLASH_SECTOR;
}

int cirvane_otadata_read(cirvane_ota_select_t entries[2])
{
    unsigned i;

    if (entries == 0) {
        return CIRVANE_OTADATA_REFUSED;
    }
    for (i = 0; i < 2; i++) {
        if (cirvane_hal_flash_read(entry_offset(i), &entries[i],
                                   sizeof(entries[i])) != CIRVANE_HAL_OK) {
            return CIRVANE_OTADATA_INVALID;
        }
    }
    return CIRVANE_OTADATA_OK;
}

int cirvane_otadata_active_slot(uint8_t *slot)
{
    cirvane_ota_select_t entries[2];
    int valid0;
    int valid1;
    uint32_t seq;

    if (slot == 0) {
        return CIRVANE_OTADATA_REFUSED;
    }
    if (cirvane_otadata_read(entries) != CIRVANE_OTADATA_OK) {
        return CIRVANE_OTADATA_INVALID;
    }
    valid0 = entry_valid(&entries[0]);
    valid1 = entry_valid(&entries[1]);
    if (valid0 && valid1) {
        seq = entries[0].ota_seq >= entries[1].ota_seq ? entries[0].ota_seq
                                                       : entries[1].ota_seq;
    } else if (valid1) {
        seq = entries[1].ota_seq;
    } else if (valid0) {
        seq = entries[0].ota_seq;
    } else {
        *slot = 0;
        return CIRVANE_OTADATA_OK;
    }
    *slot = (uint8_t)((seq - 1u) & 1u);
    return CIRVANE_OTADATA_OK;
}

static int write_entry(unsigned idx, const cirvane_ota_select_t *entry)
{
    cirvane_ota_select_t verify;
    uint32_t offset = entry_offset(idx);

    if (cirvane_hal_flash_erase(offset, CIRVANE_FLASH_SECTOR) != CIRVANE_HAL_OK) {
        return CIRVANE_OTADATA_INVALID;
    }
    if (cirvane_hal_flash_write(offset, entry, sizeof(*entry)) != CIRVANE_HAL_OK) {
        return CIRVANE_OTADATA_INVALID;
    }
    if (cirvane_hal_flash_read(offset, &verify, sizeof(verify)) != CIRVANE_HAL_OK ||
        !same_bytes(&verify, entry, sizeof(verify))) {
        return CIRVANE_OTADATA_INVALID;
    }
    return CIRVANE_OTADATA_OK;
}

int cirvane_otadata_select(uint8_t slot)
{
    cirvane_ota_select_t entries[2];
    cirvane_ota_select_t next;
    uint8_t active;
    unsigned dest;
    uint32_t seq0 = 0;
    uint32_t seq1 = 0;
    uint32_t highest = 0;

    if (slot >= 2u) {
        return CIRVANE_OTADATA_REFUSED;
    }
    if (cirvane_otadata_active_slot(&active) != CIRVANE_OTADATA_OK ||
        cirvane_otadata_read(entries) != CIRVANE_OTADATA_OK) {
        return CIRVANE_OTADATA_INVALID;
    }
    if (entry_valid(&entries[0])) {
        seq0 = entries[0].ota_seq;
        highest = seq0;
    }
    if (entry_valid(&entries[1])) {
        seq1 = entries[1].ota_seq;
        if (seq1 >= highest) {
            highest = seq1;
        }
    }
    dest = (highest == seq0) ? 1u : 0u;
    copy_bytes(&next, &entries[dest], sizeof(next));
    next.ota_seq = highest + 1u;
    while (((next.ota_seq - 1u) & 1u) != slot) {
        next.ota_seq += 1u;
    }
    next.ota_state = CIRVANE_OTA_IMG_VALID;
    next.crc = cirvane_otadata_seq_crc(next.ota_seq);
    return write_entry(dest, &next);
}

int cirvane_otadata_restore(const cirvane_ota_select_t entries[2])
{
    unsigned i;
    int rc;

    if (entries == 0) {
        return CIRVANE_OTADATA_REFUSED;
    }
    for (i = 0; i < 2; i++) {
        rc = write_entry(i, &entries[i]);
        if (rc != CIRVANE_OTADATA_OK) {
            return rc;
        }
    }
    return CIRVANE_OTADATA_OK;
}
