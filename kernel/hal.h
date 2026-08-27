/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Bounded kernel HAL. UART, GPIO, timer, flash read, watchdog and entropy.
 * Radio and network are excluded (owner decision; ESP-IDF Wi-Fi needs FreeRTOS).
 * No allocator. Flash writes and otadata are not in this increment.
 */

#pragma once

#include <stdint.h>

#define CIRVANE_HAL_OK 0
#define CIRVANE_HAL_REFUSED -1

#define CIRVANE_GPIO_LED 27u
#define CIRVANE_GPIO_LIMIT 32u
#define CIRVANE_FLASH_SIZE 0x800000u
#define CIRVANE_FLASH_READ_MAX 256u

int cirvane_hal_init(void);
int cirvane_hal_uart_write(const char *s);
int cirvane_hal_gpio_config_out(uint8_t pin);
int cirvane_hal_gpio_set(uint8_t pin, uint8_t level);
int cirvane_hal_gpio_get(uint8_t pin, uint8_t *level);
uint32_t cirvane_hal_timer_now(void);
int cirvane_hal_flash_read(uint32_t offset, void *buf, uint32_t length);
int cirvane_hal_wdt_disarm(void);
int cirvane_hal_wdt_flashboot_enabled(void);
int cirvane_hal_entropy(uint32_t *out);
