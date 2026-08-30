/* Cirvane - bounded supervised firmware for the Seeed Studio XIAO ESP32-C5.
 *
 * This product image is powered by ESP-IDF and FreeRTOS (ADR 0005). Cirvane
 * owns the service supervisor, bounded message bus, capabilities,
 * configuration journal, shell and OTA policy. A clean-sheet kernel remains
 * research and is not this firmware.
 *
 * Architecture:
 *   - Scheduler: ESP-IDF's FreeRTOS (single-core RISC-V @ 240 MHz).
 *   - Shell: esp_console REPL on the built-in USB-Serial/JTAG port. The
 *     operator gets an interactive prompt with tab completion and history.
 *   - Supervisor: a heartbeat task proves liveness every 10 s, and the
 *     task watchdog covers the idle task so a wedged system panics
 *     loudly instead of hanging silently.
 *
 * Commands:
 *   info          chip/firmware/heap identity + boot counter (NVS)
 *   ps            all tasks: stack high-water mark + CPU time %
 *   led on|off|blink    USER LED (GPIO 27) control
 *   scan          one-shot Wi-Fi scan of both bands, sorted by RSSI
 * plus vendored IDF built-ins: version, free, heap, tasks, restart, help.
 *
 * API usage follows examples/system/console/basic (REPL) and
 * examples/wifi/scan from the pinned ESP-IDF v6.0.2. In v6 the console
 * component lives at components/console; esp_console.h still provides
 * the classic esp_console_new_repl_usb_serial_jtag() API.
 */
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_flash.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "cmd_system.h"
#include "cirvane_os.h"

static const char *TAG = "cirvane";

#define LED_GPIO         GPIO_NUM_27
#define MAX_AP_RECORDS   20

/* ------------------------------- state -------------------------------- */

typedef enum {
    LED_MODE_OFF,
    LED_MODE_ON,
    LED_MODE_BLINK,
} led_mode_t;

/* Shared between the REPL task (writers) and supervisor (reader). A single
 * aligned 32-bit store is atomic on RISC-V here, so no lock is needed. */
static volatile led_mode_t s_led_mode = LED_MODE_BLINK;
static uint32_t s_boot_count;
static esp_console_repl_t *s_repl;
static uint8_t s_led_service = CIRVANE_SERVICE_INVALID;
static uint8_t s_wifi_service = CIRVANE_SERVICE_INVALID;
static cirvane_config_t s_os_config;
enum {
    WIFI_INIT_NONE,
    WIFI_INIT_NETIF,
    WIFI_INIT_EVENT_LOOP,
    WIFI_INIT_DRIVER,
    WIFI_INIT_READY,
};
static uint8_t s_wifi_init_stage;

static bool parse_u32_arg(const char *text, uint32_t minimum, uint32_t maximum,
                          uint32_t *out)
{
    if (text == NULL || out == NULL || text[0] < '0' || text[0] > '9') return false;
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value > UINT32_MAX ||
        value < minimum || value > maximum) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

static const char *phase_name(svc_phase_t phase)
{
    switch (phase) {
    case SVC_PHASE_INIT: return "init";
    case SVC_PHASE_RUNNING: return "running";
    case SVC_PHASE_STOPPED: return "stopped";
    case SVC_PHASE_BACKOFF: return "backoff";
    case SVC_PHASE_FAILED: return "failed";
    default: return "unknown";
    }
}

/* ------------------------------ commands ------------------------------ */

static int cmd_info(int argc, char **argv)
{
    const esp_app_desc_t *app = esp_app_get_description();
    esp_chip_info_t chip;
    uint32_t flash_size = 0;
    esp_chip_info(&chip);

    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("flash      unknown\n");
        flash_size = 0;
    }

    printf("project    %s\n", app->project_name);
    printf("version    %s\n", app->version);
    printf("idf        %s\n", app->idf_ver);
    printf("chip       ESP32-C5 rev %d.%d, %d core(s) @ 240 MHz\n",
           chip.revision / 100, chip.revision % 100, chip.cores);
    if (flash_size) {
        printf("flash      %lu MB\n", (unsigned long)(flash_size / (1024 * 1024)));
    }
    printf("heap free  %u B, largest block %u B\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    printf("uptime     %llu ms\n",
           (unsigned long long)(esp_timer_get_time() / 1000));
    printf("boots      %lu\n", (unsigned long)s_boot_count);
    return 0;
}

