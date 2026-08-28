/*
 * Cirvane bounded runtime for the Seeed Studio XIAO ESP32-C5.
 * Powered by ESP-IDF and FreeRTOS.
 *
 * All long-lived kernel objects are statically allocated. NVS configuration
 * uses two CRC-protected slots. Supervision is periodic and bounded by the
 * fixed service count; failures cannot create unbounded restart loops.
 */
#include "cirvane_os.h"

#include <inttypes.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_crc.h"
#include "esp_heap_caps.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"

#define CIRVANE_MAILBOX_DEPTH 4
#define CIRVANE_TASK_STACK_BYTES 4096
#define CIRVANE_SUPERVISOR_STACK_BYTES 6144
#define CIRVANE_SUPERVISOR_PERIOD_MS 1000
#define CIRVANE_RESTART_LIMIT 3
#define CIRVANE_RESTART_BASE_MS 1000
#define CIRVANE_CONFIG_SCHEMA 1
#define CIRVANE_LED_GPIO GPIO_NUM_27

static const char *TAG = "cirvane_os";

typedef struct {
    cirvane_service_t desc;
    svc_info_t info;
    TaskHandle_t task;
    StaticTask_t task_tcb;
    StackType_t task_stack[CIRVANE_TASK_STACK_BYTES / sizeof(StackType_t)];
    StaticQueue_t mailbox_cb;
    uint8_t mailbox_storage[CIRVANE_MAILBOX_DEPTH * sizeof(cirvane_msg_t *)];
    QueueHandle_t mailbox;
    uint32_t capabilities;
    uint32_t heap_baseline;
    uint32_t runtime_baseline;
    uint32_t radio_ms;
    uint32_t restart_at_ms;
    cirvane_health_t reported_health;
    bool registered;
} service_slot_t;

typedef struct {
    uint32_t generation;
    cirvane_config_t config;
    uint32_t crc;
} config_record_t;

static service_slot_t s_services[CIRVANE_MAX_SERVICES];
static uint8_t s_service_indices[CIRVANE_MAX_SERVICES];
static uint8_t s_service_count;
static uint16_t s_subscribers[CIRVANE_MSG_TYPE_MAX];

static cirvane_msg_t s_msg_pool[CIRVANE_MSG_POOL_SLOTS];
static uint32_t s_msg_free_mask;
static portMUX_TYPE s_bus_lock = portMUX_INITIALIZER_UNLOCKED;
static cirvane_bus_stats_t s_bus_stats;

static StaticTask_t s_supervisor_tcb;
static StackType_t s_supervisor_stack[CIRVANE_SUPERVISOR_STACK_BYTES /
                                     sizeof(StackType_t)];
static TaskHandle_t s_supervisor_task;
static cirvane_boot_timing_t s_boot;
static cirvane_sys_state_t s_system_state = CIRVANE_SYS_NOMINAL;
static cirvane_config_t s_config;
static uint32_t s_config_generation;
static cirvane_budget_t s_budget = CIRVANE_BUDGET_ACTIVE;
static uint32_t s_sleep_after_ms;
static bool s_started;
#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
static uint8_t s_ota_copy_block[1024];
#endif

static uint32_t record_crc(const config_record_t *record)
{
    return esp_crc32_le(UINT32_MAX, (const uint8_t *)record,
                        offsetof(config_record_t, crc));
}

static bool config_valid(const config_record_t *record)
{
    return record->config.schema_version == CIRVANE_CONFIG_SCHEMA &&
           record->config.heartbeat_ms >= 1000 &&
           record->config.heartbeat_ms <= 3600000 &&
           record->config.supervisor_ms >= 100 &&
           record->config.supervisor_ms <= 60000 &&
           record->config.default_led_mode <= 2 &&
           record->crc == record_crc(record);
}

static cirvane_config_t config_defaults(void)
{
    return (cirvane_config_t){
        .schema_version = CIRVANE_CONFIG_SCHEMA,
        .heartbeat_ms = 10000,
        .supervisor_ms = CIRVANE_SUPERVISOR_PERIOD_MS,
        .default_led_mode = 2,
    };
}

