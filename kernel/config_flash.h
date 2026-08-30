/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Flash adapter for the two-slot configuration journal. Each slot occupies
 * one 4096-byte sector at CIRVANE_CFG_FLASH_BASE. Otadata is not written.
 */

#pragma once

#include "config.h"

int cirvane_cfg_flash_load(cirvane_cfg_journal_t *journal, cirvane_cfg_t *out);
int cirvane_cfg_flash_commit(cirvane_cfg_journal_t *journal,
                             const cirvane_cfg_t *cfg);
#if defined(CIRVANE_HIL_SPIKE) || defined(CIRVANE_CFG_TEST)
int cirvane_cfg_flash_inject_corrupt(cirvane_cfg_journal_t *journal);
#endif