/* ps - richer than the vendored `tasks`: adds CPU-time share per task.
 * Static buffer: bounded to 16 tasks, which covers this image with room
 * to spare (IDF system tasks + ours ≈ 8). */
static int cmd_ps(int argc, char **argv)
{
    static TaskStatus_t st[16];
    configRUN_TIME_COUNTER_TYPE total = 0;
    UBaseType_t n = uxTaskGetSystemState(st, 16, &total);
    if (n == 0) {
        printf("no task stats available\n");
        return 1;
    }

    printf("%-16s %-4s %-7s %-6s %s\n", "task", "prio", "stack(min)", "cpu%", "state");
    for (UBaseType_t i = 0; i < n; i++) {
        unsigned pct = total ? (unsigned)((st[i].ulRunTimeCounter * 100ULL) / total) : 0;
        /* eCurrentState indexes the eTaskState enum; '?' guards OOB if the
         * enum grows beyond the letters we map. */
        const char states[] = "?RBSXD";
        unsigned si = (unsigned)st[i].eCurrentState;
        char state = states[si < sizeof(states) - 1 ? si : 0];
        printf("%-16s %-4u %-7u %-6u %c\n",
               st[i].pcTaskName, (unsigned)st[i].uxCurrentPriority,
               (unsigned)st[i].usStackHighWaterMark, pct, state);
    }
    return 0;
}

static int cmd_led(int argc, char **argv)
{
    uint8_t mode;
    if (argc == 2 && strcmp(argv[1], "on") == 0) {
        mode = LED_MODE_ON;
    } else if (argc == 2 && strcmp(argv[1], "off") == 0) {
        mode = LED_MODE_OFF;
    } else if (argc == 2 && strcmp(argv[1], "blink") == 0) {
        mode = LED_MODE_BLINK;
    } else {
        printf("usage: led on|off|blink\n");
        return 1;
    }
    if (!cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_LED)) {
        printf("permission denied\n");
        return 1;
    }
    cirvane_msg_t *msg = cirvane_bus_alloc();
    if (msg == NULL) {
        printf("message bus busy\n");
        return 1;
    }
    msg->type = CIRVANE_MSG_LED_MODE;
    msg->sender = CIRVANE_SENDER_SHELL;
    msg->payload_len = 1;
    msg->payload[0] = mode;
    cirvane_bus_publish(msg);
    return 0;
}

/* Reuses the proven pattern from ../wifi_scan: get_ap_num() must run
 * before get_ap_records(), which consumes the internal list. Bounded to
 * MAX_AP_RECORDS records; the count line reports the true total. */
static int cmd_scan(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_WIFI)) {
        printf("permission denied\n");
        return 1;
    }
    cirvane_msg_t *msg = cirvane_bus_alloc();
    if (msg == NULL) {
        printf("message bus busy\n");
        return 1;
    }
    msg->type = CIRVANE_MSG_SCAN_REQUEST;
    msg->sender = CIRVANE_SENDER_SHELL;
    cirvane_bus_publish(msg);
    printf("scan queued; results follow asynchronously\n");
    return 0;
}

