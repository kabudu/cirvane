/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Host mock for the kernel HAL. Not linked into the C5 spike.
 */

#include "hal.h"

static uint8_t s_gpio_out[CIRVANE_GPIO_LIMIT];
static uint8_t s_gpio_cfg[CIRVANE_GPIO_LIMIT];
static uint32_t s_timer;
static uint8_t s_flash[64];
static uint8_t s_wdt_flashboot;
static uint32_t s_entropy;
static uint8_t s_inited;

static void memzero(void *ptr, unsigned len)
{
    uint8_t *bytes = ptr;
    unsigned i;
    for (i = 0; i < len; i++) {
        bytes[i] = 0;
    }
}

int cirvane_hal_init(void)
{
    unsigned i;
    memzero(s_gpio_out, sizeof(s_gpio_out));
    memzero(s_gpio_cfg, sizeof(s_gpio_cfg));
    s_timer = 1;
    memzero(s_flash, sizeof(s_flash));
    s_flash[0] = 0xe9;
    s_wdt_flashboot = 1;
    s_entropy = 0xA5A5A5A5u;
    s_inited = 1;
    for (i = 1; i < sizeof(s_flash); i++) {
        s_flash[i] = (uint8_t)i;
    }
    return CIRVANE_HAL_OK;
}

int cirvane_hal_uart_write(const char *s)
{
    if (!s_inited || s == 0 || s[0] == 0) {
        return CIRVANE_HAL_REFUSED;
    }
    return CIRVANE_HAL_OK;
}

int cirvane_hal_gpio_config_out(uint8_t pin)
{
    if (!s_inited || pin >= CIRVANE_GPIO_LIMIT) {
        return CIRVANE_HAL_REFUSED;
    }
    s_gpio_cfg[pin] = 1;
    return CIRVANE_HAL_OK;
}

int cirvane_hal_gpio_set(uint8_t pin, uint8_t level)
{
    if (!s_inited || pin >= CIRVANE_GPIO_LIMIT || !s_gpio_cfg[pin] ||
        level > 1u) {
        return CIRVANE_HAL_REFUSED;
    }
    s_gpio_out[pin] = level;
    return CIRVANE_HAL_OK;
}

int cirvane_hal_gpio_get(uint8_t pin, uint8_t *level)
{
    if (!s_inited || pin >= CIRVANE_GPIO_LIMIT || !s_gpio_cfg[pin] ||
        level == 0) {
        return CIRVANE_HAL_REFUSED;
    }
    *level = s_gpio_out[pin];
    return CIRVANE_HAL_OK;
}

uint32_t cirvane_hal_timer_now(void)
{
    if (!s_inited) {
        return 0;
    }
    s_timer += 1u;
    return s_timer;
}

int cirvane_hal_flash_read(uint32_t offset, void *buf, uint32_t length)
{
    uint8_t *out = buf;
    uint32_t i;

    if (!s_inited || buf == 0 || length == 0 || (length & 3u) != 0 ||
        length > CIRVANE_FLASH_READ_MAX || offset >= CIRVANE_FLASH_SIZE ||
        length > CIRVANE_FLASH_SIZE - offset) {
        return CIRVANE_HAL_REFUSED;
    }
    for (i = 0; i < length; i++) {
        uint32_t src = offset + i;
        out[i] = src < sizeof(s_flash) ? s_flash[src] : 0xffu;
    }
    return CIRVANE_HAL_OK;
}

int cirvane_hal_wdt_disarm(void)
{
    if (!s_inited) {
        return CIRVANE_HAL_REFUSED;
    }
    s_wdt_flashboot = 0;
    return CIRVANE_HAL_OK;
}

int cirvane_hal_wdt_flashboot_enabled(void)
{
    if (!s_inited) {
        return CIRVANE_HAL_REFUSED;
    }
    return s_wdt_flashboot ? 1 : 0;
}

int cirvane_hal_entropy(uint32_t *out)
{
    if (!s_inited || out == 0) {
        return CIRVANE_HAL_REFUSED;
    }
    s_entropy = (s_entropy * 1664525u) + 1013904223u;
    *out = s_entropy;
    return CIRVANE_HAL_OK;
}
