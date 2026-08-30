/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Cirvane OS - public API: supervised services, typed message bus,
 *        capability permissions, resource accounting, power and OTA control.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================ services =============================== */

/** Maximum number of services in the static registry. */
#define CIRVANE_MAX_SERVICES 8

/**
 * Service health as reported by the service itself each supervision tick.
 */
typedef enum {
    CIRVANE_HEALTH_OK = 0,    /**< service is alive and making progress */
    CIRVANE_HEALTH_DEGRADED,  /**< alive but operating below full function */
    CIRVANE_HEALTH_FAILED,    /**< service requests a supervisor restart */
} cirvane_health_t;

/**
 * What the supervisor does when a service reports FAILED (or its task dies).
 */
typedef enum {
    CIRVANE_RESTART_AUTO = 0, /**< restart with backoff, give up to degraded mode */
    CIRVANE_RESTART_ONETIME,  /**< restart once per boot, then leave failed */
    CIRVANE_RESTART_NEVER,    /**< never restart; a dead task stays dead */
} cirvane_restart_policy_t;

/**
 * Static description of one service. Instances live in a fixed-size array;
 * no service registry allocation happens after boot.
 */
typedef struct cirvane_service {
    const char *name;                 /**< stable identifier for `svc` shell output */
    const char *desc;                 /**< one-line human-readable purpose */
    void (*init)(void *ctx);          /**< required: bring-up; may log, must not block long */
    void (*tick)(void *ctx);          /**< optional: periodic work; NULL = taskless service */
    uint32_t period_ms;               /**< tick period; ignored when tick == NULL */
    uint16_t stack_size;              /**< FreeRTOS stack bytes for the tick task */
    UBaseType_t priority;             /**< FreeRTOS priority for the tick task */
    cirvane_restart_policy_t policy;  /**< supervisor behaviour on failure */
    uint32_t capabilities;            /**< cirvane_cap_t mask held by service */
    bool required;                    /**< failure contributes to degraded mode */
    void *ctx;                        /**< opaque context passed back to init/tick */
} cirvane_service_t;

/* ============================== message bus ============================== */

/** Maximum number of distinct message types across the whole system. */
#define CIRVANE_MSG_TYPE_MAX 16

/** Total number of message slots shared by all mailboxes. */
#define CIRVANE_MSG_POOL_SLOTS 32

/** Maximum payload bytes carried inline by one message. */
#define CIRVANE_MSG_PAYLOAD_MAX 24

/**
 * Message type identifiers. One slot of CIRVANE_MSG_TYPE_MAX per type,
 * allocated at build time by listing them here.
 */
typedef enum {
    CIRVANE_MSG_NONE = 0,
    CIRVANE_MSG_LED_MODE,      /**< payload[0]: 0=off 1=on 2=blink */
    CIRVANE_MSG_SCAN_REQUEST,  /**< ask wifi service to run one scan */
    CIRVANE_MSG_SCAN_DONE,     /**< broadcast after a scan finishes; no payload */
    CIRVANE_MSG_POWER_BUDGET,  /**< payload[0]: requested power budget id */
    CIRVANE_MSG_SLEEP_DUE,     /**< power service announces imminent sleep */
    /* new types go above this line */
    CIRVANE_MSG_TYPE_COUNT,
} cirvane_msg_type_t;

/** One pooled message. Copies of small payloads only; no pointers outlive send. */
typedef struct {
    uint8_t type;                          /**< cirvane_msg_type_t of this message */
    uint8_t sender;                        /**< service index or CIRVANE_SENDER_SHELL */
    uint8_t payload_len;                   /**< bytes used in payload[] */
    uint8_t _pad;
    uint8_t payload[CIRVANE_MSG_PAYLOAD_MAX];
} cirvane_msg_t;

/** Shell-originated sends use this sender id (outside the service index space). */
#define CIRVANE_SENDER_SHELL 0xFFu

/**
 * Create the static message pool and per-service mailboxes. Call once from
 * the main thread before services are registered/started.
 */
void cirvane_bus_init(void);

/**
 * Subscribe a service to a message type. Fails if the type table is full or
 * the service index is out of range.
 *
 * @return ESP_OK on success.
 */
int cirvane_bus_subscribe(uint8_t service_idx, uint8_t type);

/**
 * Allocate a slot from the pool and fill it in. Returns NULL when the pool is
 * exhausted - callers must treat that as backpressure, not an error to retry
 * in a loop.
 */
cirvane_msg_t *cirvane_bus_alloc(void);

/**
 * Deliver an allocated message to every subscriber of its type. Frees the
 * slot even if delivery fails. Delivery to one subscriber cannot starve the
 * others because each mailbox drain runs in that subscriber's own context.
 */
void cirvane_bus_publish(cirvane_msg_t *msg);

/**
 * Fetch the next message addressed to this service, waiting at most
 * wait_ms. Returns NULL when no message arrived in time.
 */
cirvane_msg_t *cirvane_bus_recv(uint8_t service_idx, uint32_t wait_ms);

/** Return a received message's slot to the pool. */
void cirvane_bus_free(cirvane_msg_t *msg);

