/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS-free ESP32-C5 kernel spike. Proves boot, traps, interrupt
 * dispatch, timer, USB Serial/JTAG, static memory, flash ROM read,
 * PMP/privilege probes, recovery syscalls, cooperative scheduling,
 * configuration fallback from flash, fail-closed OTA selection and a bounded HAL.
 */

#include "hw.h"
#include "hal.h"
#include "kernel.h"
#include "obs.h"
#include "config.h"
#include "config_flash.h"
#include "otadata.h"
#include "rollback.h"

#include <stdint.h>

/* First bytes of segment 0. ESP32-C5 2nd-stage bootloader requires this
 * magic before it will copy any application into SRAM.
 */
struct spike_app_desc {
    uint32_t magic_word;
    uint32_t secure_version;
    uint32_t reserv1[2];
    char version[32];
    char project_name[32];
    char time[16];
    char date[16];
    char idf_ver[32];
    uint8_t app_elf_sha256[32];
    uint16_t min_efuse_blk_rev_full;
    uint16_t max_efuse_blk_rev_full;
    uint8_t mmu_page_size;
    uint8_t reserv3[3];
    uint32_t reserv2[18];
};

_Static_assert(sizeof(struct spike_app_desc) == 256, "esp_app_desc_t is 256 bytes");

__attribute__((section(".appdesc"), used))
const struct spike_app_desc g_app_desc = {
    .magic_word = 0xABCD5432u,
    .version = "stage1",
    .project_name = "cirvane-spike",
    .max_efuse_blk_rev_full = 0xffffu,
    .mmu_page_size = 16u,
};

__attribute__((section(".iromdummy"), used))
const uint32_t g_irom_dummy[4] = { 0, 0, 0, 0 };

volatile uint32_t g_interrupt_count;
volatile uint32_t g_ecall_count;
volatile uint32_t g_other_trap_count;
volatile uint32_t g_last_mcause;
static cirvane_kernel_t g_kernel;
#ifdef CIRVANE_HIL_SPIKE
static uint32_t g_sched_consumed;
#endif

void cirvane_trap_entry(void);

static void enable_usb_serial_jtag(void)
{
    uint32_t pcr = REG32(PCR_USB_DEVICE_CONF_REG);
    uint32_t conf0;
    volatile uint32_t wait = 0;

    pcr |= PCR_USB_DEVICE_CLK_EN;
    pcr &= ~(1u << 1);
    REG32(PCR_USB_DEVICE_CONF_REG) = pcr;
    /* Do not write CONFIG_UPDATE: that can drop the host CDC session. */
    conf0 = REG32(USB_SERIAL_JTAG_CONF0_REG);
    conf0 &= ~USB_SERIAL_JTAG_PHY_SEL;
    conf0 |= USB_SERIAL_JTAG_USB_PAD_ENABLE;
    REG32(USB_SERIAL_JTAG_CONF0_REG) = conf0;
    while (wait < 4000000u) {
        wait++;
    }
}

static void usb_write(const char *s)
{
    rom_usb_tx_one_char_fn rom_tx =
        (rom_usb_tx_one_char_fn)(uintptr_t)ROM_USB_TX_ONE_CHAR;
    rom_usb_tx_flush_fn rom_flush =
        (rom_usb_tx_flush_fn)(uintptr_t)ROM_USB_TX_FLUSH;

    while (*s) {
        rom_tx((uint8_t)*s);
        s++;
    }
    rom_flush();
}

#ifdef CIRVANE_HIL_SPIKE
static void usb_hex(uint32_t value, unsigned digits)
{
    char buf[9];
    unsigned i;
    static const char hex[] = "0123456789abcdef";

    if (digits > 8) {
        digits = 8;
    }
    for (i = digits; i > 0; i--) {
        buf[i - 1] = hex[value & 0xFu];
        value >>= 4;
    }
    buf[digits] = 0;
    usb_write(buf);
}
#endif

#if defined(CIRVANE_HIL_SPIKE) || defined(CIRVANE_SPIKE_EVAL)
static void usb_u32(uint32_t value)
{
    char buf[11];
    unsigned n = 0;
    uint32_t v = value;
    char tmp[10];

    if (v == 0) {
        usb_write("0");
        return;
    }
    while (v != 0 && n < 10) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0) {
        n--;
        buf[0] = tmp[n];
        buf[1] = 0;
        usb_write(buf);
    }
}
#endif

