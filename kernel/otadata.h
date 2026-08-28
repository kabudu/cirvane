/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Live ESP otadata adapter. Cirvane never writes otadata unless rollback
 * policy has already accepted a verified select. Signature parsing stays
 * outside this module.
 */

#pragma once

#include <stdint.h>

#define CIRVANE_OTADATA_OK 0
#define CIRVANE_OTADATA_REFUSED -1
#define CIRVANE_OTADATA_INVALID -2

#define CIRVANE_OTADATA_ENTRY 32
#define CIRVANE_OTA_IMG_NEW 0u
#define CIRVANE_OTA_IMG_PENDING 1u
#define CIRVANE_OTA_IMG_VALID 2u

typedef struct {
    uint32_t ota_seq;
    uint8_t seq_label[20];
    uint32_t ota_state;
    uint32_t crc;
} cirvane_ota_select_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(cirvane_ota_select_t) == CIRVANE_OTADATA_ENTRY,
               "otadata entry must stay 32 bytes");
#endif

uint32_t cirvane_otadata_seq_crc(uint32_t seq);
int cirvane_otadata_read(cirvane_ota_select_t entries[2]);
int cirvane_otadata_active_slot(uint8_t *slot);
int cirvane_otadata_select(uint8_t slot);
int cirvane_otadata_restore(const cirvane_ota_select_t entries[2]);
