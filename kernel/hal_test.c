/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 */

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

static void test_gpio_and_uart(void)
{
    uint8_t level = 9;

    expect(cirvane_hal_init() == CIRVANE_HAL_OK, "init");
    expect(cirvane_hal_uart_write("ok") == CIRVANE_HAL_OK, "uart");
    expect(cirvane_hal_uart_write("") == CIRVANE_HAL_REFUSED, "empty uart");
    expect(cirvane_hal_gpio_config_out(CIRVANE_GPIO_LIMIT) == CIRVANE_HAL_REFUSED,
           "bad pin");
    expect(cirvane_hal_gpio_config_out(CIRVANE_GPIO_LED) == CIRVANE_HAL_OK,
           "cfg led");
    expect(cirvane_hal_gpio_set(CIRVANE_GPIO_LED, 2) == CIRVANE_HAL_REFUSED,
           "bad level");
    expect(cirvane_hal_gpio_set(CIRVANE_GPIO_LED, 1) == CIRVANE_HAL_OK, "set");
    expect(cirvane_hal_gpio_get(CIRVANE_GPIO_LED, &level) == CIRVANE_HAL_OK,
           "get");
    expect(level == 1, "level 1");
    expect(cirvane_hal_gpio_set(CIRVANE_GPIO_LED, 0) == CIRVANE_HAL_OK, "off");
}

static void test_flash_timer_wdt_entropy(void)
{
    uint8_t buf[4];
    uint32_t t0;
    uint32_t t1;
    uint32_t e0;
    uint32_t e1;

    expect(cirvane_hal_wdt_disarm() == CIRVANE_HAL_OK, "disarm");
    expect(cirvane_hal_wdt_flashboot_enabled() == 0, "flashboot off");
    t0 = cirvane_hal_timer_now();
    t1 = cirvane_hal_timer_now();
    expect(t1 > t0, "timer advance");
    expect(cirvane_hal_flash_read(0, buf, 4) == CIRVANE_HAL_OK, "flash");
    expect(buf[0] == 0xe9, "magic");
    expect(cirvane_hal_flash_read(0, buf, 0) == CIRVANE_HAL_REFUSED, "zero len");
    expect(cirvane_hal_flash_read(0, buf, 1) == CIRVANE_HAL_REFUSED, "unaligned");
    expect(cirvane_hal_flash_read(0, buf, CIRVANE_FLASH_READ_MAX + 1) ==
               CIRVANE_HAL_REFUSED,
           "oversize");
    expect(cirvane_hal_flash_read(CIRVANE_FLASH_SIZE, buf, 4) ==
               CIRVANE_HAL_REFUSED,
           "oob");
    expect(cirvane_hal_entropy(&e0) == CIRVANE_HAL_OK, "e0");
    expect(cirvane_hal_entropy(&e1) == CIRVANE_HAL_OK, "e1");
    expect(e0 != e1, "entropy changes");
    expect(cirvane_hal_entropy(0) == CIRVANE_HAL_REFUSED, "null entropy");
}

int main(void)
{
    test_gpio_and_uart();
    test_flash_timer_wdt_entropy();
    if (failures != 0) {
        fprintf(stderr, "%d HAL checks failed\n", failures);
        return 1;
    }
    puts("kernel HAL checks passed");
    return 0;
}
