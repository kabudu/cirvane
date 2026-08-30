/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host driver for the Stage 2 Cirvane kernel core.
 */

#include "kernel.h"

#include <stdio.h>
#include <string.h>

static int failures;
static uint32_t g_isr_count;
static uint32_t g_tick_a;
static uint32_t g_tick_b;
static cirvane_kernel_t *g_k;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        failures++;
    }
}

static void isr_count(void)
{
    g_isr_count += 1;
}

static void tick_a(uint8_t service)
{
    (void)service;
    g_tick_a += 1;
}

static void tick_b(uint8_t service)
{
    (void)service;
    g_tick_b += 1;
}

static void producer_tick(uint8_t service)
{
    int32_t slot = cirvane_syscall(g_k, CIRVANE_SYS_MSG_ALLOC, service, 0, 0);
    if (slot >= 0) {
        uint8_t *payload = cirvane_msg_payload(g_k, (int)slot);
        payload[0] = 0xA5;
        cirvane_syscall(g_k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 2, 7);
    }
}

static void consumer_tick(uint8_t service)
{
    int32_t slot = cirvane_syscall(g_k, CIRVANE_SYS_MSG_RECV, service, 0, 0);
    if (slot >= 0) {
        expect(cirvane_msg_payload(g_k, (int)slot)[0] == 0xA5, "payload");
        expect(g_k->msgs[slot].type == 7, "type");
        expect(g_k->msgs[slot].sender == 1, "sender");
        cirvane_syscall(g_k, CIRVANE_SYS_MSG_FREE, (uint32_t)slot, 0, 0);
        g_tick_b += 1;
    }
}

static void test_panic_and_irq(void)
{
    cirvane_kernel_t k;

    cirvane_kernel_init(&k);
    expect(cirvane_panic_reason(&k) == CIRVANE_PANIC_NONE, "no panic");
    expect(cirvane_irq_attach(&k, 0, isr_count) == CIRVANE_SYS_OK, "attach");
    g_isr_count = 0;
    cirvane_irq_dispatch(&k, 0);
    expect(g_isr_count == 1, "isr ran");
    cirvane_irq_dispatch(&k, 9);
    expect(cirvane_panic_reason(&k) == CIRVANE_PANIC_BAD_IRQ, "bad irq panic");
    expect(k.panic_mcause == 0 && k.panic_mepc == 0, "irq panic detail");
    cirvane_kernel_init(&k);
    cirvane_panic_fault(&k, CIRVANE_PANIC_UNKNOWN_TRAP, 0x11u, 0x20u);
    expect(k.panic_mcause == 0x11u && k.panic_mepc == 0x20u, "trap detail");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 2, 1) ==
               CIRVANE_SYS_REFUSED,
           "syscall after panic");
}

static void test_sched_skips_failed_and_unbound(void)
{
    cirvane_kernel_t k;

    cirvane_kernel_init(&k);
    g_tick_a = 0;
    g_tick_b = 0;
    cirvane_sched_set_tick(&k, 0, tick_a);
    cirvane_sched_set_tick(&k, 1, tick_b);
    expect(cirvane_sched_step(&k) == CIRVANE_SYS_REFUSED, "idle unbound");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 2, CIRVANE_CAP_MSG) ==
               CIRVANE_SYS_OK,
           "bind 0");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 1, 1, CIRVANE_CAP_MSG) ==
               CIRVANE_SYS_OK,
           "bind 1");
    expect(cirvane_sched_step(&k) == 0, "first 0");
    expect(cirvane_sched_step(&k) == 1, "then 1");
    expect(cirvane_sched_step(&k) == 0, "wrap 0");
    expect(g_tick_a == 2 && g_tick_b == 1, "rr counts");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 1,
                           CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_RESTART,
           "first restart");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 1,
                           CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_FAILED,
           "budget exhausted");
    expect(cirvane_sched_step(&k) == 0, "skip failed");
    expect(cirvane_sched_step(&k) == 0, "still only 0");
    cirvane_panic(&k, CIRVANE_PANIC_UNKNOWN_TRAP);
    expect(cirvane_sched_step(&k) == CIRVANE_SYS_REFUSED, "panic idle");
}

