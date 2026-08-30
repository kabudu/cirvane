/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host driver for kernel-owned config journal and dual-slot rollback policy.
 */

#include "config.h"
#include "config_flash.h"
#include "otadata.h"
#include "rollback.h"
#include "hal.h"

#include <stdio.h>

static int failures;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        failures++;
    }
}

static int verify_pass(uint8_t slot, uint32_t length)
{
    (void)slot;
    (void)length;
    return 0;
}

static int verify_fail(uint8_t slot, uint32_t length)
{
    (void)slot;
    (void)length;
    return -1;
}

static void test_config_flash_persist_and_fallback(void)
{
    cirvane_cfg_journal_t journal;
    cirvane_cfg_t cfg;
    cirvane_cfg_t loaded;

    expect(cirvane_hal_init() == CIRVANE_HAL_OK, "hal init");
    cirvane_cfg_reset(&journal);
    expect(cirvane_cfg_flash_load(&journal, &loaded) == CIRVANE_CFG_OK,
           "empty flash load");
    expect(cirvane_cfg_generation(&journal) == 0, "empty gen 0");
    cfg = loaded;
    cfg.heartbeat_ms = 2000;
    expect(cirvane_cfg_flash_commit(&journal, &cfg) == CIRVANE_CFG_OK,
           "flash commit");
    expect(cirvane_cfg_generation(&journal) == 1, "flash gen 1");
    cirvane_cfg_reset(&journal);
    expect(cirvane_cfg_flash_load(&journal, &loaded) == CIRVANE_CFG_OK,
           "reload flash");
    expect(cirvane_cfg_generation(&journal) == 1, "persisted gen");
    expect(loaded.heartbeat_ms == 2000, "persisted heartbeat");
    expect(cirvane_cfg_flash_inject_corrupt(&journal) == CIRVANE_CFG_OK,
           "flash corrupt fallback");
    expect(cirvane_cfg_generation(&journal) == 2, "flash restored");
    cirvane_cfg_reset(&journal);
    expect(cirvane_cfg_flash_load(&journal, &loaded) == CIRVANE_CFG_OK,
           "reload after fallback");
    expect(loaded.heartbeat_ms == 2000, "kept after fallback");
    expect(cirvane_cfg_flash_commit(&journal, 0) == CIRVANE_CFG_REFUSED,
           "null flash commit");
}

static void test_otadata_select_restore_and_refuse(void)
{
    cirvane_ota_select_t backup[2];
    cirvane_ota_select_t after[2];
    uint8_t slot = 9;
    uint8_t restored = 9;

    expect(cirvane_hal_init() == CIRVANE_HAL_OK, "otadata hal");
    expect(cirvane_otadata_seq_crc(1u) == 0x4743989au, "rom crc seq 1");
    expect(cirvane_otadata_read(backup) == CIRVANE_OTADATA_OK, "read empty");
    expect(cirvane_otadata_active_slot(&slot) == CIRVANE_OTADATA_OK, "active");
    expect(slot == 0, "default slot 0");
    expect(cirvane_otadata_select(0) == CIRVANE_OTADATA_OK, "select 0");
    expect(cirvane_otadata_active_slot(&slot) == CIRVANE_OTADATA_OK, "active after");
    expect(slot == 0, "still slot 0");
    expect(cirvane_otadata_select(2) == CIRVANE_OTADATA_REFUSED, "bad slot");
    expect(cirvane_otadata_restore(backup) == CIRVANE_OTADATA_OK, "restore");
    expect(cirvane_otadata_read(after) == CIRVANE_OTADATA_OK, "reread");
    expect(after[0].ota_seq == backup[0].ota_seq, "seq0 restored");
    expect(cirvane_otadata_active_slot(&restored) == CIRVANE_OTADATA_OK,
           "active restored");
    expect(restored == 0, "restored slot");
}

