/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Portable Cirvane kernel: panic, IRQ table, cooperative scheduler,
 * typed messages and frozen syscalls. No FreeRTOS, no allocator.
 */

#include "kernel.h"

static void memzero(void *ptr, unsigned len)
{
    uint8_t *bytes = ptr;
    unsigned i;
    for (i = 0; i < len; i++) {
        bytes[i] = 0;
    }
}

static void clear_msg(cirvane_kernel_t *k, int slot)
{
    memzero(&k->msgs[slot], sizeof(k->msgs[slot]));
}

static void clear_unused_msgs(cirvane_kernel_t *k)
{
    unsigned i;
    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        if (!k->rtx.slots[i].in_use) {
            clear_msg(k, (int)i);
        }
    }
}

void cirvane_kernel_init(cirvane_kernel_t *k)
{
    memzero(k, sizeof(*k));
    cirvane_rtx_reset(&k->rtx);
}

void cirvane_panic(cirvane_kernel_t *k, uint32_t reason)
{
    k->panicked = 1;
    k->panic_reason = reason;
}

uint32_t cirvane_panic_reason(const cirvane_kernel_t *k)
{
    return k->panic_reason;
}

int cirvane_irq_attach(cirvane_kernel_t *k, uint8_t irq, cirvane_isr_fn isr)
{
    if (k->panicked || irq >= CIRVANE_IRQ_LIMIT || isr == 0) {
        return CIRVANE_SYS_REFUSED;
    }
    k->isrs[irq] = isr;
    return CIRVANE_SYS_OK;
}

void cirvane_irq_dispatch(cirvane_kernel_t *k, uint8_t irq)
{
    if (irq >= CIRVANE_IRQ_LIMIT) {
        cirvane_panic(k, CIRVANE_PANIC_BAD_IRQ);
        return;
    }
    if (k->isrs[irq] != 0) {
        k->isrs[irq]();
    }
}

void cirvane_sched_set_tick(cirvane_kernel_t *k, uint8_t service,
                            cirvane_tick_fn tick)
{
    if (service < CIRVANE_RTX_SERVICE_COUNT) {
        k->ticks[service] = tick;
    }
}

int cirvane_sched_step(cirvane_kernel_t *k)
{
    unsigned i;
    uint8_t start;

    if (k->panicked) {
        return CIRVANE_SYS_REFUSED;
    }

    start = k->next_service;
    for (i = 0; i < CIRVANE_RTX_SERVICE_COUNT; i++) {
        uint8_t service = (uint8_t)((start + i) % CIRVANE_RTX_SERVICE_COUNT);
        const cirvane_service_t *svc = &k->rtx.services[service];

        if (!svc->bound || svc->recovering ||
            svc->health == CIRVANE_HEALTH_FAILED || k->ticks[service] == 0) {
            continue;
        }
        k->next_service = (uint8_t)((service + 1u) % CIRVANE_RTX_SERVICE_COUNT);
        k->ticks[service](service);
        return (int)service;
    }
    return CIRVANE_SYS_REFUSED;
}

uint8_t *cirvane_msg_payload(cirvane_kernel_t *k, int slot)
{
    if (slot < 0 || slot >= CIRVANE_RTX_SLOT_COUNT || !k->rtx.slots[slot].in_use) {
        return 0;
    }
    return k->msgs[slot].payload;
}

static int32_t sys_bind(cirvane_kernel_t *k, uint32_t service, uint32_t budget,
                        uint32_t lease)
{
    if (service > 0xffu || budget > 0xffu || lease > 0xffu) {
        return CIRVANE_SYS_REFUSED;
    }
    return cirvane_rtx_bind(&k->rtx, (uint8_t)service, (uint8_t)budget,
                            (uint8_t)lease)
               ? CIRVANE_SYS_OK
               : CIRVANE_SYS_REFUSED;
}

static int32_t sys_admit(cirvane_kernel_t *k, uint32_t service, uint32_t reason)
{
    uint8_t outcome;

    if (service > 0xffu || reason > 0xffu) {
        return CIRVANE_SYS_REFUSED;
    }
    outcome = cirvane_rtx_admit(&k->rtx, (uint8_t)service, (uint8_t)reason);
    clear_unused_msgs(k);
    return (int32_t)outcome;
}

static int32_t sys_alloc(cirvane_kernel_t *k, uint32_t service)
{
    int slot;

    if (service > 0xffu) {
        return CIRVANE_SYS_REFUSED;
    }
    slot = cirvane_rtx_alloc_slot(&k->rtx, (uint8_t)service);
    if (slot < 0) {
        return CIRVANE_SYS_REFUSED;
    }
    clear_msg(k, slot);
    return slot;
}

