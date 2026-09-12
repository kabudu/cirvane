/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CIRVANE_OTA_REMOTE_H
#define CIRVANE_OTA_REMOTE_H

#include "esp_err.h"

typedef enum {
    CIRVANE_OTA_REMOTE_OK = 0,
    CIRVANE_OTA_REMOTE_BUSY,
    CIRVANE_OTA_REMOTE_NETWORK,
    CIRVANE_OTA_REMOTE_TRANSPORT,
    CIRVANE_OTA_REMOTE_MANIFEST,
    CIRVANE_OTA_REMOTE_SIGNATURE,
    CIRVANE_OTA_REMOTE_POLICY,
    CIRVANE_OTA_REMOTE_STORAGE,
    CIRVANE_OTA_REMOTE_IMAGE,
} cirvane_ota_remote_result_t;

cirvane_ota_remote_result_t cirvane_ota_remote_update(const char *version);
const char *cirvane_ota_remote_result_name(cirvane_ota_remote_result_t result);

#endif
