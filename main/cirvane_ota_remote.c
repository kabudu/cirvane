/* SPDX-License-Identifier: Apache-2.0 */
#include "cirvane_ota_remote.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "cirvane_ota_policy.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "psa/crypto.h"

#define OTA_HTTP_TIMEOUT_MS 10000
#define OTA_HTTP_BUFFER_SIZE 2048
#define OTA_MAX_REDIRECTS 3
#define OTA_MANIFEST_DEADLINE_US 30000000LL
#define OTA_IMAGE_DEADLINE_US 180000000LL
#define OTA_NVS_NAMESPACE "cirvane_ota"
#define OTA_NVS_SEQUENCE "highest_seq"

typedef enum { DOWNLOAD_MANIFEST, DOWNLOAD_IMAGE } download_kind_t;

typedef struct {
    download_kind_t kind;
    uint8_t *manifest_buffer;
    esp_ota_handle_t ota_handle;
    psa_hash_operation_t *sha;
    uint32_t limit;
    uint32_t expected_length;
    uint32_t received;
    int status;
    int redirects;
    bool headers_accepted;
    bool location_allowed;
    bool location_seen;
    bool content_length_seen;
    bool transfer_encoding_seen;
    bool failed;
    int64_t deadline_us;
} download_context_t;

static StaticSemaphore_t s_ota_lock_storage;
static SemaphoreHandle_t s_ota_lock;
static uint8_t s_manifest_text[CIRVANE_OTA_MANIFEST_MAX + 1];
static cirvane_ota_manifest_t s_manifest;

/* Uncompressed P-256 public point for key id release-2026-01. The private
 * key is owner-held outside the repository and is never required by devices. */
static const uint8_t s_release_2026_01_public_key[65] = {
    0x04, 0x85, 0x25, 0x7c, 0xe0, 0x67, 0x6c, 0xce,
    0xef, 0x8c, 0xb8, 0x42, 0x09, 0xa1, 0xf7, 0xb1,
    0xac, 0xba, 0xe0, 0xe0, 0xa4, 0xab, 0xc4, 0x31,
    0x3c, 0x58, 0x8f, 0x9e, 0xcf, 0x54, 0xca, 0x0f,
    0xfb, 0x71, 0xc3, 0x21, 0x1c, 0xef, 0xbf, 0xc3,
    0x8c, 0x54, 0x0d, 0x99, 0xcf, 0x9a, 0x5f, 0x2e,
    0x03, 0xf3, 0xdd, 0xac, 0xe8, 0x48, 0x70, 0xb5,
    0xb2, 0xf4, 0x1b, 0x5c, 0x66, 0x70, 0x7a, 0x7f,
    0x5e,
};

static bool status_is_redirect(int status)
{
    return status == 301 || status == 302 || status == 303 ||
           status == 307 || status == 308;
}

