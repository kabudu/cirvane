/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CIRVANE_OTA_POLICY_H
#define CIRVANE_OTA_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CIRVANE_OTA_MANIFEST_MAX 1536u
#define CIRVANE_OTA_VERSION_MAX 31u
#define CIRVANE_OTA_URL_MAX 255u
#define CIRVANE_OTA_KEY_ID_MAX 31u
#define CIRVANE_OTA_SIGNATURE_MAX 64u
#define CIRVANE_OTA_SHA256_SIZE 32u
#define CIRVANE_OTA_PRODUCT "cirvane"
#define CIRVANE_OTA_DEVICE "seeed-xiao-esp32c5"
#define CIRVANE_OTA_RELEASE_PREFIX \
    "https://github.com/kabudu/cirvane/releases/download/"

typedef enum {
    CIRVANE_OTA_POLICY_OK = 0,
    CIRVANE_OTA_POLICY_MALFORMED,
    CIRVANE_OTA_POLICY_IDENTITY,
    CIRVANE_OTA_POLICY_VERSION,
    CIRVANE_OTA_POLICY_REPLAY,
    CIRVANE_OTA_POLICY_LENGTH,
    CIRVANE_OTA_POLICY_DIGEST,
    CIRVANE_OTA_POLICY_URL,
    CIRVANE_OTA_POLICY_KEY,
    CIRVANE_OTA_POLICY_SIGNATURE,
} cirvane_ota_policy_result_t;

typedef enum {
    CIRVANE_OTA_KEY_UNKNOWN = 0,
    CIRVANE_OTA_KEY_ACTIVE,
    CIRVANE_OTA_KEY_REVOKED,
} cirvane_ota_key_status_t;

typedef struct {
    char version[CIRVANE_OTA_VERSION_MAX + 1];
    uint32_t sequence;
    uint32_t image_size;
    uint8_t image_sha256[CIRVANE_OTA_SHA256_SIZE];
    char image_url[CIRVANE_OTA_URL_MAX + 1];
    char key_id[CIRVANE_OTA_KEY_ID_MAX + 1];
    uint8_t signature[CIRVANE_OTA_SIGNATURE_MAX];
    size_t signature_len;
    size_t signed_len;
} cirvane_ota_manifest_t;

cirvane_ota_policy_result_t cirvane_ota_manifest_parse(
    const char *text, size_t length, cirvane_ota_manifest_t *manifest);
cirvane_ota_policy_result_t cirvane_ota_manifest_validate(
    const cirvane_ota_manifest_t *manifest, uint32_t highest_sequence,
    uint32_t partition_size);
bool cirvane_ota_release_version_valid(const char *version);
bool cirvane_ota_url_allowed(const char *url, bool redirect);
bool cirvane_ota_chunk_fits(uint32_t received, size_t chunk_size, uint32_t limit);
cirvane_ota_key_status_t cirvane_ota_key_status(const char *key_id);
const char *cirvane_ota_policy_result_name(cirvane_ota_policy_result_t result);

#endif