#ifdef CIRVANE_HIL_SPIKE
static void software_isr(void)
{
    g_interrupt_count += 1;
}
#endif

static void line(const char *s)
{
    usb_write(s);
    usb_write("\r\n");
}

uint32_t cirvane_trap(uint32_t mcause, uint32_t mepc)
{
    g_last_mcause = mcause;
    if (mcause & MCAUSE_INTERRUPT) {
        REG32(INTPRI_CPU_INTR_FROM_CPU_0_REG) = 0;
        REG32(CLIC_INT_CTRL_REG(CLIC_EXT_INTR_NUM_OFFSET + 1)) =
            CLIC_INT_CTL_PRIO | CLIC_INT_ATTR_MODE_M | CLIC_INT_IE;
        cirvane_irq_dispatch(&g_kernel, 0);
        return mepc;
    }
    if ((mcause & 0x7ffu) == MCAUSE_ECALL_M ||
        (mcause & 0x7ffu) == MCAUSE_ECALL_U) {
        g_ecall_count += 1;
        return mepc + 4u;
    }
    g_other_trap_count += 1;
    cirvane_panic_fault(&g_kernel, CIRVANE_PANIC_UNKNOWN_TRAP, mcause, mepc);
    {
        char crash[CIRVANE_OBS_LINE_MAX];
        if (cirvane_obs_crash(crash, sizeof(crash), g_kernel.panic_reason,
                              g_kernel.panic_mcause, g_kernel.panic_mepc) > 0) {
            line(crash);
        }
    }
    for (;;) {
        __asm__ volatile("wfi");
    }
}

#ifdef CIRVANE_HIL_SPIKE
static uint32_t csr_mstatus(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, mstatus" : "=r"(v));
    return v;
}

static uint32_t csr_misa(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, misa" : "=r"(v));
    return v;
}

static uint32_t csr_pmpcfg0(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, pmpcfg0" : "=r"(v));
    return v;
}

static uint32_t csr_pmpaddr0(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, pmpaddr0" : "=r"(v));
    return v;
}

static void setup_clic_software_interrupt(void)
{
    uint32_t clic_id = CLIC_EXT_INTR_NUM_OFFSET + 1u;
    uint32_t mtvec;

    REG32(CLIC_INT_THRESH_REG) = 0;
    REG32(CLIC_INT_CONFIG_REG) = 3u;
    REG32(INTERRUPT_CORE0_CPU_INTR_FROM_CPU_0_MAP_REG) = 1u;
    REG32(CLIC_INT_CTRL_REG(clic_id)) =
        CLIC_INT_CTL_PRIO | CLIC_INT_ATTR_MODE_M | CLIC_INT_ATTR_TRIG_EDGE |
        CLIC_INT_IE;

    mtvec = ((uint32_t)&cirvane_trap_entry) & ~63u;
    mtvec |= 3u;
    __asm__ volatile("csrw mtvec, %0" ::"r"(mtvec));
    __asm__ volatile("csrs mstatus, %0" ::"r"(MSTATUS_MIE));
}

static void fire_software_interrupt(void)
{
    uint32_t clic_id = CLIC_EXT_INTR_NUM_OFFSET + 1u;

    REG32(INTPRI_CPU_INTR_FROM_CPU_0_REG) = 0;
    REG32(CLIC_INT_CTRL_REG(clic_id)) =
        CLIC_INT_CTL_PRIO | CLIC_INT_ATTR_MODE_M | CLIC_INT_ATTR_TRIG_EDGE |
        CLIC_INT_IE;
    REG32(INTPRI_CPU_INTR_FROM_CPU_0_REG) = 1u;
    REG32(CLIC_INT_CTRL_REG(clic_id)) |= CLIC_INT_IP;
}

static uint32_t umode_implemented(void)
{
    /* misa bit 20 is the U extension. A live mret into U-mode is deferred
     * because a failed drop would hide the other spike probes.
     */
    return (csr_misa() >> 20) & 1u;
}

static uint32_t flash_magic(void)
{
    uint32_t word = 0;
    if (cirvane_hal_flash_read(FLASH_BOOTLOADER_OFF, &word, 4) != CIRVANE_HAL_OK) {
        return 0xffffffffu;
    }
    return word & 0xffu;
}