static esp_err_t download_event(esp_http_client_event_t *event)
{
    download_context_t *context = event->user_data;
    if (context == NULL || context->failed) return ESP_FAIL;
    if (esp_timer_get_time() > context->deadline_us) {
        context->failed = true;
        return ESP_ERR_TIMEOUT;
    }
    switch (event->event_id) {
    case HTTP_EVENT_ON_STATUS_CODE:
        context->status = esp_http_client_get_status_code(event->client);
        context->headers_accepted = false;
        context->location_allowed = false;
        context->location_seen = false;
        context->content_length_seen = false;
        context->transfer_encoding_seen = false;
        break;
    case HTTP_EVENT_ON_HEADER:
        if (event->header_key == NULL || event->header_value == NULL) break;
        if (strcasecmp(event->header_key, "Content-Length") == 0) {
            if (context->content_length_seen) {
                context->failed = true;
                return ESP_FAIL;
            }
            context->content_length_seen = true;
        } else if (strcasecmp(event->header_key, "Transfer-Encoding") == 0) {
            context->transfer_encoding_seen = true;
        } else if (context->status && status_is_redirect(context->status) &&
                   strcasecmp(event->header_key, "Location") == 0) {
            if (context->location_seen) {
                context->failed = true;
                return ESP_FAIL;
            }
            context->location_seen = true;
            context->location_allowed =
                cirvane_ota_url_allowed(event->header_value, true);
        }
        break;
    case HTTP_EVENT_ON_HEADERS_COMPLETE: {
        if (context->status != 200) break;
        int64_t length = esp_http_client_get_content_length(event->client);
        if (!context->content_length_seen || context->transfer_encoding_seen ||
            length <= 0 || (uint64_t)length > context->limit ||
            (context->expected_length != 0 &&
             (uint64_t)length != context->expected_length)) {
            context->failed = true;
            return ESP_FAIL;
        }
        context->headers_accepted = true;
        break;
    }
    case HTTP_EVENT_ON_DATA:
        if (status_is_redirect(context->status) && context->location_allowed) break;
        if (!context->headers_accepted || event->data_len <= 0 ||
            !cirvane_ota_chunk_fits(context->received,
                                    (size_t)event->data_len, context->limit)) {
            context->failed = true;
            return ESP_FAIL;
        }
        if (context->kind == DOWNLOAD_MANIFEST) {
            memcpy(context->manifest_buffer + context->received,
                   event->data, (size_t)event->data_len);
        } else {
            if (esp_ota_write(context->ota_handle, event->data,
                              (size_t)event->data_len) != ESP_OK ||
                psa_hash_update(context->sha, event->data,
                                (size_t)event->data_len) != PSA_SUCCESS) {
                context->failed = true;
                return ESP_FAIL;
            }
        }
        context->received += (uint32_t)event->data_len;
        break;
    case HTTP_EVENT_REDIRECT:
        if (!context->location_seen || !context->location_allowed ||
            ++context->redirects > OTA_MAX_REDIRECTS ||
            esp_http_client_set_redirection(event->client) != ESP_OK) {
            context->failed = true;
            return ESP_FAIL;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t perform_download(const char *url, download_context_t *context)
{
    if (!cirvane_ota_url_allowed(url, false)) return ESP_ERR_INVALID_ARG;
    context->deadline_us = esp_timer_get_time() +
        (context->kind == DOWNLOAD_MANIFEST ? OTA_MANIFEST_DEADLINE_US
                                            : OTA_IMAGE_DEADLINE_US);
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = OTA_HTTP_TIMEOUT_MS,
        .disable_auto_redirect = true,
        .max_redirection_count = OTA_MAX_REDIRECTS,
        .max_authorization_retries = -1,
        .buffer_size = OTA_HTTP_BUFFER_SIZE,
        .buffer_size_tx = OTA_HTTP_BUFFER_SIZE,
        .event_handler = download_event,
        .user_data = context,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_http_client_perform(client);
    int final_status = esp_http_client_get_status_code(client);
    bool complete = esp_http_client_is_complete_data_received(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK || context->failed || final_status != 200 || !complete ||
        context->received == 0 ||
        (context->expected_length != 0 &&
         context->received != context->expected_length)) {
        return result == ESP_OK ? ESP_FAIL : result;
    }
    return ESP_OK;
}

static esp_err_t read_highest_sequence(uint32_t *sequence)
{
    nvs_handle_t nvs = 0;
    esp_err_t error = nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        *sequence = CONFIG_CIRVANE_RELEASE_SEQUENCE;
        return ESP_OK;
    }
    if (error != ESP_OK) return error;
    error = nvs_get_u32(nvs, OTA_NVS_SEQUENCE, sequence);
    nvs_close(nvs);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        *sequence = CONFIG_CIRVANE_RELEASE_SEQUENCE;
        return ESP_OK;
    }
#if CONFIG_CIRVANE_RELEASE_SEQUENCE > 0
    if (error == ESP_OK && *sequence < (uint32_t)CONFIG_CIRVANE_RELEASE_SEQUENCE)
        *sequence = (uint32_t)CONFIG_CIRVANE_RELEASE_SEQUENCE;
#endif
    return error;
}

static esp_err_t persist_highest_sequence(uint32_t sequence)
{
    nvs_handle_t nvs = 0;
    esp_err_t error = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (error == ESP_OK) error = nvs_set_u32(nvs, OTA_NVS_SEQUENCE, sequence);
    if (error == ESP_OK) error = nvs_commit(nvs);
    if (nvs != 0) nvs_close(nvs);
    return error;
}

static bool verify_manifest_signature(const char *text,
                                      const cirvane_ota_manifest_t *manifest)
{
    uint8_t digest[32];
    size_t digest_len = 0;
    if (psa_crypto_init() != PSA_SUCCESS ||
        psa_hash_compute(PSA_ALG_SHA_256, (const uint8_t *)text,
                         manifest->signed_len, digest, sizeof(digest),
                         &digest_len) != PSA_SUCCESS || digest_len != sizeof(digest)) {
        return false;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_key_id_t key = 0;
    psa_status_t result = psa_import_key(&attributes, s_release_2026_01_public_key,
                                         sizeof(s_release_2026_01_public_key), &key);
    if (result == PSA_SUCCESS) {
        result = psa_verify_hash(key, PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                 digest, sizeof(digest), manifest->signature,
                                 manifest->signature_len);
        psa_destroy_key(key);
    }
    psa_reset_key_attributes(&attributes);
    return result == PSA_SUCCESS;
}

cirvane_ota_remote_result_t cirvane_ota_remote_update(const char *version)
{
    if (!cirvane_ota_release_version_valid(version)) return CIRVANE_OTA_REMOTE_POLICY;
    if (s_ota_lock == NULL) {
        s_ota_lock = xSemaphoreCreateMutexStatic(&s_ota_lock_storage);
    }
    if (xSemaphoreTake(s_ota_lock, 0) != pdTRUE) return CIRVANE_OTA_REMOTE_BUSY;

    cirvane_ota_remote_result_t result = CIRVANE_OTA_REMOTE_TRANSPORT;
    char manifest_url[CIRVANE_OTA_URL_MAX + 1];
    int written = snprintf(manifest_url, sizeof(manifest_url), "%s%s/cirvane-%s.manifest",
                           CIRVANE_OTA_RELEASE_PREFIX, version, version);
    if (written < 0 || (size_t)written >= sizeof(manifest_url)) {
        result = CIRVANE_OTA_REMOTE_POLICY;
        goto done;
    }

    download_context_t manifest_download = {
        .kind = DOWNLOAD_MANIFEST,
        .manifest_buffer = s_manifest_text,
        .limit = CIRVANE_OTA_MANIFEST_MAX,
    };
    if (perform_download(manifest_url, &manifest_download) != ESP_OK) goto done;
    s_manifest_text[manifest_download.received] = '\0';

    cirvane_ota_policy_result_t policy = cirvane_ota_manifest_parse(
        (const char *)s_manifest_text, manifest_download.received, &s_manifest);
    if (policy != CIRVANE_OTA_POLICY_OK) {
        result = CIRVANE_OTA_REMOTE_MANIFEST;
        goto done;
    }
    if (strcmp(s_manifest.version, version) != 0) {
        result = CIRVANE_OTA_REMOTE_POLICY;
        goto done;
    }
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL) {
        result = CIRVANE_OTA_REMOTE_STORAGE;
        goto done;
    }
    uint32_t highest_sequence = 0;
    if (read_highest_sequence(&highest_sequence) != ESP_OK) {
        result = CIRVANE_OTA_REMOTE_STORAGE;
        goto done;
    }
    policy = cirvane_ota_manifest_validate(&s_manifest, highest_sequence, target->size);
    if (policy != CIRVANE_OTA_POLICY_OK) {
        printf("ota refused: %s\n", cirvane_ota_policy_result_name(policy));
        result = CIRVANE_OTA_REMOTE_POLICY;
        goto done;
    }
    if (!verify_manifest_signature((const char *)s_manifest_text, &s_manifest)) {
        result = CIRVANE_OTA_REMOTE_SIGNATURE;
        goto done;
    }

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(target, s_manifest.image_size, &handle) != ESP_OK) {
        result = CIRVANE_OTA_REMOTE_STORAGE;
        goto done;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        esp_ota_abort(handle);
        result = CIRVANE_OTA_REMOTE_IMAGE;
        goto done;
    }
    psa_hash_operation_t image_sha = PSA_HASH_OPERATION_INIT;
    if (psa_hash_setup(&image_sha, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        esp_ota_abort(handle);
        result = CIRVANE_OTA_REMOTE_IMAGE;
        goto done;
    }
    download_context_t image_download = {
        .kind = DOWNLOAD_IMAGE,
        .ota_handle = handle,
        .sha = &image_sha,
        .limit = s_manifest.image_size,
        .expected_length = s_manifest.image_size,
    };
    esp_err_t error = perform_download(s_manifest.image_url, &image_download);
    uint8_t image_digest[32];
    size_t image_digest_len = 0;
    if (error == ESP_OK &&
        (psa_hash_finish(&image_sha, image_digest, sizeof(image_digest),
                         &image_digest_len) != PSA_SUCCESS ||
         image_digest_len != sizeof(image_digest))) {
        error = ESP_FAIL;
    }
    if (error != ESP_OK) psa_hash_abort(&image_sha);
    if (error != ESP_OK || memcmp(image_digest, s_manifest.image_sha256,
                                  sizeof(image_digest)) != 0) {
        esp_ota_abort(handle);
        result = CIRVANE_OTA_REMOTE_IMAGE;
        goto done;
    }
    if (esp_ota_end(handle) != ESP_OK) {
        result = CIRVANE_OTA_REMOTE_IMAGE;
        goto done;
    }
    if (persist_highest_sequence(s_manifest.sequence) != ESP_OK) {
        result = CIRVANE_OTA_REMOTE_STORAGE;
        goto done;
    }
    if (esp_ota_set_boot_partition(target) != ESP_OK) {
        result = CIRVANE_OTA_REMOTE_STORAGE;
        goto done;
    }
    result = CIRVANE_OTA_REMOTE_OK;

done:
    xSemaphoreGive(s_ota_lock);
    return result;
}

const char *cirvane_ota_remote_result_name(cirvane_ota_remote_result_t result)
{
    switch (result) {
    case CIRVANE_OTA_REMOTE_OK: return "ready-to-reboot";
    case CIRVANE_OTA_REMOTE_BUSY: return "busy";
    case CIRVANE_OTA_REMOTE_NETWORK: return "network-unavailable";
    case CIRVANE_OTA_REMOTE_TRANSPORT: return "transport-rejected";
    case CIRVANE_OTA_REMOTE_MANIFEST: return "manifest-rejected";
    case CIRVANE_OTA_REMOTE_SIGNATURE: return "signature-rejected";
    case CIRVANE_OTA_REMOTE_POLICY: return "policy-rejected";
    case CIRVANE_OTA_REMOTE_STORAGE: return "storage-failed";
    case CIRVANE_OTA_REMOTE_IMAGE: return "image-rejected";
    default: return "unknown";
    }
}
