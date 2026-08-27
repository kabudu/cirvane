/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * ESP32-C5 HAL: USB Serial/JTAG UART, GPIO 27 LED, SYSTIMER, ROM SPI flash
 * read/erase/write in the config window, TIMG/LP watchdog mute, LPPERI RNG.
 * No FreeRTOS, no allocator.
 */

#include "hal.h"
#include "hw.h"

static uint8_t s_gpio_cfg[CIRVANE_GPIO_LIMIT];

int cirvane_hal_init(void)
{
    unsigned i;
    for (i = 0; i < CIRVANE_GPIO_LIMIT; i++) {
        s_gpio_cfg[i] = 0;
    }
    REG32(LPPERI_CLK_EN_REG) |= LPPERI_RNG_CK_EN | LPPERI_CLK_EN;
    REG32(LPPERI_RNG_CFG_REG) |= LPPERI_RNG_SAMPLE_ENABLE | LPPERI_RNG_TIMER_EN;
    return CIRVANE_HAL_OK;
}

int cirvane_hal_uart_write(const char *s)
{
    rom_usb_tx_one_char_fn rom_tx =
        (rom_usb_tx_one_char_fn)(uintptr_t)ROM_USB_TX_ONE_CHAR;
    rom_usb_tx_flush_fn rom_flush =
        (rom_usb_tx_flush_fn)(uintptr_t)ROM_USB_TX_FLUSH;

    if (s == 0 || s[0] == 0) {
        return CIRVANE_HAL_REFUSED;
    }
    while (*s) {
        rom_tx((uint8_t)*s);
        s++;
    }
    rom_flush();
    return CIRVANE_HAL_OK;
}

int cirvane_hal_gpio_config_out(uint8_t pin)
{
    uint32_t bit;

    if (pin != CIRVANE_GPIO_LED) {
        return CIRVANE_HAL_REFUSED;
    }
    bit = 1u << pin;
    REG32(IO_MUX_GPIO27_REG) = (FUNC_GPIO27_GPIO << MCU_SEL_S) | (2u << FUN_DRV_S);
    REG32(GPIO_FUNC27_OUT_SEL_CFG_REG) = GPIO_OUT_SEL_GPIO;
    REG32(GPIO_ENABLE_W1TS_REG) = bit;
    s_gpio_cfg[pin] = 1;
    return CIRVANE_HAL_OK;
}

int cirvane_hal_gpio_set(uint8_t pin, uint8_t level)
{
    uint32_t bit;

    if (pin >= CIRVANE_GPIO_LIMIT || !s_gpio_cfg[pin] || level > 1u) {
        return CIRVANE_HAL_REFUSED;
    }
    bit = 1u << pin;
    if (level) {
        REG32(GPIO_OUT_W1TS_REG) = bit;
    } else {
        REG32(GPIO_OUT_W1TC_REG) = bit;
    }
    return CIRVANE_HAL_OK;
}

int cirvane_hal_gpio_get(uint8_t pin, uint8_t *level)
{
    if (pin >= CIRVANE_GPIO_LIMIT || !s_gpio_cfg[pin] || level == 0) {
        return CIRVANE_HAL_REFUSED;
    }
    *level = (uint8_t)((REG32(GPIO_OUT_REG) >> pin) & 1u);
    return CIRVANE_HAL_OK;
}

uint32_t cirvane_hal_timer_now(void)
{
    uint32_t spins = 0;

    REG32(SYSTIMER_CONF_REG) |= SYSTIMER_TIMER_UNIT0_WORK_EN;
    REG32(SYSTIMER_UNIT0_OP_REG) = SYSTIMER_TIMER_UNIT0_UPDATE;
    while (spins < 10000u) {
        if (REG32(SYSTIMER_UNIT0_OP_REG) & SYSTIMER_TIMER_UNIT0_VALUE_VALID) {
            return REG32(SYSTIMER_UNIT0_VALUE_LO_REG);
        }
        spins++;
    }
    return 0;
}

static int flash_in_cfg_window(uint32_t offset, uint32_t length)
{
    if (length == 0 || offset < CIRVANE_CFG_FLASH_BASE) {
        return 0;
    }
    if (length > CIRVANE_CFG_FLASH_SIZE) {
        return 0;
    }
    if (offset > CIRVANE_CFG_FLASH_BASE + CIRVANE_CFG_FLASH_SIZE - length) {
        return 0;
    }
    return 1;
}

static uint32_t irq_suspend(void)
{
    uint32_t mstatus;

    __asm__ volatile("csrrc %0, mstatus, %1" : "=r"(mstatus) : "r"(8));
    return mstatus;
}