static int32_t sys_send(cirvane_kernel_t *k, uint32_t slot, uint32_t dest,
                        uint32_t type)
{
    uint8_t sender;
    cirvane_service_t *dst;

    if (slot >= CIRVANE_RTX_SLOT_COUNT || dest >= CIRVANE_RTX_SERVICE_COUNT ||
        type > 0xffu) {
        return CIRVANE_SYS_REFUSED;
    }
    if (!cirvane_rtx_slot_deliverable(&k->rtx, (int)slot)) {
        return CIRVANE_SYS_REFUSED;
    }
    sender = k->rtx.slots[slot].owner;
    if (!cirvane_rtx_cap_check(&k->rtx, sender, CIRVANE_CAP_MSG)) {
        return CIRVANE_SYS_REFUSED;
    }
    dst = &k->rtx.services[dest];
    if (!dst->bound || dst->recovering || dst->health == CIRVANE_HEALTH_FAILED) {
        return CIRVANE_SYS_REFUSED;
    }
    if (!cirvane_rtx_cap_check(&k->rtx, (uint8_t)dest, CIRVANE_CAP_MSG)) {
        return CIRVANE_SYS_REFUSED;
    }

    k->msgs[slot].type = (uint8_t)type;
    k->msgs[slot].sender = sender;
    k->msgs[slot].payload_len = CIRVANE_MSG_PAYLOAD_MAX;
    k->msgs[slot].queued = 1;
    k->rtx.slots[slot].owner = (uint8_t)dest;
    k->rtx.slots[slot].epoch = dst->epoch;
    return CIRVANE_SYS_OK;
}

static int32_t sys_recv(cirvane_kernel_t *k, uint32_t service)
{
    unsigned i;

    if (service > 0xffu ||
        !cirvane_rtx_cap_check(&k->rtx, (uint8_t)service, CIRVANE_CAP_MSG)) {
        return CIRVANE_SYS_REFUSED;
    }
    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        if (!k->msgs[i].queued) {
            continue;
        }
        if (k->rtx.slots[i].owner != service) {
            continue;
        }
        if (!cirvane_rtx_slot_deliverable(&k->rtx, (int)i)) {
            continue;
        }
        k->msgs[i].queued = 0;
        return (int32_t)i;
    }
    return CIRVANE_SYS_REFUSED;
}

static int32_t sys_free(cirvane_kernel_t *k, uint32_t slot)
{
    if (slot >= CIRVANE_RTX_SLOT_COUNT) {
        return CIRVANE_SYS_REFUSED;
    }
    if (!cirvane_rtx_free_slot(&k->rtx, (int)slot)) {
        return CIRVANE_SYS_REFUSED;
    }
    clear_msg(k, (int)slot);
    return CIRVANE_SYS_OK;
}

static int32_t sys_grant(cirvane_kernel_t *k, uint32_t service, uint32_t lease)
{
    if (service > 0xffu || lease > 0xffu) {
        return CIRVANE_SYS_REFUSED;
    }
    return cirvane_rtx_cap_grant(&k->rtx, (uint8_t)service, (uint8_t)lease)
               ? CIRVANE_SYS_OK
               : CIRVANE_SYS_REFUSED;
}

static int32_t sys_revoke(cirvane_kernel_t *k, uint32_t service)
{
    if (service > 0xffu) {
        return CIRVANE_SYS_REFUSED;
    }
    return cirvane_rtx_cap_revoke(&k->rtx, (uint8_t)service) ? CIRVANE_SYS_OK
                                                            : CIRVANE_SYS_REFUSED;
}

int32_t cirvane_syscall(cirvane_kernel_t *k, uint32_t nr, uint32_t a0,
                        uint32_t a1, uint32_t a2)
{
    if (k->panicked) {
        return CIRVANE_SYS_REFUSED;
    }
    switch (nr) {
    case CIRVANE_SYS_RTX_BIND:
        return sys_bind(k, a0, a1, a2);
    case CIRVANE_SYS_RTX_ADMIT:
        return sys_admit(k, a0, a1);
    case CIRVANE_SYS_MSG_ALLOC:
        return sys_alloc(k, a0);
    case CIRVANE_SYS_MSG_SEND:
        return sys_send(k, a0, a1, a2);
    case CIRVANE_SYS_MSG_RECV:
        return sys_recv(k, a0);
    case CIRVANE_SYS_MSG_FREE:
        return sys_free(k, a0);
    case CIRVANE_SYS_CAP_GRANT:
        return sys_grant(k, a0, a1);
    case CIRVANE_SYS_CAP_REVOKE:
        return sys_revoke(k, a0);
    default:
        return CIRVANE_SYS_REFUSED;
    }
}