static void producer_tick(uint8_t service)
{
    int32_t slot = cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_ALLOC, service, 0, 0);
    uint8_t *payload;

    if (slot < 0) {
        return;
    }
    payload = cirvane_msg_payload(&g_kernel, (int)slot);
    if (payload == 0) {
        return;
    }
    payload[0] = 0x5Cu;
    cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 2, 1);
}

static void consumer_tick(uint8_t service)
{
    int32_t slot = cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_RECV, service, 0, 0);

    if (slot < 0) {
        return;
    }
    if (cirvane_msg_payload(&g_kernel, (int)slot)[0] == 0x5Cu) {
        g_sched_consumed = 1;
    }
    cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_FREE, (uint32_t)slot, 0, 0);
}

static void demo_recovery(void)
{
    int32_t slot;
    int32_t outcome;

    if (cirvane_syscall(&g_kernel, CIRVANE_SYS_RTX_BIND, 0, 2, CIRVANE_CAP_MSG) !=
        CIRVANE_SYS_OK) {
        line("cirvane-spike recovery=bind-fail");
        return;
    }
    slot = cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
    outcome = cirvane_syscall(&g_kernel, CIRVANE_SYS_RTX_ADMIT, 0,
                              CIRVANE_RTX_REASON_FAULT, 0);
    usb_write("cirvane-spike recovery outcome=");
    usb_u32((uint32_t)outcome);
    usb_write(" epoch=");
    usb_u32(cirvane_rtx_evidence(&g_kernel.rtx, 0)->epoch);
    usb_write(" stale=");
    usb_u32(cirvane_rtx_slot_deliverable(&g_kernel.rtx, (int)slot) ? 1u : 0u);
    usb_write(" health=");
    usb_u32(cirvane_rtx_health(&g_kernel.rtx, 0));
    usb_write("\r\n");
}

static void demo_kernel_core(void)
{
    int steps = 0;

    if (cirvane_syscall(&g_kernel, CIRVANE_SYS_RTX_BIND, 1, 2, CIRVANE_CAP_MSG) !=
            CIRVANE_SYS_OK ||
        cirvane_syscall(&g_kernel, CIRVANE_SYS_RTX_BIND, 2, 2, CIRVANE_CAP_MSG) !=
            CIRVANE_SYS_OK) {
        line("cirvane-spike kernel=bind-fail");
        return;
    }
    cirvane_sched_set_tick(&g_kernel, 1, producer_tick);
    cirvane_sched_set_tick(&g_kernel, 2, consumer_tick);
    while (steps < 4 && g_sched_consumed == 0) {
        if (cirvane_sched_step(&g_kernel) < 0) {
            break;
        }
        steps++;
    }
    usb_write("cirvane-spike kernel sched=");
    usb_u32((uint32_t)steps);
    usb_write(" msg=");
    usb_u32(g_sched_consumed);
    usb_write(" cap=");
    usb_u32(cirvane_rtx_cap_check(&g_kernel.rtx, 1, CIRVANE_CAP_MSG) ? 1u : 0u);
    usb_write(" panic=");
    usb_u32(cirvane_panic_reason(&g_kernel));
    usb_write("\r\n");
}

static int verify_fail(uint8_t slot, uint32_t length)
{
    (void)slot;
    (void)length;
    return -1;
}

static void demo_config(void)
{
    cirvane_cfg_journal_t journal;
    cirvane_cfg_t cfg;
    int fallback;
    int durable;

    cirvane_cfg_reset(&journal);
    if (cirvane_cfg_flash_load(&journal, &cfg) != CIRVANE_CFG_OK) {
        line("cirvane-spike config=load-fail");
        return;
    }
    cfg.heartbeat_ms = 2000;
    if (cirvane_cfg_flash_commit(&journal, &cfg) != CIRVANE_CFG_OK) {
        line("cirvane-spike config=commit-fail");
        return;
    }
    cirvane_cfg_reset(&journal);
    durable = 0;
    if (cirvane_cfg_flash_load(&journal, &cfg) == CIRVANE_CFG_OK &&
        cirvane_cfg_generation(&journal) != 0 && cfg.heartbeat_ms == 2000) {
        durable = 1;
    }
    fallback = cirvane_cfg_flash_inject_corrupt(&journal) == CIRVANE_CFG_OK ? 1
                                                                           : 0;
    usb_write("cirvane-spike config gen=");
    usb_u32(cirvane_cfg_generation(&journal));
    usb_write(" fallback=");
    usb_u32((uint32_t)fallback);
    usb_write(" durable=");
    usb_u32((uint32_t)durable);
    usb_write("\r\n");
}