static void run_wifi_scan(void)
{
    uint32_t scan_started_ms = cirvane_uptime_ms();
    uint16_t max_records = MAX_AP_RECORDS;
    wifi_ap_record_t ap_info[MAX_AP_RECORDS];
    uint16_t ap_count = 0;

    if (s_wifi_init_stage != WIFI_INIT_READY) {
        esp_err_t err = ESP_OK;
        if (s_wifi_init_stage == WIFI_INIT_NONE) {
            err = esp_netif_init();
            if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
                s_wifi_init_stage = WIFI_INIT_NETIF;
            }
        }
        if (s_wifi_init_stage == WIFI_INIT_NETIF) {
            err = esp_event_loop_create_default();
            if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
                s_wifi_init_stage = WIFI_INIT_EVENT_LOOP;
            }
        }
        if (s_wifi_init_stage == WIFI_INIT_EVENT_LOOP) {
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            err = esp_wifi_init(&cfg);
            if (err == ESP_OK) s_wifi_init_stage = WIFI_INIT_DRIVER;
        }
        if (s_wifi_init_stage == WIFI_INIT_DRIVER) err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (s_wifi_init_stage == WIFI_INIT_DRIVER && err == ESP_OK) err = esp_wifi_start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi lazy init failed: %s", esp_err_to_name(err));
            cirvane_service_report_health(s_wifi_service, CIRVANE_HEALTH_DEGRADED);
            return;
        }
        s_wifi_init_stage = WIFI_INIT_READY;
    }

    memset(ap_info, 0, sizeof(ap_info));
    printf("scanning both bands...\n");
    esp_err_t err = esp_wifi_scan_start(NULL, true); /* blocking, all channels */
    if (err != ESP_OK) {
        cirvane_res_record_radio(s_wifi_service,
                                 cirvane_uptime_ms() - scan_started_ms);
        ESP_LOGE(TAG, "scan failed: %s", esp_err_to_name(err));
        cirvane_service_report_health(s_wifi_service, CIRVANE_HEALTH_DEGRADED);
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_records, ap_info));
    cirvane_res_record_radio(s_wifi_service,
                             cirvane_uptime_ms() - scan_started_ms);

    printf("%u APs visible (%u shown)\n", ap_count, max_records);
    for (int i = 0; i < max_records; i++) {
        const wifi_ap_record_t *ap = &ap_info[i];
        const char *band = (ap->primary > 14) ? "5GHz" : "2.4GHz";
        printf("%2d. %-32s %-5s ch=%3d rssi=%4d\n",
               i + 1, (const char *)ap->ssid, band, ap->primary, ap->rssi);
    }
    cirvane_service_report_health(s_wifi_service, CIRVANE_HEALTH_OK);
}

static int cmd_svc(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("system=%s services=%u\n",
           cirvane_system_state() == CIRVANE_SYS_NOMINAL ? "nominal" : "degraded",
           cirvane_manager_count());
    printf("%-3s %-14s %-9s %-7s %-8s %s\n",
           "id", "name", "phase", "health", "restarts", "last/max ms");
    for (uint8_t idx = 0; idx < cirvane_manager_count(); ++idx) {
        svc_info_t info;
        if (cirvane_manager_get_info(idx, &info)) {
            printf("%-3u %-14s %-9s %-7u %-8u %" PRIu32 "/%" PRIu32 "\n",
                   idx, info.name, phase_name(info.phase), info.last_health,
                   info.restarts, info.last_run_ms, info.max_run_ms);
        }
    }
    return 0;
}

static int cmd_svcctl(int argc, char **argv)
{
    if (argc != 3 || !cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_SVCCTL)) {
        printf("usage: svcctl <id> <start|stop|restart>\n");
        return 1;
    }
    char *end = NULL;
    unsigned long idx = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || idx >= cirvane_manager_count()) {
        printf("invalid service id\n");
        return 1;
    }
    esp_err_t err = cirvane_svc_control((uint8_t)idx, argv[2]);
    if (err != ESP_OK) {
        printf("svcctl failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    return 0;
}

#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
static int cmd_svcfail(int argc, char **argv)
{
    if (argc != 2 ||
        !cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_SVCCTL)) {
        printf("usage: svcfail <id>\n");
        return 1;
    }
    char *end = NULL;
    unsigned long idx = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || idx >= cirvane_manager_count()) {
        printf("invalid service id\n");
        return 1;
    }
    cirvane_service_report_health((uint8_t)idx, CIRVANE_HEALTH_FAILED);
    cirvane_supervisor_kick();
    printf("failure injected into service %lu; inspect with svc\n", idx);
    return 0;
}
#endif

