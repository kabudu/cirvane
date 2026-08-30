/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CIRVANE_WIFI_SSID_MAX 32u
#define CIRVANE_WIFI_PASSWORD_MAX 63u

static inline bool cirvane_wifi_printable_ascii(const char *value, size_t length)
{
    if (value == NULL) return false;
    for (size_t i = 0; i < length; ++i) {
        uint8_t ch = (uint8_t)value[i];
        if (ch < 32u || ch > 126u) return false;
    }
    return true;
}

static inline bool cirvane_wifi_ssid_valid(const char *ssid, size_t length)
{
    return length >= 1u && length <= CIRVANE_WIFI_SSID_MAX &&
           cirvane_wifi_printable_ascii(ssid, length);
}

static inline bool cirvane_wifi_password_valid(const char *password,
                                                size_t length)
{
    return (length == 0u || (length >= 8u && length <= CIRVANE_WIFI_PASSWORD_MAX)) &&
           cirvane_wifi_printable_ascii(password, length);
}