uint32_t cirvane_uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

void cirvane_bus_init(void)
{
    taskENTER_CRITICAL(&s_bus_lock);
    s_msg_free_mask = UINT32_MAX;
    memset(&s_bus_stats, 0, sizeof(s_bus_stats));
    memset(s_subscribers, 0, sizeof(s_subscribers));
    taskEXIT_CRITICAL(&s_bus_lock);
}

int cirvane_bus_subscribe(uint8_t service_idx, uint8_t type)
{
    if (service_idx >= s_service_count || type == CIRVANE_MSG_NONE ||
        type >= CIRVANE_MSG_TYPE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    taskENTER_CRITICAL(&s_bus_lock);
    s_subscribers[type] |= (uint16_t)(1u << service_idx);
    taskEXIT_CRITICAL(&s_bus_lock);
    return ESP_OK;
}

cirvane_msg_t *cirvane_bus_alloc(void)
{
    cirvane_msg_t *result = NULL;
    taskENTER_CRITICAL(&s_bus_lock);
    if (s_msg_free_mask != 0) {
        unsigned idx = (unsigned)__builtin_ctz(s_msg_free_mask);
        s_msg_free_mask &= ~(1u << idx);
        unsigned used = CIRVANE_MSG_POOL_SLOTS - (unsigned)__builtin_popcount(s_msg_free_mask);
        if (used > s_bus_stats.peak_used) {
            s_bus_stats.peak_used = (uint16_t)used;
        }
        result = &s_msg_pool[idx];
        memset(result, 0, sizeof(*result));
    } else {
        s_bus_stats.alloc_fails++;
    }
    taskEXIT_CRITICAL(&s_bus_lock);
    return result;
}

void cirvane_bus_free(cirvane_msg_t *msg)
{
    if (msg == NULL || msg < s_msg_pool || msg >= s_msg_pool + CIRVANE_MSG_POOL_SLOTS) {
        return;
    }
    unsigned idx = (unsigned)(msg - s_msg_pool);
    taskENTER_CRITICAL(&s_bus_lock);
    s_msg_free_mask |= 1u << idx;
    taskEXIT_CRITICAL(&s_bus_lock);
}

void cirvane_bus_publish(cirvane_msg_t *msg)
{
    if (msg == NULL || msg->type == CIRVANE_MSG_NONE ||
        msg->type >= CIRVANE_MSG_TYPE_COUNT ||
        msg->payload_len > CIRVANE_MSG_PAYLOAD_MAX) {
        cirvane_bus_free(msg);
        return;
    }

    uint16_t subscribers;
    taskENTER_CRITICAL(&s_bus_lock);
    subscribers = s_subscribers[msg->type];
    taskEXIT_CRITICAL(&s_bus_lock);

    for (uint8_t idx = 0; idx < s_service_count; ++idx) {
        if ((subscribers & (1u << idx)) == 0) {
            continue;
        }
        cirvane_msg_t *copy = cirvane_bus_alloc();
        if (copy == NULL) {
            continue;
        }
        *copy = *msg;
        if (xQueueSend(s_services[idx].mailbox, (const void *)&copy, 0) != pdTRUE) {
            taskENTER_CRITICAL(&s_bus_lock);
            s_bus_stats.total_drops++;
            taskEXIT_CRITICAL(&s_bus_lock);
            cirvane_bus_free(copy);
        }
    }
    cirvane_bus_free(msg);
}

cirvane_msg_t *cirvane_bus_recv(uint8_t service_idx, uint32_t wait_ms)
{
    if (service_idx >= s_service_count || s_services[service_idx].mailbox == NULL) {
        return NULL;
    }
    cirvane_msg_t *msg = NULL;
    return xQueueReceive(s_services[service_idx].mailbox, (void *)&msg,
                         pdMS_TO_TICKS(wait_ms)) == pdTRUE ? msg : NULL;
}

void cirvane_bus_get_stats(cirvane_bus_stats_t *out)
{
    if (out == NULL) {
        return;
    }
    taskENTER_CRITICAL(&s_bus_lock);
    *out = s_bus_stats;
    out->free_slots = (uint16_t)__builtin_popcount(s_msg_free_mask);
    taskEXIT_CRITICAL(&s_bus_lock);
}

bool cirvane_caps_check(uint8_t service_idx, uint32_t needed_cap)
{
    uint32_t held = service_idx == CIRVANE_SERVICE_INVALID
                        ? CIRVANE_CAP_OPERATOR_ALL
                        : cirvane_service_caps(service_idx);
    return (held & needed_cap) == needed_cap;
}

uint32_t cirvane_service_caps(uint8_t service_idx)
{
    return service_idx < s_service_count ? s_services[service_idx].capabilities : 0;
}

void cirvane_service_report_health(uint8_t service_idx, cirvane_health_t health)
{
    if (service_idx < s_service_count && health <= CIRVANE_HEALTH_FAILED) {
        s_services[service_idx].reported_health = health;
    }
}

int cirvane_config_load(cirvane_config_t *out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "null config output");
    config_record_t records[2] = {0};
    bool valid[2] = {false, false};
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("cirvane_v2", NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        for (unsigned i = 0; i < 2; ++i) {
            char key[8];
            snprintf(key, sizeof(key), "cfg%u", i);
            size_t size = sizeof(records[i]);
            if (nvs_get_blob(nvs, key, &records[i], &size) == ESP_OK &&
                size == sizeof(records[i])) {
                valid[i] = config_valid(&records[i]);
            }
        }
        nvs_close(nvs);
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }

    int selected = valid[0] && valid[1]
                       ? (records[1].generation > records[0].generation ? 1 : 0)
                       : valid[1] ? 1 : valid[0] ? 0 : -1;
    if (selected < 0) {
        s_config = config_defaults();
        s_config_generation = 0;
    } else {
        s_config = records[selected].config;
        s_config_generation = records[selected].generation;
    }
    *out = s_config;
    return ESP_OK;
}