static int cmd_bus(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    cirvane_bus_stats_t stats;
    cirvane_bus_get_stats(&stats);
    printf("pool free=%u/%u peak=%u drops=%u alloc_fails=%u\n",
           stats.free_slots, CIRVANE_MSG_POOL_SLOTS, stats.peak_used,
           stats.total_drops, stats.alloc_fails);
    return 0;
}

static int cmd_boot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const cirvane_boot_timing_t *timing = cirvane_boot_timing();
    printf("nvs=%" PRIu32 " bus=%" PRIu32 " services=%" PRIu32
           " tasks=%" PRIu32 " first_tick=%" PRIu32 " ms\n",
           timing->nvs_done_ms, timing->bus_ready_ms, timing->services_inited_ms,
           timing->tasks_spawned_ms, timing->first_tick_ms);
    return 0;
}

static int cmd_res(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("%-3s %-14s %-10s %-10s %-5s %s\n", "id", "name", "stack_min", "heap_delta", "cpu%", "radio_ms");
    for (uint8_t idx = 0; idx < cirvane_manager_count(); ++idx) {
        svc_info_t info;
        cirvane_res_usage_t usage;
        if (cirvane_manager_get_info(idx, &info)) {
            cirvane_res_get(idx, &usage);
            printf("%-3u %-14s %-10u %-10" PRId32 " %-5u %" PRIu32 "\n",
                   idx, info.name, usage.stack_hwm, usage.heap_delta,
                   usage.cpu_percent, usage.radio_ms_window);
        }
    }
    return 0;
}