static void test_config_commit_and_corrupt_fallback(void)
{
    cirvane_cfg_journal_t journal;
    cirvane_cfg_t cfg;
    cirvane_cfg_t loaded;

    cirvane_cfg_reset(&journal);
    expect(cirvane_cfg_load(&journal, &loaded) == CIRVANE_CFG_OK, "empty load");
    expect(cirvane_cfg_generation(&journal) == 0, "defaults gen 0");
    expect(loaded.heartbeat_ms == 10000, "default heartbeat");
    cfg = loaded;
    cfg.heartbeat_ms = 2000;
    expect(cirvane_cfg_commit(&journal, &cfg) == CIRVANE_CFG_OK, "commit");
    expect(cirvane_cfg_generation(&journal) == 1, "gen 1");
    expect(cirvane_cfg_inject_corrupt(&journal) == CIRVANE_CFG_OK, "corrupt fallback");
    expect(cirvane_cfg_generation(&journal) == 2, "restored redundancy");
    expect(cirvane_cfg_load(&journal, &loaded) == CIRVANE_CFG_OK, "reload");
    expect(loaded.heartbeat_ms == 2000, "kept committed value");
}

static void test_config_refuses_malformed(void)
{
    cirvane_cfg_journal_t journal;
    cirvane_cfg_t cfg;
    uint32_t gen;

    cirvane_cfg_reset(&journal);
    cirvane_cfg_load(&journal, &cfg);
    gen = cirvane_cfg_generation(&journal);
    cfg.heartbeat_ms = 99;
    expect(cirvane_cfg_commit(&journal, &cfg) == CIRVANE_CFG_REFUSED, "low heartbeat");
    cfg.heartbeat_ms = 2000;
    cfg.schema_version = 99;
    expect(cirvane_cfg_commit(&journal, &cfg) == CIRVANE_CFG_REFUSED, "bad schema");
    expect(cirvane_cfg_generation(&journal) == gen, "gen unchanged");
    expect(cirvane_cfg_commit(&journal, 0) == CIRVANE_CFG_REFUSED, "null");
}

static void test_config_picks_highest_valid_generation(void)
{
    cirvane_cfg_journal_t journal;
    cirvane_cfg_t cfg;
    cirvane_cfg_t loaded;

    cirvane_cfg_reset(&journal);
    cirvane_cfg_load(&journal, &cfg);
    cfg.heartbeat_ms = 3000;
    cirvane_cfg_commit(&journal, &cfg);
    cfg.heartbeat_ms = 4000;
    cirvane_cfg_commit(&journal, &cfg);
    expect(cirvane_cfg_load(&journal, &loaded) == CIRVANE_CFG_OK, "load both");
    expect(loaded.heartbeat_ms == 4000, "newest wins");
    expect(cirvane_cfg_generation(&journal) == 2, "gen 2");
}

static void test_ota_rejects_corrupt_before_select(void)
{
    cirvane_ota_t ota;
    uint8_t boot_before;

    cirvane_ota_reset(&ota, 0);
    boot_before = cirvane_ota_boot_slot(&ota);
    expect(cirvane_ota_begin(&ota, 1, 2048) == CIRVANE_OTA_OK, "begin");
    expect(cirvane_ota_write(&ota, CIRVANE_OTA_BLOCK) == CIRVANE_OTA_OK, "block 1");
    expect(cirvane_ota_write(&ota, CIRVANE_OTA_BLOCK) == CIRVANE_OTA_OK, "block 2");
    expect(cirvane_ota_end(&ota, verify_fail) == CIRVANE_OTA_REFUSED, "verify fail");
    expect(cirvane_ota_select(&ota) == CIRVANE_OTA_REFUSED, "no select");
    expect(cirvane_ota_boot_slot(&ota) == boot_before, "boot unchanged");
    expect(cirvane_ota_state(&ota, 1) == CIRVANE_OTA_INVALID, "invalid slot");
}

static void test_ota_truncation_and_oversize_write(void)
{
    cirvane_ota_t ota;
    uint8_t boot_before;

    cirvane_ota_reset(&ota, 0);
    boot_before = cirvane_ota_boot_slot(&ota);
    expect(cirvane_ota_begin(&ota, 1, 2048) == CIRVANE_OTA_OK, "begin");
    expect(cirvane_ota_write(&ota, CIRVANE_OTA_BLOCK) == CIRVANE_OTA_OK, "partial");
    expect(cirvane_ota_end(&ota, verify_pass) == CIRVANE_OTA_REFUSED, "truncated");
    expect(cirvane_ota_boot_slot(&ota) == boot_before, "trunc boot");
    cirvane_ota_reset(&ota, 0);
    cirvane_ota_begin(&ota, 1, 1024);
    expect(cirvane_ota_write(&ota, 1025) == CIRVANE_OTA_REFUSED, "oversize chunk");
    expect(cirvane_ota_write(&ota, 0) == CIRVANE_OTA_REFUSED, "zero chunk");
}