static void irq_restore(uint32_t mstatus)
{
    __asm__ volatile("csrw mstatus, %0" ::"r"(mstatus));
}

int cirvane_hal_flash_read(uint32_t offset, void *buf, uint32_t length)
{
    esp_rom_spiflash_read_fn read_fn =
        (esp_rom_spiflash_read_fn)(uintptr_t)ROM_SPIFLASH_READ;

    if (buf == 0 || length == 0 || (length & 3u) != 0 ||
        ((uintptr_t)buf & 3u) != 0 || length > CIRVANE_FLASH_READ_MAX ||
        offset >= CIRVANE_FLASH_SIZE || length > CIRVANE_FLASH_SIZE - offset) {
        return CIRVANE_HAL_REFUSED;
    }
    if (read_fn(offset, buf, (int32_t)length) != 0) {
        return CIRVANE_HAL_REFUSED;
    }
    return CIRVANE_HAL_OK;
}

int cirvane_hal_flash_erase(uint32_t offset, uint32_t length)
{
    esp_rom_spiflash_unlock_fn unlock_fn =
        (esp_rom_spiflash_unlock_fn)(uintptr_t)ROM_SPIFLASH_UNLOCK;
    esp_rom_spiflash_erase_sector_fn erase_fn =
        (esp_rom_spiflash_erase_sector_fn)(uintptr_t)ROM_SPIFLASH_ERASE_SECTOR;
    uint32_t saved;
    uint32_t pos;

    if ((offset & (CIRVANE_FLASH_SECTOR - 1u)) != 0 ||
        (length & (CIRVANE_FLASH_SECTOR - 1u)) != 0 ||
        !flash_in_cfg_window(offset, length)) {
        return CIRVANE_HAL_REFUSED;
    }
    saved = irq_suspend();
    if (unlock_fn() != 0) {
        irq_restore(saved);
        return CIRVANE_HAL_REFUSED;
    }
    for (pos = offset; pos < offset + length; pos += CIRVANE_FLASH_SECTOR) {
        if (erase_fn(pos / CIRVANE_FLASH_SECTOR) != 0) {
            irq_restore(saved);
            return CIRVANE_HAL_REFUSED;
        }
    }
    irq_restore(saved);
    return CIRVANE_HAL_OK;
}

int cirvane_hal_flash_write(uint32_t offset, const void *buf, uint32_t length)
{
    esp_rom_spiflash_unlock_fn unlock_fn =
        (esp_rom_spiflash_unlock_fn)(uintptr_t)ROM_SPIFLASH_UNLOCK;
    esp_rom_spiflash_write_fn write_fn =
        (esp_rom_spiflash_write_fn)(uintptr_t)ROM_SPIFLASH_WRITE;
    uint32_t saved;

    if (buf == 0 || (length & 3u) != 0 || ((uintptr_t)buf & 3u) != 0 ||
        (offset & 3u) != 0 || length > CIRVANE_FLASH_WRITE_MAX ||
        (offset & (CIRVANE_FLASH_SECTOR - 1u)) + length > CIRVANE_FLASH_SECTOR ||
        !flash_in_cfg_window(offset, length)) {
        return CIRVANE_HAL_REFUSED;
    }
    saved = irq_suspend();
    if (unlock_fn() != 0) {
        irq_restore(saved);
        return CIRVANE_HAL_REFUSED;
    }
    if (write_fn(offset, (const uint32_t *)buf, (int32_t)length) != 0) {
        irq_restore(saved);
        return CIRVANE_HAL_REFUSED;
    }
    irq_restore(saved);
    return CIRVANE_HAL_OK;
}

int cirvane_hal_wdt_disarm(void)
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
    return CIRVANE_HAL_OK;
}

int cirvane_hal_wdt_flashboot_enabled(void)
{
    uint32_t lp = REG32(LP_WDT_CONFIG0_REG);
    uint32_t t0 = REG32(TIMG_WDTCONFIG0_REG(DR_REG_TIMERG0_BASE));
    uint32_t t1 = REG32(TIMG_WDTCONFIG0_REG(DR_REG_TIMERG1_BASE));
    if ((lp & LP_WDT_WDT_FLASHBOOT_MOD_EN) || (t0 & TIMG_WDT_FLASHBOOT_MOD_EN) ||
        (t1 & TIMG_WDT_FLASHBOOT_MOD_EN)) {
        return 1;
    }
    return 0;
}

int cirvane_hal_entropy(uint32_t *out)
{
    volatile uint32_t wait = 0;

    if (out == 0) {
        return CIRVANE_HAL_REFUSED;
    }
    while (wait < 2000u) {
        wait++;
    }
    *out = REG32(LPPERI_RNG_DATA_REG);
    return CIRVANE_HAL_OK;
}
