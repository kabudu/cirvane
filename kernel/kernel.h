/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Stage 2 Cirvane kernel core. Host-testable. No FreeRTOS, no allocator.
 * C5 reset, trap entry, timer and USB remain in kernel/spike.
 */

#pragma once

#include "recovery.h"

#include <stdint.h>

#define CIRVANE_SYS_RTX_BIND 1u
#define CIRVANE_SYS_RTX_ADMIT 2u
#define CIRVANE_SYS_MSG_ALLOC 3u
#define CIRVANE_SYS_MSG_SEND 4u
#define CIRVANE_SYS_MSG_RECV 5u
#define CIRVANE_SYS_MSG_FREE 6u
#define CIRVANE_SYS_CAP_GRANT 7u
#define CIRVANE_SYS_CAP_REVOKE 8u

#define CIRVANE_MSG_PAYLOAD_MAX 24
#define CIRVANE_IRQ_LIMIT 8
#define CIRVANE_CAP_MSG 0x1u

#define CIRVANE_PANIC_NONE 0u
#define CIRVANE_PANIC_UNKNOWN_TRAP 1u
#define CIRVANE_PANIC_BAD_IRQ 2u

#define CIRVANE_SYS_OK 0
#define CIRVANE_SYS_REFUSED -1

typedef void (*cirvane_tick_fn)(uint8_t service);
typedef void (*cirvane_isr_fn)(void);

typedef struct {
    uint8_t type;
    uint8_t sender;
    uint8_t payload_len;
    uint8_t queued;
    uint8_t payload[CIRVANE_MSG_PAYLOAD_MAX];
} cirvane_msg_t;

typedef struct {
    cirvane_world_t rtx;
    cirvane_msg_t msgs[CIRVANE_RTX_SLOT_COUNT];
    cirvane_tick_fn ticks[CIRVANE_RTX_SERVICE_COUNT];
    cirvane_isr_fn isrs[CIRVANE_IRQ_LIMIT];
    uint8_t next_service;
    uint8_t panicked;
    uint32_t panic_reason;
} cirvane_kernel_t;

void cirvane_kernel_init(cirvane_kernel_t *k);
int32_t cirvane_syscall(cirvane_kernel_t *k, uint32_t nr, uint32_t a0,
                        uint32_t a1, uint32_t a2);
uint8_t *cirvane_msg_payload(cirvane_kernel_t *k, int slot);

void cirvane_sched_set_tick(cirvane_kernel_t *k, uint8_t service,
                            cirvane_tick_fn tick);
int cirvane_sched_step(cirvane_kernel_t *k);

int cirvane_irq_attach(cirvane_kernel_t *k, uint8_t irq, cirvane_isr_fn isr);
void cirvane_irq_dispatch(cirvane_kernel_t *k, uint8_t irq);

void cirvane_panic(cirvane_kernel_t *k, uint32_t reason);
uint32_t cirvane_panic_reason(const cirvane_kernel_t *k);