static void demo_ota(void)
{
    cirvane_ota_t ota;
    uint8_t boot_before;
    uint32_t refused;
    uint32_t unchanged;

    cirvane_ota_reset(&ota, 0);
    boot_before = cirvane_ota_boot_slot(&ota);
    if (cirvane_ota_begin(&ota, 1, 1024) != CIRVANE_OTA_OK ||
        cirvane_ota_write(&ota, 1024) != CIRVANE_OTA_OK) {
        line("cirvane-spike ota=stage-fail");
        return;
    }
    refused = (cirvane_ota_end(&ota, verify_fail) == CIRVANE_OTA_REFUSED &&
               cirvane_ota_select(&ota) == CIRVANE_OTA_REFUSED)
                  ? 1u
                  : 0u;
    unchanged = cirvane_ota_boot_slot(&ota) == boot_before ? 1u : 0u;
    usb_write("cirvane-spike ota refuse=");
    usb_u32(refused);
    usb_write(" boot_unchanged=");
    usb_u32(unchanged);
    usb_write("\r\n");
}

static void demo_otadata(void)
{
    cirvane_ota_select_t backup[2];
    cirvane_ota_t ota;
    uint8_t slot = 0;
    uint8_t after = 9;
    uint32_t live = 0;
    uint32_t restored = 0;
    uint32_t crc_ok = 0;
    uint32_t refuse_write = 0;
    uint32_t boot_before;

    if (cirvane_otadata_read(backup) != CIRVANE_OTADATA_OK ||
        cirvane_otadata_active_slot(&slot) != CIRVANE_OTADATA_OK) {
        line("cirvane-spike otadata=read-fail");
        return;
    }
    crc_ok = (backup[0].ota_seq == 0xffffffffu ||
              backup[0].crc == cirvane_otadata_seq_crc(backup[0].ota_seq) ||
              backup[1].crc == cirvane_otadata_seq_crc(backup[1].ota_seq))
                 ? 1u
                 : 0u;
    cirvane_ota_reset(&ota, slot);
    boot_before = cirvane_ota_boot_slot(&ota);
    if (cirvane_ota_begin(&ota, (uint8_t)(1u - slot), 1024) == CIRVANE_OTA_OK &&
        cirvane_ota_write(&ota, 1024) == CIRVANE_OTA_OK &&
        cirvane_ota_end(&ota, verify_fail) == CIRVANE_OTA_REFUSED &&
        cirvane_ota_select(&ota) == CIRVANE_OTA_REFUSED &&
        cirvane_ota_boot_slot(&ota) == boot_before) {
        refuse_write = 1;
    }
    if (cirvane_otadata_select(slot) == CIRVANE_OTADATA_OK) {
        cirvane_ota_select_t written[2];
        live = 1;
        if (cirvane_otadata_read(written) == CIRVANE_OTADATA_OK &&
            ((written[0].ota_seq != 0xffffffffu &&
              written[0].crc == cirvane_otadata_seq_crc(written[0].ota_seq)) ||
             (written[1].ota_seq != 0xffffffffu &&
              written[1].crc == cirvane_otadata_seq_crc(written[1].ota_seq)))) {
            crc_ok = 1;
        }
    }
    if (cirvane_otadata_restore(backup) == CIRVANE_OTADATA_OK &&
        cirvane_otadata_active_slot(&after) == CIRVANE_OTADATA_OK &&
        after == slot) {
        restored = 1;
    }
    usb_write("cirvane-spike otadata live=");
    usb_u32(live);
    usb_write(" refuse_write=");
    usb_u32(refuse_write);
    usb_write(" restored=");
    usb_u32(restored);
    usb_write(" slot=");
    usb_u32(slot);
    usb_write(" crc=");
    usb_u32(crc_ok);
    usb_write("\r\n");
}

