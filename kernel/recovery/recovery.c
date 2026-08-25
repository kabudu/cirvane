/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 */

#include "recovery.h"

static void memzero(void *ptr, unsigned len)
{
    uint8_t *bytes = ptr;
    unsigned i;
    for (i = 0; i < len; i++) {
        bytes[i] = 0;
    }
}

static bool valid_service(uint8_t service)
{
    return service < CIRVANE_RTX_SERVICE_COUNT;
}

static void write_evidence(cirvane_world_t *world, uint8_t service, uint8_t outcome,
                           uint8_t reason, uint8_t reclaimed)
{
    cirvane_service_t *svc = &world->services[service];
    cirvane_evidence_t *ev = &world->evidence[service];
    ev->epoch = svc->epoch;
    ev->generation = svc->state_generation;
    ev->outcome = outcome;
    ev->reason = reason;
    ev->reclaimed = reclaimed;
    ev->health_after = svc->health;
    ev->reserved = 0;
    world->last_outcome = outcome;
}

void cirvane_rtx_reset(cirvane_world_t *world)
{
    memzero(world, sizeof(*world));
}

bool cirvane_rtx_bind(cirvane_world_t *world, uint8_t service,
                      uint8_t restart_budget, uint8_t cap_lease)
{
    cirvane_service_t *svc;

    if (!valid_service(service) || restart_budget == 0 ||
        restart_budget > CIRVANE_RTX_BUDGET_MAX) {
        return false;
    }

    svc = &world->services[service];
    if (svc->recovering) {
        return false;
    }

    svc->bound = 1;
    svc->recovering = 0;
    svc->health = CIRVANE_HEALTH_OK;
    svc->restart_budget = restart_budget;
    svc->restart_used = 0;
    svc->cap_lease = cap_lease;
    svc->epoch = 1;
    svc->state_generation = 1;
    memzero(&world->evidence[service], sizeof(world->evidence[service]));
    return true;
}

int cirvane_rtx_alloc_slot(cirvane_world_t *world, uint8_t service)
{
    unsigned i;
    cirvane_service_t *svc;

    if (!valid_service(service)) {
        return -1;
    }
    svc = &world->services[service];
    if (!svc->bound || svc->recovering || svc->health == CIRVANE_HEALTH_FAILED) {
        return -1;
    }

    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        if (!world->slots[i].in_use) {
            world->slots[i].in_use = 1;
            world->slots[i].owner = service;
            world->slots[i].epoch = svc->epoch;
            return (int)i;
        }
    }
    return -1;
}

bool cirvane_rtx_slot_deliverable(const cirvane_world_t *world, int slot)
{
    const cirvane_slot_t *entry;
    const cirvane_service_t *svc;

    if (slot < 0 || slot >= CIRVANE_RTX_SLOT_COUNT) {
        return false;
    }
    entry = &world->slots[slot];
    if (!entry->in_use || !valid_service(entry->owner)) {
        return false;
    }
    svc = &world->services[entry->owner];
    if (!svc->bound || svc->recovering) {
        return false;
    }
    return entry->epoch == svc->epoch;
}

bool cirvane_rtx_service_has_stale_work(const cirvane_world_t *world,
                                        uint8_t service)
{
    unsigned i;

    if (!valid_service(service)) {
        return false;
    }
    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        const cirvane_slot_t *entry = &world->slots[i];
        if (entry->in_use && entry->owner == service &&
            entry->epoch != world->services[service].epoch) {
            return true;
        }
    }
    return false;
}

uint8_t cirvane_rtx_health(const cirvane_world_t *world, uint8_t service)
{
    if (!valid_service(service) || !world->services[service].bound) {
        return CIRVANE_HEALTH_FAILED;
    }
    if (world->services[service].recovering) {
        return CIRVANE_HEALTH_RECOVERING;
    }
    return world->services[service].health;
}

