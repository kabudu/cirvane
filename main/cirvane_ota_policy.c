/* SPDX-License-Identifier: Apache-2.0 */
#include "cirvane_ota_policy.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool copy_field(const char *line, size_t line_len, const char *prefix,
                       char *out, size_t capacity)
{
    size_t prefix_len = strlen(prefix);
    if (line_len <= prefix_len || line_len - prefix_len >= capacity ||
        memcmp(line, prefix, prefix_len) != 0) {
        return false;
    }
    memcpy(out, line + prefix_len, line_len - prefix_len);
    out[line_len - prefix_len] = '\0';
    return true;
}

static bool parse_u32_field(const char *line, size_t line_len, const char *prefix,
                            uint32_t *out)
{
    char value[11];
    if (!copy_field(line, line_len, prefix, value, sizeof(value)) ||
        value[0] < '1' || value[0] > '9') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed > UINT32_MAX) return false;
    *out = (uint32_t)parsed;
    return true;
}

static int hex_nibble(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static bool parse_hex_field(const char *line, size_t line_len, const char *prefix,
                            uint8_t *out, size_t out_len)
{
    size_t prefix_len = strlen(prefix);
    if (line_len != prefix_len + out_len * 2 ||
        memcmp(line, prefix, prefix_len) != 0) {
        return false;
    }
    for (size_t index = 0; index < out_len; ++index) {
        int high = hex_nibble(line[prefix_len + index * 2]);
        int low = hex_nibble(line[prefix_len + index * 2 + 1]);
        if (high < 0 || low < 0) return false;
        out[index] = (uint8_t)((high << 4) | low);
    }
    return true;
}

static bool next_line(const char *text, size_t length, size_t *offset,
                      const char **line, size_t *line_len)
{
    if (*offset >= length) return false;
    size_t start = *offset;
    while (*offset < length && text[*offset] != '\n') {
        if (text[*offset] == '\r' || text[*offset] == '\0') return false;
        (*offset)++;
    }
    if (*offset >= length || *offset == start) return false;
    *line = text + start;
    *line_len = *offset - start;
    (*offset)++;
    return true;
}

bool cirvane_ota_release_version_valid(const char *version)
{
    if (version == NULL || version[0] != 'v') return false;
    size_t length = strlen(version);
    if (length < 6 || length > CIRVANE_OTA_VERSION_MAX) return false;
    size_t index = 1;
    for (unsigned component = 0; component < 3; ++component) {
        if (index >= length) return false;
        size_t start = index;
        if (index + 1 < length && version[index] == '0' && version[index + 1] >= '0' &&
            version[index + 1] <= '9') return false;
        while (index < length && version[index] >= '0' && version[index] <= '9') {
            index++;
        }
        if (index == start) return false;
        if (component < 2) {
            if (index >= length || version[index] != '.') return false;
            index++;
        }
    }
    return index == length;
}

typedef struct {
    const char *key_id;
    cirvane_ota_key_status_t status;
} ota_key_record_t;

static const ota_key_record_t s_key_registry[] = {
    { "release-2026-01", CIRVANE_OTA_KEY_ACTIVE },
};

cirvane_ota_key_status_t cirvane_ota_key_status(const char *key_id)
{
    if (key_id == NULL) return CIRVANE_OTA_KEY_UNKNOWN;
    for (size_t index = 0; index < sizeof(s_key_registry) / sizeof(s_key_registry[0]);
         ++index) {
        if (strcmp(key_id, s_key_registry[index].key_id) == 0)
            return s_key_registry[index].status;
    }
    return CIRVANE_OTA_KEY_UNKNOWN;
}

bool cirvane_ota_url_allowed(const char *url, bool redirect)
{
    size_t maximum = redirect ? 1024u : CIRVANE_OTA_URL_MAX;
    if (url == NULL || strlen(url) > maximum) return false;
    if (strncmp(url, CIRVANE_OTA_RELEASE_PREFIX,
                strlen(CIRVANE_OTA_RELEASE_PREFIX)) == 0) {
        return true;
    }
    static const char redirect_prefix[] =
        "https://release-assets.githubusercontent.com/";
    return redirect && strncmp(url, redirect_prefix,
                               sizeof(redirect_prefix) - 1) == 0;
}

bool cirvane_ota_chunk_fits(uint32_t received, size_t chunk_size, uint32_t limit)
{
    return chunk_size > 0 && chunk_size <= limit &&
           received <= limit - (uint32_t)chunk_size;
}

cirvane_ota_policy_result_t cirvane_ota_manifest_parse(
    const char *text, size_t length, cirvane_ota_manifest_t *manifest)
{
    static const char header[] = "cirvane-update-v1\n";
    if (text == NULL || manifest == NULL || length < sizeof(header) ||
        length > CIRVANE_OTA_MANIFEST_MAX ||
        memcmp(text, header, sizeof(header) - 1) != 0 || text[length - 1] != '\n') {
        return CIRVANE_OTA_POLICY_MALFORMED;
    }
    memset(manifest, 0, sizeof(*manifest));
    size_t offset = sizeof(header) - 1;
    const char *line = NULL;
    size_t line_len = 0;
    char identity[32];

#define LINE_OR_FAIL() \
    do { if (!next_line(text, length, &offset, &line, &line_len)) \
        return CIRVANE_OTA_POLICY_MALFORMED; } while (0)
    LINE_OR_FAIL();
    if (!copy_field(line, line_len, "product=", identity, sizeof(identity)) ||
        strcmp(identity, CIRVANE_OTA_PRODUCT) != 0) return CIRVANE_OTA_POLICY_IDENTITY;
    LINE_OR_FAIL();
    if (!copy_field(line, line_len, "device=", identity, sizeof(identity)) ||
        strcmp(identity, CIRVANE_OTA_DEVICE) != 0) return CIRVANE_OTA_POLICY_IDENTITY;
    LINE_OR_FAIL();
    if (!copy_field(line, line_len, "version=", manifest->version,
                    sizeof(manifest->version))) return CIRVANE_OTA_POLICY_VERSION;
    LINE_OR_FAIL();
    if (!parse_u32_field(line, line_len, "sequence=", &manifest->sequence))
        return CIRVANE_OTA_POLICY_MALFORMED;
    LINE_OR_FAIL();
    if (!parse_u32_field(line, line_len, "image_size=", &manifest->image_size))
        return CIRVANE_OTA_POLICY_LENGTH;
    LINE_OR_FAIL();
    if (!parse_hex_field(line, line_len, "image_sha256=", manifest->image_sha256,
                         sizeof(manifest->image_sha256))) return CIRVANE_OTA_POLICY_DIGEST;
    LINE_OR_FAIL();
    if (!copy_field(line, line_len, "image_url=", manifest->image_url,
                    sizeof(manifest->image_url))) return CIRVANE_OTA_POLICY_URL;
    LINE_OR_FAIL();
    if (!copy_field(line, line_len, "key_id=", manifest->key_id,
                    sizeof(manifest->key_id))) return CIRVANE_OTA_POLICY_KEY;
    manifest->signed_len = offset;
    LINE_OR_FAIL();
    if (!parse_hex_field(line, line_len, "signature=", manifest->signature,
                         sizeof(manifest->signature))) return CIRVANE_OTA_POLICY_SIGNATURE;
    manifest->signature_len = sizeof(manifest->signature);
    if (offset != length) return CIRVANE_OTA_POLICY_MALFORMED;
#undef LINE_OR_FAIL
    return CIRVANE_OTA_POLICY_OK;
}

cirvane_ota_policy_result_t cirvane_ota_manifest_validate(
    const cirvane_ota_manifest_t *manifest, uint32_t highest_sequence,
    uint32_t partition_size)
{
    if (manifest == NULL) return CIRVANE_OTA_POLICY_MALFORMED;
    if (!cirvane_ota_release_version_valid(manifest->version))
        return CIRVANE_OTA_POLICY_VERSION;
    if (manifest->sequence <= highest_sequence) return CIRVANE_OTA_POLICY_REPLAY;
    if (manifest->image_size == 0 || manifest->image_size > partition_size)
        return CIRVANE_OTA_POLICY_LENGTH;
    if (!cirvane_ota_url_allowed(manifest->image_url, false))
        return CIRVANE_OTA_POLICY_URL;
    char expected_url[CIRVANE_OTA_URL_MAX + 1];
    int written = snprintf(expected_url, sizeof(expected_url),
                           "%s%s/cirvane-%s-esp32c5.bin",
                           CIRVANE_OTA_RELEASE_PREFIX, manifest->version,
                           manifest->version);
    if (written < 0 || (size_t)written >= sizeof(expected_url) ||
        strcmp(manifest->image_url, expected_url) != 0)
        return CIRVANE_OTA_POLICY_URL;
    if (cirvane_ota_key_status(manifest->key_id) != CIRVANE_OTA_KEY_ACTIVE)
        return CIRVANE_OTA_POLICY_KEY;
    if (manifest->signature_len != CIRVANE_OTA_SIGNATURE_MAX)
        return CIRVANE_OTA_POLICY_SIGNATURE;
    return CIRVANE_OTA_POLICY_OK;
}

const char *cirvane_ota_policy_result_name(cirvane_ota_policy_result_t result)
{
    switch (result) {
    case CIRVANE_OTA_POLICY_OK: return "ok";
    case CIRVANE_OTA_POLICY_MALFORMED: return "manifest-malformed";
    case CIRVANE_OTA_POLICY_IDENTITY: return "identity-rejected";
    case CIRVANE_OTA_POLICY_VERSION: return "version-rejected";
    case CIRVANE_OTA_POLICY_REPLAY: return "replay-rejected";
    case CIRVANE_OTA_POLICY_LENGTH: return "length-rejected";
    case CIRVANE_OTA_POLICY_DIGEST: return "digest-rejected";
    case CIRVANE_OTA_POLICY_URL: return "origin-rejected";
    case CIRVANE_OTA_POLICY_KEY: return "key-rejected";
    case CIRVANE_OTA_POLICY_SIGNATURE: return "signature-rejected";
    default: return "unknown";
    }
}