static void test_ota_abort_leaves_boot(void)
{
    cirvane_ota_t ota;
    uint8_t boot_before;

    cirvane_ota_reset(&ota, 0);
    boot_before = cirvane_ota_boot_slot(&ota);
    cirvane_ota_begin(&ota, 1, 1024);
    cirvane_ota_write(&ota, 1024);
    expect(cirvane_ota_abort(&ota) == CIRVANE_OTA_OK, "abort");
    expect(cirvane_ota_select(&ota) == CIRVANE_OTA_REFUSED, "select after abort");
    expect(cirvane_ota_boot_slot(&ota) == boot_before, "abort boot");
    expect(cirvane_ota_state(&ota, 1) == CIRVANE_OTA_ABORTED, "aborted");
}

static void test_ota_pending_confirm_and_unhealthy_rollback(void)
{
    cirvane_ota_t ota;

    cirvane_ota_reset(&ota, 0);
    expect(cirvane_ota_begin(&ota, 1, 1024) == CIRVANE_OTA_OK, "begin ok");
    expect(cirvane_ota_write(&ota, 1024) == CIRVANE_OTA_OK, "write ok");
    expect(cirvane_ota_end(&ota, verify_pass) == CIRVANE_OTA_OK, "signed ok");
    expect(cirvane_ota_select(&ota) == CIRVANE_OTA_OK, "select");
    expect(cirvane_ota_boot_slot(&ota) == 1, "boot pending");
    expect(cirvane_ota_reboot(&ota) == CIRVANE_OTA_OK, "reboot into pending");
    expect(cirvane_ota_confirm(&ota) == CIRVANE_OTA_OK, "confirm");
    expect(cirvane_ota_state(&ota, 1) == CIRVANE_OTA_VALID, "confirmed");
    expect(cirvane_ota_confirm(&ota) == CIRVANE_OTA_OK, "already valid");

    cirvane_ota_reset(&ota, 0);
    cirvane_ota_begin(&ota, 1, 1024);
    cirvane_ota_write(&ota, 1024);
    cirvane_ota_end(&ota, verify_pass);
    cirvane_ota_select(&ota);
    cirvane_ota_reboot(&ota);
    expect(cirvane_ota_rollback(&ota) == CIRVANE_OTA_OK, "rollback");
    expect(cirvane_ota_boot_slot(&ota) == 0, "rolled back");
    expect(cirvane_ota_state(&ota, 1) == CIRVANE_OTA_INVALID, "failed pending");
}

static void test_ota_refuses_nested_and_same_slot(void)
{
    cirvane_ota_t ota;

    cirvane_ota_reset(&ota, 0);
    expect(cirvane_ota_begin(&ota, 0, 1024) == CIRVANE_OTA_REFUSED, "same slot");
    expect(cirvane_ota_begin(&ota, 1, 0) == CIRVANE_OTA_REFUSED, "zero len");
    expect(cirvane_ota_begin(&ota, 1, 1024) == CIRVANE_OTA_OK, "first begin");
    expect(cirvane_ota_begin(&ota, 1, 1024) == CIRVANE_OTA_REFUSED, "nested begin");
    expect(cirvane_ota_select(&ota) == CIRVANE_OTA_REFUSED, "select while staging");
}

int main(void)
{
    test_config_commit_and_corrupt_fallback();
    test_config_flash_persist_and_fallback();
    test_otadata_select_restore_and_refuse();
    test_config_refuses_malformed();
    test_config_picks_highest_valid_generation();
    test_ota_rejects_corrupt_before_select();
    test_ota_truncation_and_oversize_write();
    test_ota_abort_leaves_boot();
    test_ota_pending_confirm_and_unhealthy_rollback();
    test_ota_refuses_nested_and_same_slot();
    if (failures != 0) {
        fprintf(stderr, "%d config/rollback checks failed\n", failures);
        return 1;
    }
    puts("config rollback checks passed");
    return 0;
}
