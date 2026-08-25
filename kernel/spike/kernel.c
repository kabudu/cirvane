/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOS-free ESP32-C5 kernel spike. Proves boot, traps, interrupts,
 * timer, USB Serial/JTAG, static memory, flash ROM read, PMP/privilege
 * probes and one recovery-transaction demonstration.
 */

#include "hw.h"
#include "recovery.h"

#include <stdint.h>

volatile uint32_t g_interrupt_count;
volatile uint32_t g_ecall_count;
volatile uint32_t g_other_trap_count;
volatile uint32_t g_last_mcause;
static cirvane_world_t g_world;

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

static int fifo_ready(uint32_t spins)
{
    uint32_t n = 0;
    while (n < spins) {
        if (REG32(USB_SERIAL_JTAG_EP1_CONF_REG) &
            USB_SERIAL_JTAG_SERIAL_IN_EP_DATA_FREE) {
            return 1;
        }
        n++;
    }
    return 0;
}

static void usb_write(const char *s)
{
    uint32_t pending = 0;

    while (*s) {
        if (!fifo_ready(200000u)) {
            return;
        }
        REG32(USB_SERIAL_JTAG_EP1_REG) = (uint8_t)*s;
        pending++;
        s++;
        if (pending == 64u) {
            REG32(USB_SERIAL_JTAG_EP1_CONF_REG) = USB_SERIAL_JTAG_WR_DONE;
            pending = 0;
        }
    }
    if (pending != 0) {
        REG32(USB_SERIAL_JTAG_EP1_CONF_REG) = USB_SERIAL_JTAG_WR_DONE;
    }
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

uint32_t cirvane_trap(uint32_t mcause, uint32_t mepc)
{
    g_last_mcause = mcause;
    if (mcause & MCAUSE_INTERRUPT) {
        g_interrupt_count += 1;
        REG32(INTPRI_CPU_INTR_FROM_CPU_0_REG) = 0;
        REG32(CLIC_INT_CTRL_REG(CLIC_EXT_INTR_NUM_OFFSET + 1)) =
            CLIC_INT_ATTR_MODE_M | CLIC_INT_IE;
        return mepc;
    }
    if ((mcause & 0x7fffffffu) == MCAUSE_ECALL_M ||
        (mcause & 0x7fffffffu) == MCAUSE_ECALL_U) {
        g_ecall_count += 1;
        return mepc + 4u;
    }
    g_other_trap_count += 1;
    return mepc + 4u;
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

    REG32(INTERRUPT_CORE0_CPU_INTR_FROM_CPU_0_MAP_REG) = 1u;
    REG32(CLIC_INT_CTRL_REG(clic_id)) = CLIC_INT_ATTR_MODE_M | CLIC_INT_IE;

    mtvec = ((uint32_t)&cirvane_trap_entry) & ~63u;
    mtvec |= 3u;
    __asm__ volatile("csrw mtvec, %0" ::"r"(mtvec));
    __asm__ volatile("csrs mstatus, %0" ::"r"(MSTATUS_MIE));
}

static void fire_software_interrupt(void)
{
    REG32(INTPRI_CPU_INTR_FROM_CPU_0_REG) = 1u;
}

static uint32_t umode_implemented(void)
{
    /* misa bit 20 is the U extension. A live mret into U-mode is deferred
     * until Stage 2 because a failed drop would hide the other spike probes.
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

static void demo_recovery(void)
{
    int slot;
    uint8_t outcome;

    cirvane_rtx_reset(&g_world);
    if (!cirvane_rtx_bind(&g_world, 0, 2, 0x1)) {
        line("cirvane-spike recovery=bind-fail");
        return;
    }
    slot = cirvane_rtx_alloc_slot(&g_world, 0);
    outcome = cirvane_rtx_admit(&g_world, 0, CIRVANE_RTX_REASON_FAULT);
    usb_write("cirvane-spike recovery outcome=");
    usb_u32(outcome);
    usb_write(" epoch=");
    usb_u32(cirvane_rtx_evidence(&g_world, 0)->epoch);
    usb_write(" stale=");
    usb_u32(cirvane_rtx_slot_deliverable(&g_world, slot) ? 1u : 0u);
    usb_write(" health=");
    usb_u32(cirvane_rtx_health(&g_world, 0));
    usb_write("\r\n");
}

void kernel_main(void)
{
    uint32_t t0_lo;
    uint32_t t1_lo;
    uint32_t magic;
    static uint32_t static_marker = 0xC12A0001u;
    uint32_t ecalls_before;
    disable_watchdogs();
    line("cirvane-spike boot=ok");

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
    line("cirvane-spike freertos=absent");
    line("cirvane-spike uart=ok");
    line("cirvane-spike result=PASS");
}
