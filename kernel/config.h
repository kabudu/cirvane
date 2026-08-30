/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Kernel-owned two-slot configuration journal. Highest valid CRC generation
 * wins. A failed verify-after-write does not publish. No NVS, no allocator.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CIRVANE_CFG_SLOTS 2
#define CIRVANE_CFG_SCHEMA 2
#define CIRVANE_CFG_HEARTBEAT_MIN 1000u
#define CIRVANE_CFG_HEARTBEAT_MAX 3600000u
#define CIRVANE_CFG_SUPERVISOR_MIN 100u
#define CIRVANE_CFG_SUPERVISOR_MAX 60000u
#define CIRVANE_CFG_LED_MAX 2u

#define CIRVANE_CFG_OK 0
#define CIRVANE_CFG_REFUSED -1
#define CIRVANE_CFG_INVALID -2

typedef struct {
    uint32_t schema_version;
    uint32_t heartbeat_ms;
    uint32_t supervisor_ms;
    uint8_t default_led_mode;
    uint8_t reserved[3];
} cirvane_cfg_t;

typedef struct {
    uint32_t generation;
    cirvane_cfg_t config;
    uint32_t crc;
} cirvane_cfg_record_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(cirvane_cfg_record_t) == 24, "config record must stay 24 bytes");
#endif

typedef struct {
    cirvane_cfg_record_t slots[CIRVANE_CFG_SLOTS];
    cirvane_cfg_t live;
    uint32_t generation;
} cirvane_cfg_journal_t;

void cirvane_cfg_reset(cirvane_cfg_journal_t *journal);
int cirvane_cfg_load(cirvane_cfg_journal_t *journal, cirvane_cfg_t *out);
int cirvane_cfg_commit(cirvane_cfg_journal_t *journal, const cirvane_cfg_t *cfg);
#if defined(CIRVANE_HIL_SPIKE) || defined(CIRVANE_CFG_TEST)
int cirvane_cfg_inject_corrupt(cirvane_cfg_journal_t *journal);
#endif
uint32_t cirvane_cfg_generation(const cirvane_cfg_journal_t *journal);
