/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Host-testable model of the Cirvane bounded recovery transaction.
 * Stage 2 kernel syscalls own this ABI. It is not a novelty claim.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CIRVANE_RTX_SERVICE_COUNT 8
#define CIRVANE_RTX_SLOT_COUNT 32
#define CIRVANE_RTX_EVIDENCE_SIZE 16
#define CIRVANE_RTX_BUDGET_MAX 8

#define CIRVANE_HEALTH_OK 0
#define CIRVANE_HEALTH_DEGRADED 1
#define CIRVANE_HEALTH_FAILED 2
#define CIRVANE_HEALTH_RECOVERING 3

#define CIRVANE_RTX_RESTART 0
#define CIRVANE_RTX_BACKOFF 1
#define CIRVANE_RTX_FAILED 2
#define CIRVANE_RTX_REFUSED 3

#define CIRVANE_RTX_REASON_FAULT 0
#define CIRVANE_RTX_REASON_DEADLINE 1
#define CIRVANE_RTX_REASON_NESTED 2
#define CIRVANE_RTX_REASON_MALFORMED 3
#define CIRVANE_RTX_REASON_BUDGET 4
#define CIRVANE_RTX_REASON_BUSY 5
#define CIRVANE_RTX_REASON_EXHAUSTED 6

typedef struct {
    uint32_t epoch;
    uint8_t in_use;
    uint8_t owner;
} cirvane_slot_t;

typedef struct {
    uint8_t bound;
    uint8_t recovering;
    uint8_t health;
    uint8_t restart_budget;
    uint8_t restart_used;
    uint8_t cap_lease;
    uint32_t epoch;
    uint32_t state_generation;
} cirvane_service_t;

typedef struct {
    uint32_t epoch;
    uint32_t generation;
    uint8_t outcome;
    uint8_t reason;
    uint8_t reclaimed;
    uint8_t health_after;
    uint32_t reserved;
} cirvane_evidence_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(cirvane_evidence_t) == CIRVANE_RTX_EVIDENCE_SIZE,
               "evidence record must stay 16 bytes");
#endif

typedef struct {
    cirvane_service_t services[CIRVANE_RTX_SERVICE_COUNT];
    cirvane_slot_t slots[CIRVANE_RTX_SLOT_COUNT];
    cirvane_evidence_t evidence[CIRVANE_RTX_SERVICE_COUNT];
    uint8_t last_outcome;
} cirvane_world_t;

void cirvane_rtx_reset(cirvane_world_t *world);
bool cirvane_rtx_bind(cirvane_world_t *world, uint8_t service,
                      uint8_t restart_budget, uint8_t cap_lease);
int cirvane_rtx_alloc_slot(cirvane_world_t *world, uint8_t service);
bool cirvane_rtx_slot_deliverable(const cirvane_world_t *world, int slot);
uint8_t cirvane_rtx_admit(cirvane_world_t *world, uint8_t service, uint8_t reason);
uint8_t cirvane_rtx_health(const cirvane_world_t *world, uint8_t service);
bool cirvane_rtx_service_has_stale_work(const cirvane_world_t *world,
                                        uint8_t service);
const cirvane_evidence_t *cirvane_rtx_evidence(const cirvane_world_t *world,
                                               uint8_t service);
bool cirvane_rtx_free_slot(cirvane_world_t *world, int slot);
bool cirvane_rtx_cap_grant(cirvane_world_t *world, uint8_t service,
                           uint8_t lease);
bool cirvane_rtx_cap_revoke(cirvane_world_t *world, uint8_t service);
bool cirvane_rtx_cap_check(const cirvane_world_t *world, uint8_t service,
                           uint8_t need);
