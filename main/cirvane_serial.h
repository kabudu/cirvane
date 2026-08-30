/*
 * SPDX-License-Identifier: MIT
 *
 * Serial-number arithmetic shared by the firmware and host tests.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

static inline bool cirvane_serial_u32_newer(uint32_t candidate,
                                             uint32_t reference)
{
    const uint32_t distance = candidate - reference;
    return distance != 0u && distance < (UINT32_C(1) << 31);
}