static int cmd_power(int argc, char **argv)
{
    if (argc == 1) {
        printf("budget=%s\n", cirvane_budget_name(cirvane_power_current_budget()));
        return 0;
    }
    if (argc < 2 || argc > 3) {
        printf("usage: power [active|watchdog|sentinel [wake_ms]]\n");
        return 1;
    }
    if (!cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_POWER)) {
        printf("permission denied\n");
        return 1;
    }
    cirvane_budget_t budget;
    if (strcmp(argv[1], "active") == 0) budget = CIRVANE_BUDGET_ACTIVE;
    else if (strcmp(argv[1], "watchdog") == 0) budget = CIRVANE_BUDGET_WATCHDOG;
    else if (strcmp(argv[1], "sentinel") == 0) budget = CIRVANE_BUDGET_SENTINEL;
    else {
        printf("usage: power [active|watchdog|sentinel [wake_ms]]\n");
        return 1;
    }
    esp_err_t err;
    if (argc == 3) {
        uint32_t wake_ms;
        if (!parse_u32_arg(argv[2], 100, CIRVANE_MAX_SLEEP_MS, &wake_ms)) {
            printf("invalid wake_ms\n");
            return 1;
        }
        err = cirvane_power_schedule(budget, wake_ms);
    } else {
        err = cirvane_power_set_budget(budget);
    }
    if (err != ESP_OK) {
        printf("power failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    return 0;
}

static int cmd_ota_confirm(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_OTA)) {
        printf("permission denied\n");
        return 1;
    }
    esp_err_t err = cirvane_ota_confirm();
    printf("ota confirm: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_ota_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t err = running == NULL
                        ? ESP_ERR_NOT_FOUND
                        : esp_ota_get_state_partition(running, &state);
    printf("ota running=%s boot=%s state=%s query=%s\n",
           running == NULL ? "none" : running->label,
           boot == NULL ? "none" : boot->label,
           cirvane_ota_state_name(state), esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
static int cmd_ota_stage_self(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_OTA)) {
        printf("permission denied\n");
        return 1;
    }
    esp_err_t err = cirvane_ota_stage_self();
    printf("ota stage-self: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_ota_reject_corrupt(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = cirvane_ota_reject_corrupt_test();
    printf("ota corrupt rejection: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}

static int cmd_config_corrupt_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    esp_err_t err = cirvane_config_corruption_test();
    printf("config corrupt fallback: %s\n", esp_err_to_name(err));
    return err == ESP_OK ? 0 : 1;
}
#endif

static int cmd_config(int argc, char **argv)
{
    if (argc == 1) {
        cirvane_config_get(&s_os_config);
        printf("heartbeat_ms=%" PRIu32 " supervisor_ms=%" PRIu32 " led=%u\n",
               s_os_config.heartbeat_ms, s_os_config.supervisor_ms,
               s_os_config.default_led_mode);
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "heartbeat") != 0 ||
        !cirvane_caps_check(CIRVANE_SERVICE_INVALID, CIRVANE_CAP_CONFIG)) {
        printf("usage: config [heartbeat <1000..3600000>]\n");
        return 1;
    }
    cirvane_config_get(&s_os_config);
    if (!parse_u32_arg(argv[2], 1000, 3600000, &s_os_config.heartbeat_ms)) {
        printf("invalid heartbeat_ms\n");
        return 1;
    }
    esp_err_t err = cirvane_config_commit(&s_os_config);
    if (err != ESP_OK) {
        printf("config commit failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("configuration committed transactionally\n");
    return 0;
}

static int cmd_selftest(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    cirvane_bus_stats_t before;
    cirvane_bus_get_stats(&before);
    cirvane_msg_t *msg = cirvane_bus_alloc();
    bool bus_ok = msg != NULL;
    cirvane_bus_free(msg);
    cirvane_config_t cfg;
    cirvane_config_get(&cfg);
    bool config_ok = cfg.schema_version == 1 && cfg.heartbeat_ms >= 1000;
    bool service_ok = cirvane_manager_count() >= 2;
    printf("selftest bus=%s config=%s services=%s result=%s\n",
           bus_ok ? "ok" : "fail", config_ok ? "ok" : "fail",
           service_ok ? "ok" : "fail",
           bus_ok && config_ok && service_ok ? "PASS" : "FAIL");
    return bus_ok && config_ok && service_ok ? 0 : 1;
}

static void led_service_init(void *ctx)
{
    (void)ctx;
    s_led_mode = (led_mode_t)s_os_config.default_led_mode;
}

static void led_service_tick(void *ctx)
{
    (void)ctx;
    for (;;) {
        cirvane_msg_t *msg = cirvane_bus_recv(s_led_service, 0);
        if (msg == NULL) break;
        if (msg->type == CIRVANE_MSG_LED_MODE && msg->payload_len == 1 &&
            msg->payload[0] <= LED_MODE_BLINK) {
            s_led_mode = (led_mode_t)msg->payload[0];
        }
        cirvane_bus_free(msg);
    }
    if (cirvane_power_current_budget() != CIRVANE_BUDGET_ACTIVE) {
        gpio_set_level(LED_GPIO, 0);
        return;
    }
    bool blink_state = (xTaskGetTickCount() / pdMS_TO_TICKS(500)) % 2;
    gpio_set_level(LED_GPIO, s_led_mode == LED_MODE_ON ||
                             (s_led_mode == LED_MODE_BLINK && blink_state));
    static uint32_t last_heartbeat;
    uint32_t now = cirvane_uptime_ms();
    cirvane_config_get(&s_os_config);
    if ((uint32_t)(now - last_heartbeat) >= s_os_config.heartbeat_ms) {
        last_heartbeat = now;
        /* Keep routine liveness telemetry below the default console level so
         * asynchronous output cannot corrupt an operator's command line. */
        ESP_LOGD(TAG, "heartbeat up=%" PRIu32 "s heap=%u mode=%s state=%s",
                 now / 1000, (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 s_led_mode == LED_MODE_BLINK ? "blink" :
                 s_led_mode == LED_MODE_ON ? "on" : "off",
                 cirvane_system_state() == CIRVANE_SYS_NOMINAL ? "nominal" : "degraded");
    }
}

static void wifi_service_init(void *ctx)
{
    (void)ctx;
}

static void wifi_service_tick(void *ctx)
{
    (void)ctx;
    if (cirvane_power_current_budget() != CIRVANE_BUDGET_ACTIVE) return;
    cirvane_msg_t *msg = cirvane_bus_recv(s_wifi_service, 0);
    if (msg != NULL) {
        if (msg->type == CIRVANE_MSG_SCAN_REQUEST) run_wifi_scan();
        cirvane_bus_free(msg);
    }
}

#if CONFIG_CIRVANE_MATCHED_EVAL
#define CIRVANE_MATCHED_MAGIC 0xC1455E01u
#define CIRVANE_MATCHED_N 33u

typedef struct {
    uint32_t magic;
    uint32_t count;
    uint32_t done;
    uint32_t ms[CIRVANE_MATCHED_N];
} cirvane_matched_rec_t;

__NOINIT_ATTR static volatile cirvane_matched_rec_t s_cirvane_matched;

static uint32_t hil_service_phase(uint8_t idx)
{
    svc_info_t info;
    if (!cirvane_manager_get_info(idx, &info)) {
        return 0xffu;
    }
    return (uint32_t)info.phase;
}

static void hil_matched_class1(void)
{
    const uint8_t svc = 0;
    uint32_t spins = 0;
    uint32_t batch = 0;

    if (s_cirvane_matched.magic != CIRVANE_MATCHED_MAGIC) {
        memset((void *)&s_cirvane_matched, 0, sizeof(s_cirvane_matched));
        s_cirvane_matched.magic = CIRVANE_MATCHED_MAGIC;
    }
    if (s_cirvane_matched.done || s_cirvane_matched.count >= CIRVANE_MATCHED_N) {
        s_cirvane_matched.done = 1;
        return;
    }
    while (hil_service_phase(svc) != SVC_PHASE_RUNNING && spins < 500u) {
        vTaskDelay(pdMS_TO_TICKS(20));
        spins++;
    }
    if (hil_service_phase(svc) != SVC_PHASE_RUNNING) {
        return;
    }
    while (s_cirvane_matched.count < CIRVANE_MATCHED_N && batch < 3u) {
        int64_t t0 = esp_timer_get_time();
        int64_t deadline = t0 + 15000000;
        bool left = false;
        bool back = false;
        cirvane_service_report_health(svc, CIRVANE_HEALTH_FAILED);
        cirvane_supervisor_kick();
        while (esp_timer_get_time() < deadline) {
            uint32_t phase = hil_service_phase(svc);
            if (phase == SVC_PHASE_BACKOFF || phase == SVC_PHASE_FAILED) {
                left = true;
            }
            if (left && phase == SVC_PHASE_RUNNING) {
                back = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (!back) {
            return;
        }
        s_cirvane_matched.ms[s_cirvane_matched.count] =
            (uint32_t)((esp_timer_get_time() - t0) / 1000);
        s_cirvane_matched.count++;
        batch++;
    }
    if (s_cirvane_matched.count >= CIRVANE_MATCHED_N) {
        s_cirvane_matched.done = 1;
        return;
    }
    esp_restart();
}
#endif

/* -------------------------------- shell ------------------------------- */

static void shell_start(void)
{
#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    (void)cmd_info;
    (void)cmd_ps;
    (void)cmd_led;
    (void)cmd_scan;
    (void)cmd_svc;
    (void)cmd_svcctl;
    (void)cmd_bus;
    (void)cmd_boot;
    (void)cmd_res;
    (void)cmd_power;
    (void)cmd_ota_confirm;
    (void)cmd_ota_status;
    (void)cmd_config;
    (void)cmd_selftest;
#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
    (void)cmd_svcfail;
    (void)cmd_ota_stage_self;
    (void)cmd_ota_reject_corrupt;
    (void)cmd_config_corrupt_test;
#endif
    (void)s_repl;
    return;
#else
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "cirvane> ";
    esp_console_dev_usb_serial_jtag_config_t dev_cfg =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&dev_cfg, &repl_cfg, &s_repl));
    ESP_ERROR_CHECK(esp_console_register_help_command());
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "info", .help = "Chip, firmware, heap, uptime, boots",
        .func = &cmd_info }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ps", .help = "Task table: stack high-water + CPU time",
        .func = &cmd_ps }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "led", .hint = "<on|off|blink>",
        .help = "Control the USER LED (GPIO 27)", .func = &cmd_led }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "scan", .help = "Wi-Fi scan both bands, sorted by RSSI",
        .func = &cmd_scan }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "svc", .help = "Cirvane supervised service table", .func = &cmd_svc }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "svcctl", .hint = "<id> <start|stop|restart>",
        .help = "Control a supervised service", .func = &cmd_svcctl }));
