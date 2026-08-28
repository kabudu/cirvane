#!/usr/bin/env python3
"""Offline contract checks for Cirvane (ESP-IDF/FreeRTOS); no board or credentials required."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CirvaneContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.defaults = (ROOT / "sdkconfig.defaults").read_text()
        cls.hil_defaults = (ROOT / "sdkconfig.hil.defaults").read_text()
        cls.header = (ROOT / "main" / "cirvane_os.h").read_text()
        cls.runtime = (ROOT / "main" / "cirvane_os.c").read_text()

    def enabled(self, symbol):
        self.assertRegex(self.defaults, rf"(?m)^{re.escape(symbol)}=y$")

    def test_ota_is_signed_dual_slot_with_rollback(self):
        for symbol in (
            "CONFIG_PARTITION_TABLE_TWO_OTA_LARGE",
            "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE",
            "CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT",
            "CONFIG_SECURE_SIGNED_APPS_ECDSA_V2_SCHEME",
            "CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT",
        ):
            self.enabled(symbol)

    def test_runtime_observability_and_power_are_enabled(self):
        for symbol in (
            "CONFIG_FREERTOS_USE_TRACE_FACILITY",
            "CONFIG_ESP_TASK_WDT_INIT",
            "CONFIG_ESP_TASK_WDT_PANIC",
            "CONFIG_PM_ENABLE",
            "CONFIG_COMPILER_STACK_CHECK_MODE_STRONG",
        ):
            self.enabled(symbol)

    def test_all_bounded_limits_are_compile_time_constants(self):
        expected = {
            "CIRVANE_MAX_SERVICES": 8,
            "CIRVANE_MSG_POOL_SLOTS": 32,
            "CIRVANE_MSG_PAYLOAD_MAX": 24,
        }
        for name, value in expected.items():
            self.assertRegex(self.header, rf"#define\s+{name}\s+{value}\b")
        self.assertIn("xTaskCreateStatic", self.runtime)
        self.assertIn("xQueueCreateStatic", self.runtime)
        self.assertIn("#define CIRVANE_TASK_STACK_BYTES 4096", self.runtime)
        self.assertNotRegex(self.runtime, r"\b(malloc|calloc|realloc)\s*\(")

    def test_transactional_config_has_two_slots_crc_and_commit(self):
        self.assertIn("config_record_t records[2]", self.runtime)
        self.assertIn("esp_crc32_le", self.runtime)
        self.assertIn("nvs_commit", self.runtime)
        self.assertIn("verify.generation != record.generation", self.runtime)

    def test_signing_key_is_not_embedded_in_source(self):
        self.assertNotIn("BEGIN PRIVATE KEY", self.defaults)
        self.assertEqual(
            re.findall(r'CONFIG_SECURE_BOOT_SIGNING_KEY="([^"]+)"', self.defaults),
            ["secure_boot_signing_key.pem"],
        )

    def test_ota_hil_path_uses_verified_bounded_copy(self):
        for call in (
            "esp_image_verify",
            "esp_ota_begin",
            "esp_ota_write",
            "esp_ota_end",
            "esp_ota_set_boot_partition",
        ):
            self.assertIn(call, self.runtime)
        self.assertIn("static uint8_t s_ota_copy_block[1024]", self.runtime)
        self.assertIn("esp_ota_abort(handle)", self.runtime)

    def test_wifi_initialisation_is_lazy_and_retryable(self):
        main = (ROOT / "main" / "cirvane.c").read_text()
        self.assertLess(main.index("static void run_wifi_scan"), main.index("esp_wifi_init(&cfg)"))
        for stage in ("WIFI_INIT_NETIF", "WIFI_INIT_EVENT_LOOP", "WIFI_INIT_DRIVER"):
            self.assertIn(stage, main)

    def test_shell_numbers_are_strict_and_bounded(self):
        main = (ROOT / "main" / "cirvane.c").read_text()
        self.assertIn("parse_u32_arg", main)
        self.assertIn("errno == ERANGE", main)
        self.assertIn("*end != '\\0'", main)
        self.assertIn("CIRVANE_MAX_SLEEP_MS", self.header)
        self.assertIn("wake_after_ms > CIRVANE_MAX_SLEEP_MS", self.runtime)

    def test_periodic_heartbeat_does_not_obscure_shell_prompt(self):
        main = (ROOT / "main" / "cirvane.c").read_text()
        self.assertIn('ESP_LOGD(TAG, "heartbeat up=', main)
        self.assertNotIn('ESP_LOGI(TAG, "heartbeat up=', main)

    def test_current_product_identity_is_cirvane(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("project(cirvane)", cmake)
        self.assertNotIn("project(nucleus)", cmake)
        self.assertFalse((ROOT / "main" / "nucleus.c").exists())
        main = (ROOT / "main" / "cirvane.c").read_text()
        self.assertIn('repl_cfg.prompt = "cirvane> "', main)
        self.assertNotIn("nucleus>", main)
        ci = (ROOT / "scripts" / "ci-local.sh").read_text()
        self.assertIn("build/ci/cirvane.bin", ci)
        self.assertNotIn("nucleus.bin", ci)

    def test_adversarial_hil_paths_are_compile_gated(self):
        main = (ROOT / "main" / "cirvane.c").read_text()
        matched = (ROOT / "sdkconfig.matched-eval.defaults").read_text()
        self.assertIn("CONFIG_CIRVANE_HIL_DIAGNOSTICS=y", self.hil_defaults)
        self.assertNotIn("CONFIG_CIRVANE_HIL_DIAGNOSTICS=y", self.defaults)
        self.assertNotIn("CONFIG_ESP_CONSOLE_NONE=y", self.hil_defaults)
        self.assertIn("CONFIG_ESP_CONSOLE_NONE=y", matched)
        self.assertIn("CONFIG_ESP_CONSOLE_SECONDARY_NONE=y", matched)
        self.assertIn("CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y", matched)
        self.assertIn("CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=n", matched)
        self.assertNotIn("CONFIG_ESP_CONSOLE_NONE=y", self.defaults)
        self.assertGreaterEqual(main.count("#if CONFIG_CIRVANE_HIL_DIAGNOSTICS"), 2)
        self.assertIn("ota-reject-corrupt", main)
        self.assertIn("config-corrupt-test", main)
        self.assertIn("err != ESP_OK && boot_unchanged", self.runtime)
        self.assertIn("hil_matched_class1", main)
        self.assertIn("s_cirvane_matched", main)
        self.assertIn("__NOINIT_ATTR", main)

    def test_matched_eval_contains_both_images(self):
        import json

        data = json.loads(
            (ROOT / "benchmarks" / "results" / "matched-eval.json").read_text()
        )
        self.assertEqual(data["result"], "pass")
        self.assertEqual(data["cirvane_stale_total"], 0)
        self.assertGreaterEqual(data["cirvane"]["0"]["ticks"]["n"], 30)
        self.assertGreaterEqual(data["nucleus"]["stats_ms"]["n"], 30)
        self.assertTrue(data["differentiated"])
        joined = " ".join(data["incomparable"])
        self.assertIn("Wi-Fi (ADR 0004)", joined)
        self.assertIn("ticks vs wall ms", joined)

    def test_kernel_spike_records_warm_and_cold_boots(self):
        import json

        data = json.loads(
            (ROOT / "benchmarks" / "results" / "kernel-spike.json").read_text()
        )
        self.assertEqual(data["result"], "pass")
        self.assertEqual(data["boot_kind"], "jtag_warm_reset")
        self.assertGreaterEqual(len(data["boot_s"]), 11)
        self.assertEqual(data["boot_cold_kind"], "usb_power_cycle")
        self.assertGreaterEqual(len(data["boot_cold_s"]), 5)
        self.assertEqual(data["latency"]["n"], 30)

    def test_matched_hil_uses_quiet_unsigned_bootloader(self):
        tool = (ROOT / "benchmarks" / "tools" / "matched_eval_hil.py").read_text()
        self.assertIn("quiet bootloader", tool)
        self.assertIn("JTAG CPU reset", tool)
        self.assertIn("chip RST", tool)
        self.assertIn("sdkconfig.matched-eval.defaults", tool)


if __name__ == "__main__":
    unittest.main(verbosity=2)