uint8_t cirvane_rtx_admit(cirvane_world_t *world, uint8_t service, uint8_t reason)
{
    cirvane_service_t *svc;
    unsigned i;
    uint8_t reclaimed = 0;
    uint8_t outcome;

    if (!valid_service(service) || (reason != CIRVANE_RTX_REASON_FAULT &&
                                    reason != CIRVANE_RTX_REASON_DEADLINE)) {
        if (valid_service(service)) {
            write_evidence(world, service, CIRVANE_RTX_REFUSED,
                           CIRVANE_RTX_REASON_MALFORMED, 0);
        }
        return CIRVANE_RTX_REFUSED;
    }

    svc = &world->services[service];
    if (!svc->bound) {
        write_evidence(world, service, CIRVANE_RTX_REFUSED,
                       CIRVANE_RTX_REASON_MALFORMED, 0);
        return CIRVANE_RTX_REFUSED;
    }
    if (svc->recovering) {
        write_evidence(world, service, CIRVANE_RTX_REFUSED,
                       CIRVANE_RTX_REASON_NESTED, 0);
        return CIRVANE_RTX_REFUSED;
    }

    svc->recovering = 1;
    svc->health = CIRVANE_HEALTH_RECOVERING;
    svc->epoch += 1u;
    svc->state_generation += 1u;
    svc->cap_lease = 0;

    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        if (world->slots[i].in_use && world->slots[i].owner == service) {
            world->slots[i].in_use = 0;
            world->slots[i].epoch = 0;
            world->slots[i].owner = 0;
            reclaimed++;
        }
    }

    if (svc->restart_used >= svc->restart_budget) {
        svc->health = CIRVANE_HEALTH_FAILED;
        svc->recovering = 0;
        write_evidence(world, service, CIRVANE_RTX_FAILED,
                       CIRVANE_RTX_REASON_BUDGET, reclaimed);
        return CIRVANE_RTX_FAILED;
    }

    svc->restart_used += 1;
    if (reason == CIRVANE_RTX_REASON_DEADLINE) {
        outcome = CIRVANE_RTX_BACKOFF;
        svc->health = CIRVANE_HEALTH_DEGRADED;
    } else {
        outcome = CIRVANE_RTX_RESTART;
        svc->health = CIRVANE_HEALTH_OK;
    }
    svc->recovering = 0;
    write_evidence(world, service, outcome, reason, reclaimed);
    return outcome;
}

const cirvane_evidence_t *cirvane_rtx_evidence(const cirvane_world_t *world,
                                               uint8_t service)
{
    if (!valid_service(service)) {
        return 0;
    }
    return &world->evidence[service];
}

bool cirvane_rtx_free_slot(cirvane_world_t *world, int slot)
{
    if (slot < 0 || slot >= CIRVANE_RTX_SLOT_COUNT) {
        return false;
    }
    if (!world->slots[slot].in_use) {
        return false;
    }
    world->slots[slot].in_use = 0;
    world->slots[slot].epoch = 0;
    world->slots[slot].owner = 0;
    return true;
}

bool cirvane_rtx_cap_grant(cirvane_world_t *world, uint8_t service,
                           uint8_t lease)
{
    cirvane_service_t *svc;

    if (!valid_service(service)) {
        return false;
    }
    svc = &world->services[service];
    if (!svc->bound || svc->recovering || svc->health == CIRVANE_HEALTH_FAILED) {
        return false;
    }
    svc->cap_lease = lease;
    return true;
}

bool cirvane_rtx_cap_revoke(cirvane_world_t *world, uint8_t service)
{
    if (!valid_service(service) || !world->services[service].bound) {
        return false;
    }
    world->services[service].cap_lease = 0;
    return true;
}

bool cirvane_rtx_cap_check(const cirvane_world_t *world, uint8_t service,
                           uint8_t need)
{
    const cirvane_service_t *svc;

    if (!valid_service(service) || need == 0) {
        return false;
    }
    svc = &world->services[service];
    if (!svc->bound || svc->recovering || svc->health == CIRVANE_HEALTH_FAILED) {
        return false;
    }
    return (svc->cap_lease & need) == need;
}
