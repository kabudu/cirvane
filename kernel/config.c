/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

static void memzero(void *ptr, unsigned len)
{
    uint8_t *bytes = ptr;
    unsigned i;
    for (i = 0; i < len; i++) {
        bytes[i] = 0;
    }
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

static uint32_t crc32_le(const uint8_t *data, unsigned len)
{
    uint32_t crc = 0xffffffffu;
    unsigned i;
    unsigned bit;

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
    return crc ^ 0xffffffffu;
}

static uint32_t record_crc(const cirvane_cfg_record_t *record)
{
    return crc32_le((const uint8_t *)record,
                    (unsigned)((const uint8_t *)&record->crc - (const uint8_t *)record));
}

static bool cfg_in_range(const cirvane_cfg_t *cfg)
{
    return cfg->schema_version == CIRVANE_CFG_SCHEMA &&
           cfg->heartbeat_ms >= CIRVANE_CFG_HEARTBEAT_MIN &&
           cfg->heartbeat_ms <= CIRVANE_CFG_HEARTBEAT_MAX &&
           cfg->supervisor_ms >= CIRVANE_CFG_SUPERVISOR_MIN &&
           cfg->supervisor_ms <= CIRVANE_CFG_SUPERVISOR_MAX &&
           cfg->default_led_mode <= CIRVANE_CFG_LED_MAX &&
           cfg->reserved[0] == 0 && cfg->reserved[1] == 0 &&
           cfg->reserved[2] == 0;
}

static bool record_valid(const cirvane_cfg_record_t *record)
{
    return cfg_in_range(&record->config) && record->crc == record_crc(record);
}

static void cfg_defaults(cirvane_cfg_t *cfg)
{
    memzero(cfg, sizeof(*cfg));
    cfg->schema_version = CIRVANE_CFG_SCHEMA;
    cfg->heartbeat_ms = 10000;
    cfg->supervisor_ms = 1000;
    cfg->default_led_mode = 2;
}

void cirvane_cfg_reset(cirvane_cfg_journal_t *journal)
{
    memzero(journal, sizeof(*journal));
    cfg_defaults(&journal->live);
    journal->generation = 0;
}

int cirvane_cfg_load(cirvane_cfg_journal_t *journal, cirvane_cfg_t *out)
{
    bool valid[CIRVANE_CFG_SLOTS];
    int selected;

    if (journal == 0 || out == 0) {
        return CIRVANE_CFG_REFUSED;
    }

    valid[0] = record_valid(&journal->slots[0]);
    valid[1] = record_valid(&journal->slots[1]);
    if (valid[0] && valid[1]) {
        selected = journal->slots[1].generation > journal->slots[0].generation ? 1
                                                                               : 0;
    } else if (valid[1]) {
        selected = 1;
    } else if (valid[0]) {
        selected = 0;
    } else {
        cfg_defaults(&journal->live);
        journal->generation = 0;
        copy_bytes(out, &journal->live, sizeof(*out));
        return CIRVANE_CFG_OK;
    }

    copy_bytes(&journal->live, &journal->slots[selected].config,
               sizeof(journal->live));
    journal->generation = journal->slots[selected].generation;
    copy_bytes(out, &journal->live, sizeof(*out));
    return CIRVANE_CFG_OK;
}

int cirvane_cfg_commit(cirvane_cfg_journal_t *journal, const cirvane_cfg_t *cfg)
{
    cirvane_cfg_record_t record;
    unsigned slot;
    cirvane_cfg_record_t verify;

    if (journal == 0 || cfg == 0 || !cfg_in_range(cfg)) {
        return CIRVANE_CFG_REFUSED;
    }

    memzero(&record, sizeof(record));
    record.generation = journal->generation + 1u;
    copy_bytes(&record.config, cfg, sizeof(record.config));
    record.crc = record_crc(&record);
    slot = record.generation & 1u;
    copy_bytes(&journal->slots[slot], &record, sizeof(record));
    copy_bytes(&verify, &journal->slots[slot], sizeof(verify));
    if (!record_valid(&verify) || verify.generation != record.generation) {
        return CIRVANE_CFG_INVALID;
    }
    copy_bytes(&journal->live, &record.config, sizeof(journal->live));
    journal->generation = record.generation;
    return CIRVANE_CFG_OK;
}

#if defined(CIRVANE_HIL_SPIKE) || defined(CIRVANE_CFG_TEST)
int cirvane_cfg_inject_corrupt(cirvane_cfg_journal_t *journal)
{
    cirvane_cfg_record_t corrupt;
    unsigned slot;
    cirvane_cfg_t loaded;
    uint32_t original_generation;
    cirvane_cfg_t original;

    if (journal == 0) {
        return CIRVANE_CFG_REFUSED;
    }
    original_generation = journal->generation;
    copy_bytes(&original, &journal->live, sizeof(original));
    memzero(&corrupt, sizeof(corrupt));
    corrupt.generation = original_generation + 1u;
    copy_bytes(&corrupt.config, &original, sizeof(corrupt.config));
    corrupt.crc = record_crc(&corrupt) ^ 1u;
    slot = corrupt.generation & 1u;
    copy_bytes(&journal->slots[slot], &corrupt, sizeof(corrupt));
    if (cirvane_cfg_load(journal, &loaded) != CIRVANE_CFG_OK) {
        return CIRVANE_CFG_INVALID;
    }
    if (journal->generation != original_generation ||
        loaded.heartbeat_ms != original.heartbeat_ms ||
        loaded.supervisor_ms != original.supervisor_ms ||
        loaded.default_led_mode != original.default_led_mode) {
        return CIRVANE_CFG_INVALID;
    }
    return cirvane_cfg_commit(journal, &original);
}
#endif

uint32_t cirvane_cfg_generation(const cirvane_cfg_journal_t *journal)
{
    return journal == 0 ? 0 : journal->generation;
}