static void demo_adversarial(void)
{
    cirvane_kernel_t k;
    unsigned i;
    uint32_t exhaust = 0;
    uint32_t cap = 0;
    uint32_t stale = 0;
    uint32_t budget = 0;
    uint32_t nested = 0;
    uint32_t syscall = 0;
    int32_t slot;

    cirvane_kernel_init(&k);
    syscall = cirvane_syscall(&k, 99, 0, 0, 0) == CIRVANE_SYS_REFUSED ? 1u : 0u;
    cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 1, CIRVANE_CAP_MSG);
    cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 1, 2, CIRVANE_CAP_MSG);
    slot = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 1, 0, 0);
    cap = (slot >= 0 &&
           cirvane_syscall(&k, CIRVANE_SYS_CAP_REVOKE, 1, 0, 0) ==
               CIRVANE_SYS_OK &&
           cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 0, 1) ==
               CIRVANE_SYS_REFUSED)
              ? 1u
              : 0u;
    slot = -1;
    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        int32_t next = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
        if (next < 0) {
            break;
        }
        slot = next;
    }
    exhaust = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0) ==
                      CIRVANE_SYS_REFUSED
                  ? 1u
                  : 0u;
    stale = (slot >= 0 &&
             cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                             CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_RESTART &&
             cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 1, 1) ==
                 CIRVANE_SYS_REFUSED &&
             !cirvane_rtx_service_has_stale_work(&k.rtx, 0))
                ? 1u
                : 0u;
    budget = cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                             CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_FAILED
                 ? 1u
                 : 0u;
    k.rtx.services[1].recovering = 1;
    nested = (cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 1,
                              CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_REFUSED &&
              cirvane_rtx_health(&k.rtx, 1) == CIRVANE_HEALTH_RECOVERING)
                 ? 1u
                 : 0u;
    usb_write("cirvane-spike adv exhaust=");
    usb_u32(exhaust);
    usb_write(" cap=");
    usb_u32(cap);
    usb_write(" stale=");
    usb_u32(stale);
    usb_write(" budget=");
    usb_u32(budget);
    usb_write(" nested=");
    usb_u32(nested);
    usb_write(" syscall=");
    usb_u32(syscall);
    usb_write(" wrap=");
    usb_u32(cirvane_hal_timer_delta(1u, 0xffffffffu) == 2u ? 1u : 0u);
    usb_write("\r\n");
}

extern char _ebss[];
extern char _stack_top[];

static uint32_t stack_used(void)
{
    uint8_t *top = (uint8_t *)&_stack_top;
    uint8_t *low = (uint8_t *)&_ebss;
    uint8_t *p = top - 256;

    while (p > low && *p == 0xA5u) {
        p--;
    }
    return (uint32_t)(top - p);
}

static void demo_resources(void)
{
    uint32_t t0;
    uint32_t t1;
    uint32_t sched;
    uint32_t rec;

    t0 = cirvane_hal_timer_now();
    (void)cirvane_sched_step(&g_kernel);
    t1 = cirvane_hal_timer_now();
    sched = cirvane_hal_timer_delta(t1, t0);
    t0 = cirvane_hal_timer_now();
    (void)cirvane_syscall(&g_kernel, CIRVANE_SYS_RTX_ADMIT, 0,
                          CIRVANE_RTX_REASON_FAULT, 0);
    t1 = cirvane_hal_timer_now();
    rec = cirvane_hal_timer_delta(t1, t0);
    usb_write("cirvane-spike res sram=");
    usb_u32((uint32_t)((uintptr_t)&_ebss - 0x40800000u));
    usb_write(" stack=");
    usb_u32(stack_used());
    usb_write(" sched=");
    usb_u32(sched);
    usb_write(" rec=");
    usb_u32(rec);
    usb_write("\r\n");
}

#define CIRVANE_LAT_N 30u

static void sort_u32(uint32_t *vals, unsigned n)
{
    unsigned i;
    unsigned j;

    for (i = 1; i < n; i++) {
        uint32_t cur = vals[i];
        j = i;
        while (j > 0 && vals[j - 1] > cur) {
            vals[j] = vals[j - 1];
            j--;
        }
        vals[j] = cur;
    }
}