static void test_messages_and_caps(void)
{
    cirvane_kernel_t k;
    int32_t slot;

    cirvane_kernel_init(&k);
    g_k = &k;
    g_tick_b = 0;
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 1, 3, 0) == CIRVANE_SYS_OK,
           "bind producer no cap");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 2, 3, CIRVANE_CAP_MSG) ==
               CIRVANE_SYS_OK,
           "bind consumer");
    slot = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 1, 0, 0);
    expect(slot >= 0, "alloc");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 2, 1) ==
               CIRVANE_SYS_REFUSED,
           "send without cap");
    expect(cirvane_syscall(&k, CIRVANE_SYS_CAP_GRANT, 1, CIRVANE_CAP_MSG, 0) ==
               CIRVANE_SYS_OK,
           "grant");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 2, 1) ==
               CIRVANE_SYS_OK,
           "send with cap");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_RECV, 2, 0, 0) == slot, "recv");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_FREE, (uint32_t)slot, 0, 0) ==
               CIRVANE_SYS_OK,
           "free");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_FREE, (uint32_t)slot, 0, 0) ==
               CIRVANE_SYS_REFUSED,
           "double free");
    expect(cirvane_syscall(&k, CIRVANE_SYS_CAP_REVOKE, 1, 0, 0) == CIRVANE_SYS_OK,
           "revoke");
    slot = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 1, 0, 0);
    expect(slot >= 0, "alloc after revoke");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 2, 1) ==
               CIRVANE_SYS_REFUSED,
           "send after revoke");
}

static void test_stale_send_after_admit(void)
{
    cirvane_kernel_t k;
    int32_t slot;

    cirvane_kernel_init(&k);
    cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 2, CIRVANE_CAP_MSG);
    cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 1, 2, CIRVANE_CAP_MSG);
    slot = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
    expect(slot >= 0, "alloc before admit");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                           CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_RESTART,
           "admit");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_SEND, (uint32_t)slot, 1, 1) ==
               CIRVANE_SYS_REFUSED,
           "stale send");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_RECV, 1, 0, 0) ==
               CIRVANE_SYS_REFUSED,
           "no stale recv");
    expect(cirvane_syscall(&k, CIRVANE_SYS_CAP_GRANT, 0, CIRVANE_CAP_MSG, 0) ==
               CIRVANE_SYS_OK,
           "regrant after admit");
}

static void test_malformed_and_exhaustion(void)
{
    cirvane_kernel_t k;
    unsigned i;
    int32_t last = CIRVANE_SYS_REFUSED;

    cirvane_kernel_init(&k);
    expect(cirvane_syscall(&k, 99, 0, 0, 0) == CIRVANE_SYS_REFUSED,
           "unknown nr");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 9, 1, 1) ==
               CIRVANE_SYS_REFUSED,
           "bad bind");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0) ==
               CIRVANE_SYS_REFUSED,
           "alloc unbound");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 0, 2, CIRVANE_CAP_MSG) ==
               CIRVANE_SYS_OK,
           "bind");
    for (i = 0; i < CIRVANE_RTX_SLOT_COUNT; i++) {
        last = cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0);
        expect(last >= 0, "fill");
    }
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0) ==
               CIRVANE_SYS_REFUSED,
           "exhausted");
    expect(cirvane_syscall(&k, CIRVANE_SYS_RTX_ADMIT, 0,
                           CIRVANE_RTX_REASON_FAULT, 0) == CIRVANE_RTX_RESTART,
           "reclaim");
    expect(cirvane_syscall(&k, CIRVANE_SYS_CAP_GRANT, 0, CIRVANE_CAP_MSG, 0) ==
               CIRVANE_SYS_OK,
           "grant after reclaim");
    expect(cirvane_syscall(&k, CIRVANE_SYS_MSG_ALLOC, 0, 0, 0) >= 0,
           "alloc after reclaim");
}

static void test_sched_message_path(void)
{
    cirvane_kernel_t k;

    cirvane_kernel_init(&k);
    g_k = &k;
    g_tick_b = 0;
    cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 1, 2, CIRVANE_CAP_MSG);
    cirvane_syscall(&k, CIRVANE_SYS_RTX_BIND, 2, 2, CIRVANE_CAP_MSG);
    cirvane_sched_set_tick(&k, 1, producer_tick);
    cirvane_sched_set_tick(&k, 2, consumer_tick);
    expect(cirvane_sched_step(&k) == 1, "produce");
    expect(cirvane_sched_step(&k) == 2, "consume");
    expect(g_tick_b == 1, "consumed");
}

int main(void)
{
    test_panic_and_irq();
    test_sched_skips_failed_and_unbound();
    test_messages_and_caps();
    test_stale_send_after_admit();
    test_malformed_and_exhaustion();
    test_sched_message_path();
    if (failures != 0) {
        fprintf(stderr, "%d kernel core checks failed\n", failures);
        return 1;
    }
    puts("kernel core checks passed");
    return 0;
}