#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "svcfail", .hint = "<id>",
        .help = "Inject a service failure to test supervision",
        .func = &cmd_svcfail }));
#endif
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "bus", .help = "Bounded message-pool statistics", .func = &cmd_bus }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "boot", .help = "Deterministic startup timing", .func = &cmd_boot }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "res", .help = "Per-service stack, heap and CPU accounting",
        .func = &cmd_res }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "power", .hint = "[active|watchdog|sentinel [wake_ms]]",
        .help = "Inspect or set a power budget", .func = &cmd_power }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota-confirm", .help = "Confirm a pending rollback image",
        .func = &cmd_ota_confirm }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota-status", .help = "Show running and selected OTA state",
        .func = &cmd_ota_status }));
#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota-stage-self",
        .help = "HIL only: copy the signed running image into the inactive OTA slot",
        .func = &cmd_ota_stage_self }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "ota-reject-corrupt",
        .help = "HIL: prove a corrupted signed image is not selected",
        .func = &cmd_ota_reject_corrupt }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "config-corrupt-test",
        .help = "HIL: prove CRC fallback and repair of one config slot",
        .func = &cmd_config_corrupt_test }));
#endif
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "config", .hint = "[heartbeat <ms>]",
        .help = "Inspect or transactionally update configuration", .func = &cmd_config }));
    ESP_ERROR_CHECK(esp_console_cmd_register(&(esp_console_cmd_t){
        .command = "selftest", .help = "Run bounded runtime smoke tests",
        .func = &cmd_selftest }));
    register_system_common();

    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