/** Pool statistics for the shell: peak usage, drops, current free count. */
typedef struct {
    uint16_t peak_used;
    uint16_t total_drops;   /**< publishes dropped because a mailbox was full */
    uint16_t alloc_fails;   /**< alloc() calls that found the pool empty */
    uint16_t free_slots;    /**< snapshot taken under the pool lock */
} cirvane_bus_stats_t;

void cirvane_bus_get_stats(cirvane_bus_stats_t *out);

/* ============================= capabilities ============================== */

/**
 * Capabilities gate state-changing operations. The shell grants itself only
 * the operator capability set; services hold what their descriptor declares.
 */
typedef enum {
    CIRVANE_CAP_LED     = 1u << 0, /**< drive GPIO / LED modes            */
    CIRVANE_CAP_WIFI    = 1u << 1, /**< start scans, change radio state   */
    CIRVANE_CAP_CONFIG  = 1u << 2, /**< write transactional config values */
    CIRVANE_CAP_POWER   = 1u << 3, /**< arm deep-sleep timers             */
    CIRVANE_CAP_OTA     = 1u << 4, /**< begin OTA, switch boot partition  */
    CIRVANE_CAP_SVCCTL  = 1u << 5, /**< stop / start / restart services   */
} cirvane_cap_t;

#define CIRVANE_CAP_OPERATOR_ALL \
    (CIRVANE_CAP_LED | CIRVANE_CAP_WIFI | CIRVANE_CAP_CONFIG | \
     CIRVANE_CAP_POWER | CIRVANE_CAP_OTA | CIRVANE_CAP_SVCCTL)

/**
 * Check a capability against a service's declared set. Shell commands pass
 * CIRVANE_SERVICE_INVALID to test the operator set instead.
 *
 * @return true when the capability is held.
 */
bool cirvane_caps_check(uint8_t service_idx, uint32_t needed_cap);

/** Read back the capability mask declared by a service descriptor. */
uint32_t cirvane_service_caps(uint8_t service_idx);

/** Report service health to the supervisor from a service tick. */
void cirvane_service_report_health(uint8_t service_idx, cirvane_health_t health);

/** Add measured radio-active time to a service's resource accounting. */
void cirvane_res_record_radio(uint8_t service_idx, uint32_t elapsed_ms);

/* =============================== resources =============================== */

/** Resource accounting snapshot for one service (or the whole system). */
typedef struct {
    uint32_t heap_baseline;       /**< free heap at registration time */
    int32_t  heap_delta;          /**< free heap now minus baseline */
    uint16_t stack_hwm;           /**< task high-water mark in bytes (0 = none) */
    uint8_t  cpu_percent;         /**< runtime share over the accounting window */
    uint32_t radio_ms_window;     /**< radio-active ms accumulated since boot */
} cirvane_res_usage_t;

/** Reset per-service radio counters; CPU share remains a scheduler snapshot. */
void cirvane_res_window_begin(void);

/** Reserved boundary for future sampled windows; currently a no-op. */
void cirvane_res_window_end(void);

/** Fill a usage record for one service index. */
void cirvane_res_get(uint8_t service_idx, cirvane_res_usage_t *out);

/* ============================ power management =========================== */

/**
 * Named power budgets. A budget caps which subsystems may be powered and
 * what sleep state the system may enter between ticks.
 */
typedef enum {
    CIRVANE_BUDGET_ACTIVE = 0, /**< everything on; no sleep scheduled */
    CIRVANE_BUDGET_WATCHDOG,   /**< radio off, LED service paused, light sleep allowed */
    CIRVANE_BUDGET_SENTINEL,   /**< radio + LED off, deep sleep until woken */
} cirvane_budget_t;

/** Maximum accepted timed sleep request; bounds accidental denial of service. */
#define CIRVANE_MAX_SLEEP_MS 86400000u

/** Human-readable budget name for the shell. */
const char *cirvane_budget_name(cirvane_budget_t b);

/** Currently armed budget (CIRVANE_BUDGET_ACTIVE until changed). */
cirvane_budget_t cirvane_power_current_budget(void);

/**
 * Arm a budget immediately. The power service enforces it on its next tick:
 * pausing/resuming services, gating the radio, and scheduling sleep.
 *
 * @return ESP_OK, or ESP_ERR_NOT_SUPPORTED for combinations the board
 *         cannot honour (surfaced honestly rather than silently downgraded).
 */
int cirvane_power_set_budget(cirvane_budget_t b);

/** Schedule a timed sleep for a non-active budget. */
int cirvane_power_schedule(cirvane_budget_t b, uint32_t wake_after_ms);

/* ================================== OTA ================================== */

/**
 * OTA outcome codes reported by cirvane_ota_begin()/switch flows. Kept as
 * ints so the shell can print esp_err_to_name() directly.
 */
#define CIRVANE_OTA_OK 0

/**
 * Validate the currently-booted image and the inactive OTA slot, then switch
 * the pending-rollback state so the next reset keeps the confirmed image.
 * Requires CIRVANE_CAP_OTA.
 *
 * @return esp_err_t; ESP_ERR_INVALID_STATE when no OTA data exists yet
 *         (factory image), which is the honest answer on a fresh flash.
 */
