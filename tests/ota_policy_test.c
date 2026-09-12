/* SPDX-License-Identifier: Apache-2.0 */
#include "cirvane_ota_policy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char valid[] =
    "cirvane-update-v1\n"
    "product=cirvane\n"
    "device=seeed-xiao-esp32c5\n"
    "version=v0.1.0\n"
    "sequence=1\n"
    "image_size=1024\n"
    "image_sha256=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
    "image_url=https://github.com/kabudu/cirvane/releases/download/v0.1.0/cirvane-v0.1.0-esp32c5.bin\n"
    "key_id=release-2026-01\n"
    "signature=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";

static void expect_parse(const char *text, cirvane_ota_policy_result_t expected)
{
    cirvane_ota_manifest_t manifest;
    assert(cirvane_ota_manifest_parse(text, strlen(text), &manifest) == expected);
}

int main(void)
{
    cirvane_ota_manifest_t manifest;
    assert(cirvane_ota_manifest_parse(valid, strlen(valid), &manifest) ==
           CIRVANE_OTA_POLICY_OK);
    assert(manifest.sequence == 1);
    assert(manifest.image_size == 1024);
    assert(manifest.signed_len == (size_t)(strstr(valid, "signature=") - valid));
    assert(cirvane_ota_manifest_validate(&manifest, 0, 2048) ==
           CIRVANE_OTA_POLICY_OK);
    assert(cirvane_ota_manifest_validate(&manifest, 1, 2048) ==
           CIRVANE_OTA_POLICY_REPLAY);
    assert(cirvane_ota_manifest_validate(&manifest, 0, 512) ==
           CIRVANE_OTA_POLICY_LENGTH);
    strcpy(manifest.image_url,
           "https://github.com/kabudu/cirvane/releases/download/v0.1.1/cirvane-v0.1.1-esp32c5.bin");
    assert(cirvane_ota_manifest_validate(&manifest, 0, 2048) ==
           CIRVANE_OTA_POLICY_URL);

    assert(cirvane_ota_release_version_valid("v12.34.56"));
    assert(cirvane_ota_release_version_valid("v0.0.0"));
    assert(!cirvane_ota_release_version_valid("0.1.0"));
    assert(!cirvane_ota_release_version_valid("v1.2"));
    assert(!cirvane_ota_release_version_valid("v01.2.3"));
    assert(!cirvane_ota_release_version_valid("v1.02.3"));
    assert(!cirvane_ota_release_version_valid("v1.2.03"));
    assert(!cirvane_ota_release_version_valid("v1.2.3-rc1"));
    assert(cirvane_ota_key_status("release-2026-01") == CIRVANE_OTA_KEY_ACTIVE);
    assert(cirvane_ota_key_status("release-2025-99") == CIRVANE_OTA_KEY_UNKNOWN);
    assert(cirvane_ota_url_allowed(
        "https://github.com/kabudu/cirvane/releases/download/v1.2.3/a.bin", false));
    assert(cirvane_ota_url_allowed(
        "https://release-assets.githubusercontent.com/github-production-release-asset/a", true));
    assert(!cirvane_ota_url_allowed("http://github.com/kabudu/cirvane/a", false));
    assert(!cirvane_ota_url_allowed("https://example.com/a", true));
    assert(cirvane_ota_chunk_fits(0, 1536, 1536));
    assert(cirvane_ota_chunk_fits(1024, 512, 1536));
    assert(!cirvane_ota_chunk_fits(0, 0, 1536));
    assert(!cirvane_ota_chunk_fits(0, 2048, 1536));
    assert(!cirvane_ota_chunk_fits(1536, 1, 1536));

    char changed[sizeof(valid)];
    memcpy(changed, valid, sizeof(valid));
    strstr(changed, "product=cirvane")[8] = 'x';
    expect_parse(changed, CIRVANE_OTA_POLICY_IDENTITY);
    memcpy(changed, valid, sizeof(valid));
    strstr(changed, "device=seeed")[7] = 'x';
    expect_parse(changed, CIRVANE_OTA_POLICY_IDENTITY);
    memcpy(changed, valid, sizeof(valid));
    strstr(changed, "sequence=1")[9] = '0';
    expect_parse(changed, CIRVANE_OTA_POLICY_MALFORMED);
    memcpy(changed, valid, sizeof(valid));
    strstr(changed, "image_sha256=")[13] = 'G';
    expect_parse(changed, CIRVANE_OTA_POLICY_DIGEST);
    memcpy(changed, valid, sizeof(valid));
    changed[strlen(changed) - 1] = '\0';
    expect_parse(changed, CIRVANE_OTA_POLICY_MALFORMED);
    memcpy(changed, valid, sizeof(valid));
    strstr(changed, "version=v0.1.0")[8] = '\r';
    expect_parse(changed, CIRVANE_OTA_POLICY_MALFORMED);

    puts("ota policy tests passed");
    return 0;
}
