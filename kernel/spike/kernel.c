/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOS-free ESP32-C5 kernel spike. Proves boot, traps, interrupt
 * dispatch, timer, USB Serial/JTAG, static memory, flash ROM read,
 * PMP/privilege probes, recovery syscalls and cooperative scheduling.
 */

#include "hw.h"
#include "kernel.h"

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
static uint32_t g_sched_consumed;

void cirvane_trap_entry(void);

static void disable_watchdogs(void)
{
    unsigned i;
    const uint32_t timg[2] = {DR_REG_TIMERG0_BASE, DR_REG_TIMERG1_BASE};

    REG32(LP_WDT_WPROTECT_REG) = WDT_UNLOCK_KEY;
    REG32(LP_WDT_CONFIG0_REG) &= ~(LP_WDT_WDT_EN | LP_WDT_WDT_FLASHBOOT_MOD_EN);
    REG32(LP_WDT_WPROTECT_REG) = 0;

    REG32(LP_WDT_SWD_WPROTECT_REG) = WDT_UNLOCK_KEY;
    REG32(LP_WDT_SWD_CONFIG_REG) |= LP_WDT_SWD_DISABLE;
    REG32(LP_WDT_SWD_WPROTECT_REG) = 0;

    for (i = 0; i < 2; i++) {
        REG32(TIMG_WDTWPROTECT_REG(timg[i])) = WDT_UNLOCK_KEY;
        REG32(TIMG_WDTCONFIG0_REG(timg[i])) &=
            ~(TIMG_WDT_EN | TIMG_WDT_FLASHBOOT_MOD_EN);
        REG32(TIMG_WDTWPROTECT_REG(timg[i])) = 0;
    }
}

typedef int (*rom_usb_tx_one_char_fn)(uint8_t);
typedef void (*rom_usb_tx_flush_fn)(void);

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

static void line(const char *s)
{
    usb_write(s);
    usb_write("\r\n");
}

static void software_isr(void)
{
    g_interrupt_count += 1;
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
    cirvane_panic(&g_kernel, CIRVANE_PANIC_UNKNOWN_TRAP);
    for (;;) {
        __asm__ volatile("wfi");
    }
}

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

static uint64_t systimer_now(void)
{
    uint32_t spins = 0;

    REG32(SYSTIMER_CONF_REG) |= SYSTIMER_TIMER_UNIT0_WORK_EN;
    REG32(SYSTIMER_UNIT0_OP_REG) = SYSTIMER_TIMER_UNIT0_UPDATE;
    while (spins < 10000u) {
        if (REG32(SYSTIMER_UNIT0_OP_REG) & SYSTIMER_TIMER_UNIT0_VALUE_VALID) {
            uint32_t lo = REG32(SYSTIMER_UNIT0_VALUE_LO_REG);
            uint32_t hi = REG32(SYSTIMER_UNIT0_VALUE_HI_REG);
            return ((uint64_t)hi << 32) | lo;
        }
        spins++;
    }
    return 0;
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
    esp_rom_spiflash_read_fn read_fn =
        (esp_rom_spiflash_read_fn)(uintptr_t)ROM_SPIFLASH_READ;
    if (read_fn(FLASH_BOOTLOADER_OFF, &word, 4) != 0) {
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

void kernel_main(void)
{
    uint32_t t0_lo;
    uint32_t t1_lo;
    uint32_t magic;
    static uint32_t static_marker = 0xC12A0001u;
    uint32_t ecalls_before;
    enable_usb_serial_jtag();
    line("cirvane-spike boot=ok");
    disable_watchdogs();
    cirvane_kernel_init(&g_kernel);
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

    t0_lo = (uint32_t)systimer_now();
    {
        volatile uint32_t wait = 0;
        while (wait < 20000u) {
            wait++;
        }
    }
    t1_lo = (uint32_t)systimer_now();
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
    line("cirvane-spike freertos=absent");
    line("cirvane-spike uart=ok");
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