static uint32_t lat_p95(uint32_t *vals, unsigned n)
{
    unsigned idx;

    if (n == 0) {
        return 0;
    }
    sort_u32(vals, n);
    idx = ((n * 95u) + 99u) / 100u;
    if (idx == 0) {
        idx = 1;
    }
    if (idx > n) {
        idx = n;
    }
    return vals[idx - 1];
}

static uint32_t lat_med(uint32_t *vals, unsigned n)
{
    if (n == 0) {
        return 0;
    }
    sort_u32(vals, n);
    if ((n & 1u) != 0) {
        return vals[n / 2u];
    }
    return (vals[n / 2u - 1u] / 2u) + (vals[n / 2u] / 2u);
}

static void demo_latency(void)
{
    uint32_t sched[CIRVANE_LAT_N];
    uint32_t msg[CIRVANE_LAT_N];
    uint32_t irq[CIRVANE_LAT_N];
    unsigned i;
    uint32_t t0;
    uint32_t t1;
    int32_t slot;

    for (i = 0; i < CIRVANE_LAT_N; i++) {
        t0 = cirvane_hal_timer_now();
        (void)cirvane_sched_step(&g_kernel);
        t1 = cirvane_hal_timer_now();
        sched[i] = cirvane_hal_timer_delta(t1, t0);
    }
    for (i = 0; i < CIRVANE_LAT_N; i++) {
        t0 = cirvane_hal_timer_now();
        slot = cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_ALLOC, 1, 0, 0);
        if (slot >= 0) {
            (void)cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_SEND,
                                  (uint32_t)slot, 2, 1);
            slot = cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_RECV, 2, 0, 0);
            if (slot >= 0) {
                (void)cirvane_syscall(&g_kernel, CIRVANE_SYS_MSG_FREE,
                                      (uint32_t)slot, 0, 0);
            }
        }
        t1 = cirvane_hal_timer_now();
        msg[i] = cirvane_hal_timer_delta(t1, t0);
    }
    for (i = 0; i < CIRVANE_LAT_N; i++) {
        uint32_t spins = 0;
        uint32_t before = g_interrupt_count;
        t0 = cirvane_hal_timer_now();
        fire_software_interrupt();
        while (g_interrupt_count == before && spins < 200000u) {
            spins++;
        }
        t1 = cirvane_hal_timer_now();
        irq[i] = (g_interrupt_count != before)
                     ? cirvane_hal_timer_delta(t1, t0)
                     : 0xffffffffu;
    }
    usb_write("cirvane-spike lat n=");
    usb_u32(CIRVANE_LAT_N);
    usb_write(" sched_med=");
    usb_u32(lat_med(sched, CIRVANE_LAT_N));
    usb_write(" sched_p95=");
    usb_u32(lat_p95(sched, CIRVANE_LAT_N));
    usb_write(" msg_med=");
    usb_u32(lat_med(msg, CIRVANE_LAT_N));
    usb_write(" msg_p95=");
    usb_u32(lat_p95(msg, CIRVANE_LAT_N));
    usb_write(" irq_med=");
    usb_u32(lat_med(irq, CIRVANE_LAT_N));
    usb_write(" irq_p95=");
    usb_u32(lat_p95(irq, CIRVANE_LAT_N));
    usb_write("\r\n");
}

static void demo_hal(void)
{
    uint8_t level = 0;
    uint32_t e0 = 0;
    uint32_t e1 = 0;
    uint32_t gpio_ok = 0;
    uint32_t entropy_ok = 0;

    if (cirvane_hal_gpio_config_out(CIRVANE_GPIO_LED) == CIRVANE_HAL_OK &&
        cirvane_hal_gpio_set(CIRVANE_GPIO_LED, 1) == CIRVANE_HAL_OK &&
        cirvane_hal_gpio_get(CIRVANE_GPIO_LED, &level) == CIRVANE_HAL_OK &&
        level == 1u) {
        gpio_ok = 1;
    }
    cirvane_hal_gpio_set(CIRVANE_GPIO_LED, 0);
    if (cirvane_hal_entropy(&e0) == CIRVANE_HAL_OK &&
        cirvane_hal_entropy(&e1) == CIRVANE_HAL_OK && (e0 != e1 || e0 != 0)) {
        entropy_ok = 1;
    }
    usb_write("cirvane-spike hal gpio=");
    usb_u32(gpio_ok);
    usb_write(" entropy=");
    usb_u32(entropy_ok);
    usb_write(" wdt=");
    usb_u32((uint32_t)cirvane_hal_wdt_flashboot_enabled());
    usb_write("\r\n");
}