int cirvane_config_commit(const cirvane_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "null config");
    config_record_t record = {
        .generation = s_config_generation + 1,
        .config = *config,
    };
    record.crc = record_crc(&record);
    ESP_RETURN_ON_FALSE(config_valid(&record), ESP_ERR_INVALID_ARG, TAG, "invalid config");

    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open("cirvane_v2", NVS_READWRITE, &nvs), TAG, "open config NVS");
    char key[8];
    snprintf(key, sizeof(key), "cfg%u", (unsigned)(record.generation & 1u));
    esp_err_t err = nvs_set_blob(nvs, key, &record, sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    if (err == ESP_OK) {
        config_record_t verify = {0};
        size_t size = sizeof(verify);
        err = nvs_get_blob(nvs, key, &verify, &size);
        if (err == ESP_OK && (size != sizeof(verify) || !config_valid(&verify) ||
                              verify.generation != record.generation)) {
            err = ESP_ERR_INVALID_CRC;
        }
    }
    nvs_close(nvs);
    if (err == ESP_OK) {
        s_config = record.config;
        s_config_generation = record.generation;
    }
    return err;
}

#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
int cirvane_config_corruption_test(void)
{
    cirvane_config_t original = s_config;
    uint32_t original_generation = s_config_generation;
    config_record_t corrupt = {
        .generation = original_generation + 1,
        .config = original,
    };
    corrupt.crc = record_crc(&corrupt) ^ 1u;

    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open("cirvane_v2", NVS_READWRITE, &nvs), TAG,
                        "open config NVS for corruption test");
    char key[8];
    snprintf(key, sizeof(key), "cfg%u", (unsigned)(corrupt.generation & 1u));
    esp_err_t err = nvs_set_blob(nvs, key, &corrupt, sizeof(corrupt));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) return err;

    cirvane_config_t loaded;
    err = cirvane_config_load(&loaded);
    if (err != ESP_OK || s_config_generation != original_generation ||
        memcmp(&loaded, &original, sizeof(original)) != 0) {
        return ESP_ERR_INVALID_CRC;
    }
    return cirvane_config_commit(&original);
}
#endif

