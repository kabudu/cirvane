/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Host driver for the bounded recovery transaction model.
 */

#include "recovery.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        failures++;
    }
}

static void test_fault_invalidates_stale_slots(void)
{
    cirvane_world_t world;
    int slot;

    cirvane_rtx_reset(&world);
    expect(cirvane_rtx_bind(&world, 1, 3, 0x12), "bind");
    slot = cirvane_rtx_alloc_slot(&world, 1);
    expect(slot >= 0, "alloc");
    expect(cirvane_rtx_slot_deliverable(&world, slot), "deliverable before");
    expect(cirvane_rtx_admit(&world, 1, CIRVANE_RTX_REASON_FAULT) ==
               CIRVANE_RTX_RESTART,
           "admit restart");
    expect(!cirvane_rtx_slot_deliverable(&world, slot), "stale not deliverable");
    expect(!cirvane_rtx_service_has_stale_work(&world, 1), "no stale work");
    expect(cirvane_rtx_health(&world, 1) == CIRVANE_HEALTH_OK, "ok after complete");
    expect(cirvane_rtx_evidence(&world, 1)->reclaimed == 1, "reclaimed one");
    expect(cirvane_rtx_evidence(&world, 1)->epoch == 2, "epoch advanced");
}

static void test_partial_recovery_cannot_be_healthy(void)
{
    cirvane_world_t world;
    cirvane_service_t *svc;

    cirvane_rtx_reset(&world);
    cirvane_rtx_bind(&world, 0, 2, 1);
    svc = &world.services[0];
    svc->recovering = 1;
    svc->health = CIRVANE_HEALTH_OK;
    expect(cirvane_rtx_health(&world, 0) == CIRVANE_HEALTH_RECOVERING,
           "recovering masks ok");
}

static void test_budget_exhaustion_fails_closed(void)
{
    cirvane_world_t world;

    cirvane_rtx_reset(&world);
    cirvane_rtx_bind(&world, 2, 1, 1);
    expect(cirvane_rtx_admit(&world, 2, CIRVANE_RTX_REASON_FAULT) ==
               CIRVANE_RTX_RESTART,
           "first restart");
    expect(cirvane_rtx_admit(&world, 2, CIRVANE_RTX_REASON_FAULT) ==
               CIRVANE_RTX_FAILED,
           "budget exhausted");
    expect(cirvane_rtx_health(&world, 2) == CIRVANE_HEALTH_FAILED, "failed health");
    expect(cirvane_rtx_alloc_slot(&world, 2) < 0, "no alloc after fail");
}

static void test_nested_admit_is_refused(void)
{
    cirvane_world_t world;

    cirvane_rtx_reset(&world);
    cirvane_rtx_bind(&world, 0, 3, 1);
    world.services[0].recovering = 1;
    expect(cirvane_rtx_admit(&world, 0, CIRVANE_RTX_REASON_FAULT) ==
               CIRVANE_RTX_REFUSED,
           "nested refused");
    expect(world.services[0].recovering == 1, "still recovering");
    expect(cirvane_rtx_health(&world, 0) == CIRVANE_HEALTH_RECOVERING,
           "nested not healthy");
}

static void test_deadline_is_backoff_not_ok(void)
{
    cirvane_world_t world;

    cirvane_rtx_reset(&world);
    cirvane_rtx_bind(&world, 3, 4, 1);
    expect(cirvane_rtx_admit(&world, 3, CIRVANE_RTX_REASON_DEADLINE) ==
               CIRVANE_RTX_BACKOFF,
           "deadline backoff");
    expect(cirvane_rtx_health(&world, 3) == CIRVANE_HEALTH_DEGRADED,
           "degraded not ok");
}

static void test_malformed_and_pool_exhaustion(void)
{
    cirvane_world_t world;
    unsigned i;
    int last = -1;

    cirvane_rtx_reset(&world);
    expect(!cirvane_rtx_bind(&world, 9, 1, 1), "bad service");
    expect(!cirvane_rtx_bind(&world, 0, 0, 1), "zero budget");
    expect(cirvane_rtx_admit(&world, 0, CIRVANE_RTX_REASON_FAULT) ==
               CIRVANE_RTX_REFUSED,
           "unbound refuse");
    cirvane_rtx_bind(&world, 0, 2, 1);
    expect(cirvane_rtx_admit(&world, 0, 99) == CIRVANE_RTX_REFUSED,
           "bad reason");
    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        last = cirvane_rtx_alloc_slot(&world, 0);
        expect(last >= 0, "fill pool");
    }
    expect(cirvane_rtx_alloc_slot(&world, 0) < 0, "pool exhausted");
    expect(cirvane_rtx_admit(&world, 0, CIRVANE_RTX_REASON_FAULT) ==
               CIRVANE_RTX_RESTART,
           "recover after full");
    expect(cirvane_rtx_alloc_slot(&world, 0) >= 0, "alloc after reclaim");
}

static void test_free_slot_and_capabilities(void)
{
    cirvane_world_t world;
    int slot;

    cirvane_rtx_reset(&world);
    cirvane_rtx_bind(&world, 0, 2, 0);
    expect(!cirvane_rtx_cap_check(&world, 0, 1), "no lease");
    expect(cirvane_rtx_cap_grant(&world, 0, 0x12), "grant");
    expect(cirvane_rtx_cap_check(&world, 0, 0x02), "has bit");
    expect(!cirvane_rtx_cap_check(&world, 0, 0x21), "missing bit");
    slot = cirvane_rtx_alloc_slot(&world, 0);
    expect(cirvane_rtx_free_slot(&world, slot), "free");
    expect(!cirvane_rtx_free_slot(&world, slot), "double free");
    expect(cirvane_rtx_cap_revoke(&world, 0), "revoke");
    expect(!cirvane_rtx_cap_check(&world, 0, 0x12), "revoked");
    expect(!cirvane_rtx_cap_grant(&world, 9, 1), "bad service grant");
}

static void test_evidence_is_fixed_and_replaced(void)
{
    cirvane_world_t world;
    const cirvane_evidence_t *ev;

    cirvane_rtx_reset(&world);
    cirvane_rtx_bind(&world, 1, 3, 7);
    cirvane_rtx_admit(&world, 1, CIRVANE_RTX_REASON_FAULT);
    ev = cirvane_rtx_evidence(&world, 1);
    expect(sizeof(*ev) == CIRVANE_RTX_EVIDENCE_SIZE, "size 16");
    expect(ev->outcome == CIRVANE_RTX_RESTART, "first outcome");
    cirvane_rtx_admit(&world, 1, CIRVANE_RTX_REASON_DEADLINE);
    ev = cirvane_rtx_evidence(&world, 1);
    expect(ev->outcome == CIRVANE_RTX_BACKOFF, "replaced not appended");
    expect(ev->reserved == 0, "reserved clear");
}

int main(void)
{
    test_fault_invalidates_stale_slots();
    test_partial_recovery_cannot_be_healthy();
    test_budget_exhaustion_fails_closed();
    test_nested_admit_is_refused();
    test_deadline_is_backoff_not_ok();
    test_malformed_and_pool_exhaustion();
    test_free_slot_and_capabilities();
    test_evidence_is_fixed_and_replaced();
    if (failures != 0) {
        fprintf(stderr, "%d recovery model checks failed\n", failures);
        return 1;
    }
    puts("recovery model checks passed");
    return 0;
}