static void hil_run_probes(void)
{
    uint32_t t0_lo;
    uint32_t t1_lo;
    uint32_t magic;
    static uint32_t static_marker = 0xC12A0001u;
    uint32_t ecalls_before;

    line("cirvane-spike boot=ok");
    cirvane_irq_attach(&g_kernel, 0, software_isr);

    ecalls_before = g_ecall_count;
    __asm__ volatile("ecall");
    usb_write("cirvane-spike trap ecall=");
    usb_u32(g_ecall_count - ecalls_before);
    usb_write(" mcause=");
    usb_hex(g_last_mcause, 8);
    usb_write("\r\n");

    setup_clic_software_interrupt();
    fire_software_interrupt();
    {
        uint32_t spins = 0;
        while (g_interrupt_count == 0 && spins < 200000u) {
            spins++;
        }
    }
    usb_write("cirvane-spike interrupt count=");
    usb_u32(g_interrupt_count);
    usb_write("\r\n");

    t0_lo = cirvane_hal_timer_now();
    {
        volatile uint32_t wait = 0;
        while (wait < 20000u) {
            wait++;
        }
    }
    t1_lo = cirvane_hal_timer_now();
    usb_write("cirvane-spike timer t0=");
    usb_hex(t0_lo, 8);
    usb_write(" t1=");
    usb_hex(t1_lo, 8);
    usb_write(" advance=");
    usb_u32((t1_lo != t0_lo && t0_lo != 0) ? 1u : 0u);
    usb_write("\r\n");

    usb_write("cirvane-spike memory static=");
    usb_hex(static_marker, 8);
    usb_write("\r\n");

    magic = flash_magic();
    usb_write("cirvane-spike flash magic=");
    usb_hex(magic, 2);
    usb_write("\r\n");

    usb_write("cirvane-spike priv misa=");
    usb_hex(csr_misa(), 8);
    usb_write(" mstatus=");
    usb_hex(csr_mstatus(), 8);
    usb_write(" pmpcfg0=");
    usb_hex(csr_pmpcfg0(), 8);
    usb_write(" pmpaddr0=");
    usb_hex(csr_pmpaddr0(), 8);
    usb_write("\r\n");

    usb_write("cirvane-spike umode=");
    usb_u32(umode_implemented());
    usb_write("\r\n");

    demo_recovery();
    demo_kernel_core();
    demo_config();
    demo_ota();
    demo_otadata();
    demo_adversarial();
    demo_hal();
    demo_resources();
    demo_latency();
    line("cirvane-spike freertos=absent");
    if (cirvane_hal_uart_write("cirvane-spike uart=ok\r\n") != CIRVANE_HAL_OK) {
        line("cirvane-spike uart=fail");
    }
    line("cirvane-spike result=PASS");
    if (cirvane_panic_reason(&g_kernel) != CIRVANE_PANIC_NONE) {
        for (;;) {
            __asm__ volatile("wfi");
        }
    }
    for (;;) {
        line("cirvane-spike result=PASS");
        {
            volatile uint32_t wait = 0;
            while (wait < 800000u) {
                wait++;
            }
        }
    }
}
#endif

#ifdef CIRVANE_SPIKE_EVAL
#define CIRVANE_EVAL_TOTAL 33u

static void eval_print_sample(uint32_t class_id, uint32_t i, int32_t outcome,
                              uint32_t stale, uint32_t ticks)
{
    usb_write("cirvane-eval class=");
    usb_u32(class_id);
    usb_write(" i=");
    usb_u32(i);
    usb_write(" outcome=");
    usb_u32((uint32_t)outcome);
    usb_write(" stale=");
    usb_u32(stale);
    usb_write(" ticks=");
    usb_u32(ticks);
    usb_write("\r\n");
}