int cirvane_ota_confirm(void);

#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
/** HIL only: copy the verified running image into the inactive OTA slot. */
int cirvane_ota_stage_self(void);

/** HIL only: corrupt a copied image and prove it is rejected before selection. */
int cirvane_ota_reject_corrupt_test(void);
#endif

/** Return the printable ESP-IDF OTA state for a partition state value. */
const char *cirvane_ota_state_name(int state);

/* ======================= transactional configuration ==================== */

/** Durable settings stored as CRC-protected, generation-counted NVS slots. */
typedef struct {
    uint32_t schema_version;
    uint32_t heartbeat_ms;
    uint32_t supervisor_ms;
    uint8_t default_led_mode;
    uint8_t _reserved[3];
} cirvane_config_t;

/** Load the newest valid configuration slot, or safe defaults. */
int cirvane_config_load(cirvane_config_t *out);

/** Commit a new configuration to the inactive slot, then verify it. */
int cirvane_config_commit(const cirvane_config_t *config);

#if CONFIG_CIRVANE_HIL_DIAGNOSTICS
/** HIL only: corrupt one slot, prove fallback, then restore redundancy. */
int cirvane_config_corruption_test(void);
#endif

/** Read the current in-memory configuration snapshot. */
void cirvane_config_get(cirvane_config_t *out);

/* ============================== service mgr ============================== */

/** Sentinel service index for "not a service". */
#define CIRVANE_SERVICE_INVALID 0xFFu

/** Lifecycle phase visible in `svc` output and used by degraded-mode logic. */
typedef enum {
    SVC_PHASE_INIT = 0,  /**< registered, init not yet run */
    SVC_PHASE_RUNNING,   /**< init done; ticking normally */
    SVC_PHASE_STOPPED,   /**< intentionally stopped via svcctl */
    SVC_PHASE_BACKOFF,   /**< failed; restart timer running */
    SVC_PHASE_FAILED,    /**< restart policy exhausted */
} svc_phase_t;

/** One row of the service table as printed by the shell. */
typedef struct {
    const char *name;
    const char *desc;
    svc_phase_t phase;
    cirvane_health_t last_health;
    cirvane_restart_policy_t policy;
    uint16_t restarts;        /**< restarts since boot */
    uint32_t last_run_ms;     /**< tick duration of the most recent tick */
    uint32_t max_run_ms;      /**< worst tick duration since boot */
    uint32_t next_tick_ms;    /**< absolute uptime-ms of the next scheduled tick */
} svc_info_t;

/**
 * Register all built-in services in deterministic order. Purely static work:
 * copies descriptors into the registry array and creates nothing.
 */
void cirvane_manager_register_builtins(void);

/** Register a descriptor before cirvane_manager_start(). */
int cirvane_manager_register(const cirvane_service_t *service, uint8_t *out_idx);

/**
 * Bring up every registered service in registration order, then create the
 * tick tasks and the supervisor. Records startup timing for determinism
 * checks. Never returns on failure: a broken bring-up panics loudly.
 */
void cirvane_manager_start(void);

/** Number of registered services. */
uint8_t cirvane_manager_count(void);

/**
 * Snapshot one service's runtime info. Returns false if idx is out of range
 * or the service was never registered.
 */
bool cirvane_manager_get_info(uint8_t idx, svc_info_t *out);

/**
 * Operator/service control: stop, start, or force-restart a service.
 * Requires CIRVANE_CAP_SVCCTL. Returns an esp_err_t; refused commands leave
 * the service untouched.
 */
int cirvane_svc_control(uint8_t idx, const char *action);

/**
 * Ask the supervisor to recheck a specific service now instead of waiting
 * for its periodic sweep. Used by tests and by the failure-injection path.
 */
void cirvane_supervisor_kick(void);

/** Milliseconds since boot, monotonic, wraps at ~49 days. */
uint32_t cirvane_uptime_ms(void);

/**
 * System-wide operating condition, derived from per-service health:
 * NOMINAL when every started service is OK, DEGRADED when any service is
 * degraded/failed/stopped-but-required. Printed by `svc` and logged by the
 * supervisor.
 */
typedef enum {
    CIRVANE_SYS_NOMINAL = 0,
    CIRVANE_SYS_DEGRADED,
} cirvane_sys_state_t;

cirvane_sys_state_t cirvane_system_state(void);

/* ========================== deterministic startup ======================== */

/**
 * Startup timing evidence: wall-clock ms from app_main entry to each phase.
 * Captured once during cirvane_manager_start(); read by the `boot` shell
 * command and by HIL checks that compare consecutive boots.
 */
typedef struct {
    uint32_t nvs_done_ms;
    uint32_t bus_ready_ms;
    uint32_t services_inited_ms;
    uint32_t tasks_spawned_ms;
    uint32_t first_tick_ms;
} cirvane_boot_timing_t;

const cirvane_boot_timing_t *cirvane_boot_timing(void);

/** Mark completion of NVS/config recovery during app_main startup. */
void cirvane_boot_mark_nvs_ready(void);

#ifdef __cplusplus
}
#endif
