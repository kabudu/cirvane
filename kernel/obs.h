/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 *
 * Bounded crash, evidence and quiet-shell encodings. No allocator.
 * The USB shell is privileged and unauthenticated.
 */

#pragma once

#include "kernel.h"

#include <stdint.h>

#define CIRVANE_OBS_LINE_MAX 96
#define CIRVANE_SHELL_CMD_MAX 32
#define CIRVANE_SHELL_PROMPT "cirvane>"

typedef struct {
    char buf[CIRVANE_SHELL_CMD_MAX];
    uint8_t len;
    uint8_t overflow;
    uint8_t ready;
} cirvane_shell_t;

int cirvane_obs_crash(char *out, unsigned cap, uint32_t reason, uint32_t mcause,
                      uint32_t mepc);
int cirvane_obs_evidence(char *out, unsigned cap, uint8_t service,
                         const cirvane_evidence_t *ev);
int cirvane_obs_info(char *out, unsigned cap, uint32_t panic_reason);

void cirvane_shell_init(cirvane_shell_t *sh);
int cirvane_shell_push(cirvane_shell_t *sh, uint8_t byte);
int cirvane_shell_run(cirvane_shell_t *sh, const cirvane_kernel_t *k, char *out,
                      unsigned cap);