void cirvane_config_get(cirvane_config_t *out)
{
    if (out != NULL) {
        *out = s_config;
    }
}

const char *cirvane_budget_name(cirvane_budget_t budget)
{
    switch (budget) {
    case CIRVANE_BUDGET_ACTIVE: return "active";
    case CIRVANE_BUDGET_WATCHDOG: return "watchdog";
    case CIRVANE_BUDGET_SENTINEL: return "sentinel";
    default: return "invalid";
    }
}

cirvane_budget_t cirvane_power_current_budget(void)
{
    return s_budget;
}

int cirvane_power_set_budget(cirvane_budget_t budget)
{
    if (budget > CIRVANE_BUDGET_SENTINEL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_budget = budget;
    if (budget == CIRVANE_BUDGET_ACTIVE) {
        s_sleep_after_ms = 0;
        esp_wifi_start();
    } else {
        esp_wifi_stop();
        gpio_set_level(CIRVANE_LED_GPIO, 0);
    }
    return ESP_OK;
}

int cirvane_power_schedule(cirvane_budget_t budget, uint32_t wake_after_ms)
{
    if (budget == CIRVANE_BUDGET_ACTIVE || budget > CIRVANE_BUDGET_SENTINEL ||
        wake_after_ms < 100 || wake_after_ms > CIRVANE_MAX_SLEEP_MS) {
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    /* ESP32-C5 cannot retain its USB Serial/JTAG peripheral across light
     * sleep. A manual light-sleep call wakes the CPU but permanently loses
     * this board's only operator console until reset, so refuse that unsafe
     * combination instead of silently stranding the shell. */
    if (budget == CIRVANE_BUDGET_WATCHDOG) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif
    ESP_RETURN_ON_ERROR(cirvane_power_set_budget(budget), TAG, "set budget");
    s_sleep_after_ms = wake_after_ms;
    return ESP_OK;
}

int cirvane_ota_confirm(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    esp_err_t err = esp_ota_get_state_partition(running, &state);
    if (err != ESP_OK) {
        return err;
    }
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        return esp_ota_mark_app_valid_cancel_rollback();
    }
    return state == ESP_OTA_IMG_VALID ? ESP_OK : ESP_ERR_INVALID_STATE;
}

const char *cirvane_ota_state_name(int state)
{
    switch ((esp_ota_img_states_t)state) {
    case ESP_OTA_IMG_NEW: return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending_verify";
    case ESP_OTA_IMG_VALID: return "valid";
    case ESP_OTA_IMG_INVALID: return "invalid";
    case ESP_OTA_IMG_ABORTED: return "aborted";
    case ESP_OTA_IMG_UNDEFINED: return "undefined";
    default: return "unknown";
    }
}

#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
static int ota_copy_running(bool corrupt_for_test)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *boot_before = esp_ota_get_boot_partition();
    ESP_RETURN_ON_FALSE(running != NULL && target != NULL, ESP_ERR_NOT_FOUND,
                        TAG, "OTA partition unavailable");

    const esp_partition_pos_t running_pos = {
        .offset = running->address,
        .size = running->size,
    };
    esp_image_metadata_t metadata = {0};
    ESP_RETURN_ON_ERROR(esp_image_verify(ESP_IMAGE_VERIFY, &running_pos, &metadata),
                        TAG, "verify running image");
    ESP_RETURN_ON_FALSE(metadata.image_len > 0 && metadata.image_len <= target->size,
                        ESP_ERR_INVALID_SIZE, TAG, "running image does not fit");

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(target, metadata.image_len, &handle);
    if (err != ESP_OK) {
        return err;
    }

    for (uint32_t offset = 0; offset < metadata.image_len;
         offset += sizeof(s_ota_copy_block)) {
        size_t length = metadata.image_len - offset;
        if (length > sizeof(s_ota_copy_block)) {
            length = sizeof(s_ota_copy_block);
        }
        err = esp_partition_read(running, offset, s_ota_copy_block, length);
        if (err == ESP_OK && corrupt_for_test && offset <= 4096u &&
            4096u < offset + length) {
            s_ota_copy_block[4096u - offset] ^= 1u;
        }
        if (err == ESP_OK) {
            err = esp_ota_write(handle, s_ota_copy_block, length);
        }
        if (err != ESP_OK) {
            esp_ota_abort(handle);
            return err;
        }
    }

    err = esp_ota_end(handle);
    if (corrupt_for_test) {
        const esp_partition_t *boot_after = esp_ota_get_boot_partition();
        bool boot_unchanged = boot_before != NULL && boot_after != NULL &&
                              boot_before->address == boot_after->address;
        return err != ESP_OK && boot_unchanged ? ESP_OK : ESP_FAIL;
    }
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(target);
    }
    return err;
}