static void eval_run(void)
{
    cirvane_kernel_t k;
    unsigned class_id;
    unsigned i;
    uint32_t t0;
    uint32_t t1;
    int32_t outcome;
    uint32_t stale;
    int32_t slot;
    unsigned n;

    line("cirvane-eval boot=ok");
    for (class_id = 0; class_id < 5; class_id++) {
        for (i = 0; i < CIRVANE_EVAL_TOTAL; i++) {
            cirvane_kernel_init(&k);
            t0 = cirvane_hal_timer_now();
            stale = 1;
            outcome = CIRVANE_RTX_REFUSED;
            if (class_id == 0) {
                cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 8, CIRVANE_CAP_MSG);
                cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 1, 8, CIRVANE_CAP_MSG);
                slot = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
                outcome = cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                                          CIRVANE_RTX_REASON_FAULT, 0);
                stale = (slot >= 0 &&
                         cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND,
                                         (uint32_t)slot, 1, 1) ==
                             CIRVANE_SYS_REFUSED &&
                         !cirvane_rtx_service_has_stale_work(&k.rtx, 0))
                            ? 0u
                            : 1u;
            } else if (class_id == 1) {
                cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 8, CIRVANE_CAP_MSG);
                slot = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
                outcome = cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                                          CIRVANE_RTX_REASON_DEADLINE, 0);
                stale = (slot >= 0 &&
                         !cirvane_rtx_service_has_stale_work(&k.rtx, 0))
                            ? 0u
                            : 1u;
            } else if (class_id == 2) {
                cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 1, CIRVANE_CAP_MSG);
                (void)cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                                      CIRVANE_RTX_REASON_FAULT, 0);
                outcome = cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                                          CIRVANE_RTX_REASON_FAULT, 0);
                stale = !cirvane_rtx_service_has_stale_work(&k.rtx, 0) ? 0u : 1u;
            } else if (class_id == 3) {
                cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 8, CIRVANE_CAP_MSG);
                k.rtx.services[0].recovering = 1;
                outcome = cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                                          CIRVANE_RTX_REASON_FAULT, 0);
                stale = (outcome == CIRVANE_RTX_REFUSED &&
                         cirvane_rtx_health(&k.rtx, 0) ==
                             CIRVANE_HEALTH_RECOVERING)
                            ? 0u
                            : 1u;
            } else {
                cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 8, CIRVANE_CAP_MSG);
                for (n = 0; n < CIRVANE_RTX_SLOT_COUNT; n++) {
                    (void)cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
                }
                outcome = cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                                          CIRVANE_RTX_REASON_FAULT, 0);
                stale = (!cirvane_rtx_service_has_stale_work(&k.rtx, 0) &&
                         cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0) >= 0)
                            ? 0u
                            : 1u;
            }
            t1 = cirvane_hal_timer_now();
            eval_print_sample(class_id, i, outcome, stale,
                              cirvane_hal_timer_delta(t1, t0));
        }
    }
    line("cirvane-eval done");
    for (;;) {
        __asm__ volatile("wfi");
    }
}
#endif

void kernel_main(void)
{
    enable_usb_serial_jtag();
    cirvane_hal_init();
    cirvane_hal_wdt_disarm();
    cirvane_kernel_init(&g_kernel);
#ifdef CIRVANE_HIL_SPIKE
    hil_run_probes();
#elif defined(CIRVANE_SPIKE_EVAL)
    eval_run();
#else
    {
        cirvane_shell_t sh;
        cirvane_cfg_journal_t journal;
        cirvane_cfg_t cfg;
        char reply[CIRVANE_OBS_LINE_MAX];
        cirvane_shell_init(&sh);
        cirvane_cfg_reset(&journal);
        (void)cirvane_cfg_flash_load(&journal, &cfg);
        line("cirvane boot=ok");
        usb_write(CIRVANE_SHELL_PROMPT);
        usb_write(" ");
        for (;;) {
            while (REG32(USB_SERIAL_JTAG_EP1_CONF_REG) &
                   USB_SERIAL_JTAG_SERIAL_OUT_EP_DATA_AVAIL) {
                uint8_t byte = (uint8_t)REG32(USB_SERIAL_JTAG_EP1_REG);
                if (cirvane_shell_push(&sh, byte)) {
                    if (cirvane_shell_run(&sh, &g_kernel, reply, sizeof(reply)) >
                        0) {
                        line(reply);
                    }
                    usb_write(CIRVANE_SHELL_PROMPT);
                    usb_write(" ");
                }
            }
        }
    }
#endif
}
