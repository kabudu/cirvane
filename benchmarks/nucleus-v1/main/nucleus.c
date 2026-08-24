/* Nucleus - a small operating system for the Seeed Studio XIAO ESP32-C5.
 *
 * Architecture (deliberately simple, per Lazarus-mode rigor):
 *   - Kernel: ESP-IDF's FreeRTOS (single-core RISC-V @ 240 MHz). Writing
 *     our own scheduler would add risk with no requirement behind it.
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_flash.h"
#include "esp_console.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "cmd_system.h"

static const char *TAG = "nucleus";

#define LED_GPIO         GPIO_NUM_27
#define HEARTBEAT_MS     10000
#define MAX_AP_RECORDS   20

/* Vendored from examples/system/console/advanced/components/cmd_system:
 * registers version, free, heap, tasks, restart, log level. */
extern void register_log_level(void);

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
    if (argc == 2 && strcmp(argv[1], "on") == 0) {
        s_led_mode = LED_MODE_ON;
        gpio_set_level(LED_GPIO, 1);
    } else if (argc == 2 && strcmp(argv[1], "off") == 0) {
        s_led_mode = LED_MODE_OFF;
        gpio_set_level(LED_GPIO, 0);
    } else if (argc == 2 && strcmp(argv[1], "blink") == 0) {
        s_led_mode = LED_MODE_BLINK;
    } else {
        printf("usage: led on|off|blink\n");
        return 1;
    }
    return 0;
}

/* Reuses the proven pattern from ../wifi_scan: get_ap_num() must run
 * before get_ap_records(), which consumes the internal list. Bounded to
 * MAX_AP_RECORDS records; the count line reports the true total. */
static int cmd_scan(int argc, char **argv)
{
    uint16_t max_records = MAX_AP_RECORDS;
    wifi_ap_record_t ap_info[MAX_AP_RECORDS];
    uint16_t ap_count = 0;

    memset(ap_info, 0, sizeof(ap_info));
    printf("scanning both bands...\n");
    esp_err_t err = esp_wifi_scan_start(NULL, true); /* blocking, all channels */
    if (err != ESP_OK) {
        printf("scan failed: %s\n", esp_err_to_name(err));
        return 1;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_records, ap_info));

    printf("%u APs visible (%u shown)\n", ap_count, max_records);
    for (int i = 0; i < max_records; i++) {
        const wifi_ap_record_t *ap = &ap_info[i];
        const char *band = (ap->primary > 14) ? "5GHz" : "2.4GHz";
        printf("%2d. %-32s %-5s ch=%3d rssi=%4d\n",
               i + 1, (const char *)ap->ssid, band, ap->primary, ap->rssi);
    }
    return 0;
}

/* -------------------------------- shell ------------------------------- */

static void shell_start(void)
{
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "nucleus> ";
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
    register_system_common();

    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
}

/* ----------------------------- supervisor ----------------------------- */

static void supervisor_task(void *arg)
{
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(HEARTBEAT_MS));
        bool blink_state = (xTaskGetTickCount() / pdMS_TO_TICKS(500)) % 2;
        switch (s_led_mode) {
        case LED_MODE_BLINK: gpio_set_level(LED_GPIO, blink_state); break;
        case LED_MODE_ON:    gpio_set_level(LED_GPIO, 1);           break;
        case LED_MODE_OFF:   gpio_set_level(LED_GPIO, 0);           break;
        default:             break;
        }
        ESP_LOGI(TAG, "heartbeat up=%llus heap=%u mode=%s",
                 (unsigned long long)(esp_timer_get_time() / 1000000),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 s_led_mode == LED_MODE_BLINK ? "blink" :
                 s_led_mode == LED_MODE_ON    ? "on"    : "off");
    }
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

    ESP_ERROR_CHECK(gpio_reset_pin(LED_GPIO));
    ESP_ERROR_CHECK(gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT));

    /* Boot counter survives reboots via NVS. */
    nvs_handle_t nvs;
    if (nvs_open("nucleus", NVS_READWRITE, &nvs) == ESP_OK) {
        uint32_t boots = 0;
        nvs_get_u32(nvs, "boots", &boots);
        s_boot_count = boots + 1;
        nvs_set_u32(nvs, "boots", s_boot_count);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    /* Wi-Fi driver up so `scan` works; STA-only, connects to nothing. */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "nucleus online - type 'help' at the prompt");

    xTaskCreate(supervisor_task, "supervisor", 3072, NULL, 5, NULL);
    shell_start(); /* spawns its own REPL task */
}