int cirvane_ota_stage_self(void)
{
    return ota_copy_running(false);
}

int cirvane_ota_reject_corrupt_test(void)
{
    return ota_copy_running(true);
}
#endif

static void service_task(void *arg)
{
    uint8_t idx = *(const uint8_t *)arg;
    service_slot_t *slot = &s_services[idx];
    slot->desc.init(slot->desc.ctx);
    slot->info.phase = SVC_PHASE_RUNNING;
    slot->reported_health = CIRVANE_HEALTH_OK;

    taskENTER_CRITICAL(&s_bus_lock);
    uint32_t first_tick = cirvane_uptime_ms();
    if (s_boot.first_tick_ms == 0 || first_tick < s_boot.first_tick_ms) {
        s_boot.first_tick_ms = first_tick;
    }
    taskEXIT_CRITICAL(&s_bus_lock);

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        uint64_t started = esp_timer_get_time();
        if (slot->desc.tick != NULL) {
            slot->desc.tick(slot->desc.ctx);
        }
        slot->info.last_run_ms = (uint32_t)((esp_timer_get_time() - started) / 1000ULL);
        if (slot->info.last_run_ms > slot->info.max_run_ms) {
            slot->info.max_run_ms = slot->info.last_run_ms;
        }
        slot->info.last_health = slot->reported_health;
        slot->info.next_tick_ms = cirvane_uptime_ms() + slot->desc.period_ms;
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(slot->desc.period_ms));
    }
}

static void start_service(uint8_t idx)
{
    service_slot_t *slot = &s_services[idx];
    s_service_indices[idx] = idx;
    slot->reported_health = CIRVANE_HEALTH_OK;
    uint32_t stack_words = slot->desc.stack_size / sizeof(StackType_t);
    slot->task = xTaskCreateStatic(service_task, slot->desc.name,
                                   stack_words,
                                   &s_service_indices[idx], slot->desc.priority,
                                   slot->task_stack, &slot->task_tcb);
    if (slot->task == NULL) {
        slot->info.phase = SVC_PHASE_FAILED;
        slot->info.last_health = CIRVANE_HEALTH_FAILED;
    }
}

