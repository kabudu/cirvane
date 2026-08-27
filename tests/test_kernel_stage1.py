#!/usr/bin/env python3
"""Stage 1 kernel spike and claim-boundary contract checks."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
KERNEL = ROOT / "kernel"


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


class KernelStage1Contract(unittest.TestCase):
    def test_spike_source_has_no_freertos_or_allocator(self):
        for path in KERNEL.rglob("*"):
            if not path.is_file() or path.suffix not in {".c", ".h", ".S", ".ld"}:
                continue
            text = path.read_text(encoding="utf-8")
            self.assertNotRegex(text, r"freertos/", msg=str(path))
            self.assertNotRegex(text, r"\b(malloc|calloc|realloc)\s*\(", msg=str(path))
            self.assertNotIn("vTaskStartScheduler", text)

    def test_recovery_bounds_are_compile_time_constants(self):
        header = read("kernel/recovery/recovery.h")
        self.assertRegex(header, r"#define\s+CIRVANE_RTX_SERVICE_COUNT\s+8\b")
        self.assertRegex(header, r"#define\s+CIRVANE_RTX_SLOT_COUNT\s+32\b")
        self.assertRegex(header, r"#define\s+CIRVANE_RTX_EVIDENCE_SIZE\s+16\b")
        self.assertIn("CIRVANE_HEALTH_RECOVERING", header)

    def test_required_systems_are_compared(self):
        matrix = read("docs/PRIOR_ART_MATRIX.md")
        for name in (
            "FreeRTOS",
            "Zephyr",
            "Tock",
            "RIOT",
            "NuttX",
            "seL4",
            "Microkit",
            "Hubris",
            "Minix",
            "VirtuosoNext",
        ):
            self.assertIn(name, matrix)
        self.assertIn("Search log", matrix)
        self.assertIn("2026-08-25", matrix)

    def test_wifi_feasibility_is_explicit(self):
        inventory = read("docs/DEPENDENCY_INVENTORY.md")
        self.assertIn("FreeRTOS", inventory)
        self.assertIn("Wi-Fi", inventory)
        self.assertIn("owner decision", inventory.lower())
        self.assertIn("esp_adapter.c", inventory)

    def test_claims_remain_provisional(self):
        novelty = read("docs/NOVELTY.md").lower()
        self.assertIn("hypothesis", novelty)
        self.assertIn("does not claim definitive worldwide novelty", novelty)
        self.assertIn("no implemented or proven novel kernel", novelty)
        adr = read("docs/DECISIONS/0003-bounded-recovery-transaction.md")
        self.assertIn("Accepted for planning", adr)
        self.assertIn("provisional", adr.lower())

    def test_matched_evaluation_is_pre_registered(self):
        protocol = read("docs/MATCHED_EVALUATION.md").lower()
        for token in (
            "stale-work",
            "recovery latency",
            "stopping rule",
            "missing-data",
            "freertos",
            "resource ceiling",
        ):
            self.assertIn(token, protocol)

    def test_spike_docs_and_adr_exist(self):
        for relative in (
            "docs/KERNEL_SPIKE.md",
            "docs/KERNEL.md",
            "docs/DEPENDENCY_INVENTORY.md",
            "docs/MATCHED_EVALUATION.md",
            "docs/DECISIONS/0003-bounded-recovery-transaction.md",
        ):
            self.assertTrue((ROOT / relative).is_file())


    def test_syscall_abi_numbers_remain_frozen(self):
        header = read("kernel/kernel.h")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_RTX_BIND\s+1u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_RTX_ADMIT\s+2u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_MSG_ALLOC\s+3u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_MSG_SEND\s+4u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_MSG_RECV\s+5u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_MSG_FREE\s+6u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_CAP_GRANT\s+7u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SYS_CAP_REVOKE\s+8u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_MSG_PAYLOAD_MAX\s+24\b")
        kernel_doc = read("docs/KERNEL.md")
        self.assertIn("software-only", kernel_doc)
        self.assertIn("starvation", kernel_doc.lower())

    def test_hal_bounds_are_frozen(self):
        header = read("kernel/hal.h")
        self.assertRegex(header, r"#define\s+CIRVANE_GPIO_LED\s+27u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_GPIO_LIMIT\s+32u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_FLASH_READ_MAX\s+256u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_FLASH_WRITE_MAX\s+256u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_FLASH_SECTOR\s+4096u\b")
        self.assertRegex(header, r"#define\s+CIRVANE_CFG_FLASH_BASE\s+0x7FE000u\b")
        self.assertIn("Radio and network are excluded", header)
        self.assertNotIn("esp_wifi", read("kernel/spike/hal_c5.c"))
        self.assertNotIn("hal_host.c", read("scripts/build-kernel-spike.sh"))
        self.assertIn("hal_c5.c", read("scripts/build-kernel-spike.sh"))
        plan = read("docs/PRODUCTISATION_COMPLETION_PLAN.md")
        self.assertIn(
            "- [ ] Implement the minimum UART, GPIO, timer, flash, watchdog, entropy and radio",
            plan,
        )

    def test_config_and_rollback_bounds_are_frozen(self):
        cfg = read("kernel/config.h")
        ota = read("kernel/rollback.h")
        self.assertRegex(cfg, r"#define\s+CIRVANE_CFG_SLOTS\s+2\b")
        self.assertRegex(ota, r"#define\s+CIRVANE_OTA_SLOTS\s+2\b")
        self.assertRegex(ota, r"#define\s+CIRVANE_OTA_BLOCK\s+1024\b")
        self.assertIn("CIRVANE_HIL_SPIKE", read("kernel/config.c"))
        self.assertNotIn("nvs_", read("kernel/config.c"))
        self.assertNotIn("esp_ota_", read("kernel/rollback.c"))
        self.assertIn("config_flash.c", read("scripts/build-kernel-spike.sh"))
        self.assertIn("CIRVANE_CFG_FLASH_BASE", read("kernel/config_flash.c"))
        self.assertNotIn("otadata", read("kernel/config_flash.c"))

    def test_spike_profiles_gate_destructive_diagnostics(self):
        build = read("scripts/build-kernel-spike.sh")
        ci = read("scripts/ci-local.sh")
        spike = read("kernel/spike/kernel.c")
        self.assertIn('profile="${2:-${CIRVANE_SPIKE_PROFILE:-hil}}"', build)
        self.assertIn("CIRVANE_SPIKE_PRODUCTION", build)
        self.assertIn("kernel-spike-ci-prod", ci)
        self.assertIn("hil_run_probes", spike)
        self.assertIn("cirvane boot=ok", spike)
        self.assertIn("result=PASS", spike)
        self.assertIn(
            "- [x] Add production and HIL profiles",
            read("docs/PRODUCTISATION_COMPLETION_PLAN.md"),
        )

    def test_obs_and_shell_bounds_are_frozen(self):
        header = read("kernel/obs.h")
        self.assertRegex(header, r"#define\s+CIRVANE_OBS_LINE_MAX\s+96\b")
        self.assertRegex(header, r"#define\s+CIRVANE_SHELL_CMD_MAX\s+32\b")
        self.assertIn("cirvane>", header)
        self.assertNotIn("heartbeat", read("kernel/obs.c"))
        self.assertIn("obs.c", read("scripts/build-kernel-spike.sh"))
        self.assertIn("test_kernel_obs.py", read("scripts/ci-local.sh"))
        self.assertIn(
            "- [x] Define stable crash and evidence formats",
            read("docs/PRODUCTISATION_COMPLETION_PLAN.md"),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