#endif
}

void app_main(void)
{
    /* NVS first - Wi-Fi needs it; tolerate a dirty partition once. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(cirvane_config_load(&s_os_config));
    cirvane_boot_mark_nvs_ready();

    ESP_ERROR_CHECK(gpio_reset_pin(LED_GPIO));
    ESP_ERROR_CHECK(gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT));

    /* Boot counter survives reboots via NVS. */
    nvs_handle_t nvs;
    if (nvs_open("cirvane", NVS_READWRITE, &nvs) == ESP_OK) {
        uint32_t boots = 0;
        nvs_get_u32(nvs, "boots", &boots);
        s_boot_count = boots + 1;
        nvs_set_u32(nvs, "boots", s_boot_count);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    cirvane_bus_init();
    ESP_ERROR_CHECK(cirvane_manager_register(&(cirvane_service_t){
        .name = "led-heartbeat", .desc = "LED control and liveness heartbeat",
        .init = led_service_init, .tick = led_service_tick, .period_ms = 100,
        .stack_size = 4096, .priority = 5, .policy = CIRVANE_RESTART_AUTO,
        .capabilities = CIRVANE_CAP_LED, .required = true,
    }, &s_led_service));
    ESP_ERROR_CHECK(cirvane_manager_register(&(cirvane_service_t){
        .name = "wifi-scan", .desc = "Asynchronous dual-band Wi-Fi scans",
        .init = wifi_service_init, .tick = wifi_service_tick, .period_ms = 50,
        .stack_size = 4096, .priority = 4, .policy = CIRVANE_RESTART_AUTO,
        .capabilities = CIRVANE_CAP_WIFI, .required = false,
    }, &s_wifi_service));
    ESP_ERROR_CHECK(cirvane_bus_subscribe(s_led_service, CIRVANE_MSG_LED_MODE));
    ESP_ERROR_CHECK(cirvane_bus_subscribe(s_wifi_service, CIRVANE_MSG_SCAN_REQUEST));
    cirvane_manager_start();
#if CONFIG_CIRVANE_MATCHED_EVAL
    hil_matched_class1();
#endif
    ESP_LOGI(TAG, "cirvane online; powered by ESP-IDF/FreeRTOS; type 'help' at the prompt");
    shell_start(); /* spawns its own REPL task */
}