static void supervisor_task(void *arg)
{
    (void)arg;
    for (;;) {
        s_system_state = CIRVANE_SYS_NOMINAL;
        uint32_t now = cirvane_uptime_ms();
        for (uint8_t idx = 0; idx < s_service_count; ++idx) {
            service_slot_t *slot = &s_services[idx];
            bool failed = slot->reported_health == CIRVANE_HEALTH_FAILED;
            if (failed && slot->info.phase == SVC_PHASE_RUNNING) {
                slot->info.last_health = CIRVANE_HEALTH_FAILED;
                vTaskDelete(slot->task);
                slot->task = NULL;
                if (slot->desc.policy == CIRVANE_RESTART_AUTO &&
                    slot->info.restarts < CIRVANE_RESTART_LIMIT) {
                    uint32_t delay = CIRVANE_RESTART_BASE_MS << slot->info.restarts;
                    slot->restart_at_ms = now + delay;
                    slot->info.restarts++;
                    slot->info.phase = SVC_PHASE_BACKOFF;
                } else if (slot->desc.policy == CIRVANE_RESTART_ONETIME &&
                           slot->info.restarts == 0) {
                    slot->restart_at_ms = now + CIRVANE_RESTART_BASE_MS;
                    slot->info.restarts = 1;
                    slot->info.phase = SVC_PHASE_BACKOFF;
                } else {
                    slot->info.phase = SVC_PHASE_FAILED;
                }
            }
            if (slot->info.phase == SVC_PHASE_BACKOFF &&
                (int32_t)(now - slot->restart_at_ms) >= 0) {
                start_service(idx);
            }
            if (slot->desc.required &&
                (slot->info.phase != SVC_PHASE_RUNNING ||
                 slot->reported_health != CIRVANE_HEALTH_OK)) {
                s_system_state = CIRVANE_SYS_DEGRADED;
            }
        }

        if (s_sleep_after_ms != 0) {
            uint32_t duration = s_sleep_after_ms;
            s_sleep_after_ms = 0;
            esp_sleep_enable_timer_wakeup((uint64_t)duration * 1000ULL);
            if (s_budget == CIRVANE_BUDGET_SENTINEL) {
                ESP_LOGI(TAG, "entering sentinel deep sleep for %" PRIu32 " ms", duration);
                esp_deep_sleep_start();
            } else {
                ESP_LOGI(TAG, "entering watchdog light sleep for %" PRIu32 " ms", duration);
                esp_light_sleep_start();
                cirvane_power_set_budget(CIRVANE_BUDGET_ACTIVE);
            }
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(s_config.supervisor_ms));
    }
}

int cirvane_manager_register(const cirvane_service_t *service, uint8_t *out_idx)
{
    if (service == NULL || service->name == NULL || service->init == NULL ||
        service->period_ms == 0 || service->stack_size < 1024 ||
        service->stack_size > sizeof(s_services[0].task_stack) ||
        s_started || s_service_count >= CIRVANE_MAX_SERVICES) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t idx = s_service_count++;
    service_slot_t *slot = &s_services[idx];
    memset(slot, 0, sizeof(*slot));
    slot->desc = *service;
    slot->capabilities = service->capabilities;
    slot->reported_health = CIRVANE_HEALTH_OK;
    slot->registered = true;
    slot->heap_baseline = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    slot->info = (svc_info_t){
        .name = service->name,
        .desc = service->desc,
        .phase = SVC_PHASE_INIT,
        .last_health = CIRVANE_HEALTH_OK,
        .policy = service->policy,
    };
    slot->mailbox = xQueueCreateStatic(CIRVANE_MAILBOX_DEPTH,
                                       sizeof(cirvane_msg_t *),
                                       slot->mailbox_storage,
                                       &slot->mailbox_cb);
    if (out_idx != NULL) {
        *out_idx = idx;
    }
    return slot->mailbox != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void cirvane_manager_register_builtins(void)
{
    /* Built-ins are registered by app_main, which owns their board context. */
}

void cirvane_manager_start(void)
{
    s_started = true;
    s_boot.bus_ready_ms = cirvane_uptime_ms();
    for (uint8_t idx = 0; idx < s_service_count; ++idx) {
        start_service(idx);
    }
    s_boot.services_inited_ms = cirvane_uptime_ms();
    s_supervisor_task = xTaskCreateStatic(supervisor_task, "cirvane_mgr",
                                          CIRVANE_SUPERVISOR_STACK_BYTES /
                                              sizeof(StackType_t),
                                          NULL, 10,
                                          s_supervisor_stack, &s_supervisor_tcb);
    ESP_ERROR_CHECK(s_supervisor_task != NULL ? ESP_OK : ESP_ERR_NO_MEM);
    s_boot.tasks_spawned_ms = cirvane_uptime_ms();
}

uint8_t cirvane_manager_count(void)
{
    return s_service_count;
}

bool cirvane_manager_get_info(uint8_t idx, svc_info_t *out)
{
    if (idx >= s_service_count || out == NULL) {
        return false;
    }
    *out = s_services[idx].info;
    return true;
}

int cirvane_svc_control(uint8_t idx, const char *action)
{
    if (idx >= s_service_count || action == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    service_slot_t *slot = &s_services[idx];
    if (strcmp(action, "stop") == 0) {
        if (slot->task != NULL) {
            vTaskDelete(slot->task);
            slot->task = NULL;
        }
        slot->info.phase = SVC_PHASE_STOPPED;
        return ESP_OK;
    }
    if (strcmp(action, "start") == 0) {
        if (slot->info.phase != SVC_PHASE_STOPPED && slot->info.phase != SVC_PHASE_FAILED) {
            return ESP_ERR_INVALID_STATE;
        }
        start_service(idx);
        return ESP_OK;
    }
    if (strcmp(action, "restart") == 0) {
        if (slot->task != NULL) {
            vTaskDelete(slot->task);
            slot->task = NULL;
        }
        slot->info.restarts++;
        start_service(idx);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

void cirvane_supervisor_kick(void)
{
    if (s_supervisor_task != NULL) {
        xTaskNotifyGive(s_supervisor_task);
    }
}

cirvane_sys_state_t cirvane_system_state(void)
{
    return s_system_state;
}

const cirvane_boot_timing_t *cirvane_boot_timing(void)
{
    return &s_boot;
}

void cirvane_boot_mark_nvs_ready(void)
{
    if (s_boot.nvs_done_ms == 0) {
        s_boot.nvs_done_ms = cirvane_uptime_ms();
    }
}

void cirvane_res_window_begin(void)
{
    for (uint8_t idx = 0; idx < s_service_count; ++idx) {
        s_services[idx].runtime_baseline = 0;
        s_services[idx].radio_ms = 0;
    }
}

void cirvane_res_window_end(void)
{
    /* uxTaskGetSystemState snapshots are taken lazily by cirvane_res_get(). */
}

void cirvane_res_record_radio(uint8_t service_idx, uint32_t elapsed_ms)
{
    if (service_idx < s_service_count) {
        taskENTER_CRITICAL(&s_bus_lock);
        uint32_t current = s_services[service_idx].radio_ms;
        s_services[service_idx].radio_ms =
            UINT32_MAX - current < elapsed_ms ? UINT32_MAX : current + elapsed_ms;
        taskEXIT_CRITICAL(&s_bus_lock);
    }
}

void cirvane_res_get(uint8_t service_idx, cirvane_res_usage_t *out)
{
    if (out == NULL || service_idx >= s_service_count) {
        return;
    }
    service_slot_t *slot = &s_services[service_idx];
    memset(out, 0, sizeof(*out));
    uint32_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    out->heap_baseline = slot->heap_baseline;
    out->heap_delta = (int32_t)free_heap - (int32_t)slot->heap_baseline;
    taskENTER_CRITICAL(&s_bus_lock);
    out->radio_ms_window = slot->radio_ms;
    taskEXIT_CRITICAL(&s_bus_lock);
    if (slot->task != NULL) {
        out->stack_hwm = (uint16_t)(uxTaskGetStackHighWaterMark(slot->task) * sizeof(StackType_t));
        TaskStatus_t tasks[20];
        configRUN_TIME_COUNTER_TYPE total = 0;
        UBaseType_t count = uxTaskGetSystemState(tasks, 20, &total);
        for (UBaseType_t idx = 0; idx < count; ++idx) {
            if (tasks[idx].xHandle == slot->task && total != 0) {
                uint64_t pct = (uint64_t)tasks[idx].ulRunTimeCounter * 100ULL / total;
                out->cpu_percent = (uint8_t)(pct > 100 ? 100 : pct);
                break;
            }
        }
    }
}
