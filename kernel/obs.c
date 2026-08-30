/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 */

#include "obs.h"

static void memzero(void *ptr, unsigned len)
{
    uint8_t *bytes = ptr;
    unsigned i;
    for (i = 0; i < len; i++) {
        bytes[i] = 0;
    }
}

static int append_char(char *out, unsigned cap, unsigned *n, char c)
{
    if (*n + 1u >= cap) {
        return -1;
    }
    out[*n] = c;
    *n += 1u;
    out[*n] = 0;
    return 0;
}

static int append_str(char *out, unsigned cap, unsigned *n, const char *s)
{
    while (*s) {
        if (append_char(out, cap, n, *s) != 0) {
            return -1;
        }
        s++;
    }
    return 0;
}

static int append_u32(char *out, unsigned cap, unsigned *n, uint32_t value)
{
    char tmp[10];
    unsigned len = 0;
    uint32_t v = value;

    if (v == 0) {
        return append_char(out, cap, n, '0');
    }
    while (v != 0 && len < 10) {
        tmp[len++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (len > 0) {
        len--;
        if (append_char(out, cap, n, tmp[len]) != 0) {
            return -1;
        }
    }
    return 0;
}

static int append_hex8(char *out, unsigned cap, unsigned *n, uint32_t value)
{
    static const char hex[] = "0123456789abcdef";
    unsigned i;
    for (i = 8; i > 0; i--) {
        char c = hex[(value >> ((i - 1u) * 4u)) & 0xFu];
        if (append_char(out, cap, n, c) != 0) {
            return -1;
        }
    }
    return 0;
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

int cirvane_obs_crash(char *out, unsigned cap, uint32_t reason, uint32_t mcause,
                      uint32_t mepc)
{
    unsigned n = 0;

    if (out == 0 || cap < 8u) {
        return -1;
    }
    out[0] = 0;
    if (append_str(out, cap, &n, "cirvane crash reason=") != 0 ||
        append_u32(out, cap, &n, reason) != 0 ||
        append_str(out, cap, &n, " mcause=") != 0 ||
        append_hex8(out, cap, &n, mcause) != 0 ||
        append_str(out, cap, &n, " mepc=") != 0 ||
        append_hex8(out, cap, &n, mepc) != 0) {
        return -1;
    }
    return (int)n;
}

int cirvane_obs_evidence(char *out, unsigned cap, uint8_t service,
                         const cirvane_evidence_t *ev)
{
    unsigned n = 0;
    cirvane_evidence_t empty;

    if (out == 0 || cap < 8u) {
        return -1;
    }
    if (ev == 0) {
        memzero(&empty, sizeof(empty));
        ev = &empty;
    }
    out[0] = 0;
    if (append_str(out, cap, &n, "cirvane evidence svc=") != 0 ||
        append_u32(out, cap, &n, service) != 0 ||
        append_str(out, cap, &n, " epoch=") != 0 ||
        append_u32(out, cap, &n, ev->epoch) != 0 ||
        append_str(out, cap, &n, " gen=") != 0 ||
        append_u32(out, cap, &n, ev->generation) != 0 ||
        append_str(out, cap, &n, " out=") != 0 ||
        append_u32(out, cap, &n, ev->outcome) != 0 ||
        append_str(out, cap, &n, " reason=") != 0 ||
        append_u32(out, cap, &n, ev->reason) != 0 ||
        append_str(out, cap, &n, " rec=") != 0 ||
        append_u32(out, cap, &n, ev->reclaimed) != 0 ||
        append_str(out, cap, &n, " health=") != 0 ||
        append_u32(out, cap, &n, ev->health_after) != 0) {
        return -1;
    }
    return (int)n;
}

int cirvane_obs_info(char *out, unsigned cap, uint32_t panic_reason)
{
    unsigned n = 0;

    if (out == 0 || cap < 8u) {
        return -1;
    }
    out[0] = 0;
    if (append_str(out, cap, &n, "cirvane info panic=") != 0 ||
        append_u32(out, cap, &n, panic_reason) != 0 ||
        append_str(out, cap, &n, " services=") != 0 ||
        append_u32(out, cap, &n, CIRVANE_RTX_SERVICE_COUNT) != 0 ||
        append_str(out, cap, &n, " slots=") != 0 ||
        append_u32(out, cap, &n, CIRVANE_RTX_SLOT_COUNT) != 0) {
        return -1;
    }
    return (int)n;
}

void cirvane_shell_init(cirvane_shell_t *sh)
{
    if (sh == 0) {
        return;
    }
    memzero(sh, sizeof(*sh));
}

int cirvane_shell_push(cirvane_shell_t *sh, uint8_t byte)
{
    if (sh == 0) {
        return 0;
    }
    if (byte == '\n' || byte == '\r') {
        if (sh->len < CIRVANE_SHELL_CMD_MAX) {
            sh->buf[sh->len] = 0;
        } else {
            sh->buf[CIRVANE_SHELL_CMD_MAX - 1u] = 0;
        }
        sh->ready = 1;
        return 1;
    }
    if (sh->overflow || sh->len >= (CIRVANE_SHELL_CMD_MAX - 1u)) {
        sh->overflow = 1;
        return 0;
    }
    sh->buf[sh->len] = (char)byte;
    sh->len += 1u;
    sh->buf[sh->len] = 0;
    return 0;
}

static int write_refuse(char *out, unsigned cap)
{
    unsigned n = 0;
    if (out == 0 || cap < 8u) {
        return -1;
    }
    out[0] = 0;
    if (append_str(out, cap, &n, "cirvane refuse") != 0) {
        return -1;
    }
    return (int)n;
}

int cirvane_shell_run(cirvane_shell_t *sh, const cirvane_kernel_t *k, char *out,
                      unsigned cap)
{
    const char *cmd;
    int wrote;

    if (sh == 0 || k == 0) {
        return write_refuse(out, cap);
    }
    cmd = sh->buf;
    if (sh->overflow) {
        wrote = write_refuse(out, cap);
        cirvane_shell_init(sh);
        return wrote;
    }
    if (cmd[0] == 0) {
        if (out != 0 && cap > 0) {
            out[0] = 0;
        }
        cirvane_shell_init(sh);
        return 0;
    }
    if (streq(cmd, "help")) {
        unsigned n = 0;
        if (out == 0 || cap < 8u) {
            cirvane_shell_init(sh);
            return -1;
        }
        out[0] = 0;
        wrote = append_str(out, cap, &n, "cirvane help help|info|crash|evidence") ==
                        0
                    ? (int)n
                    : -1;
    } else if (streq(cmd, "info")) {
        wrote = cirvane_obs_info(out, cap, cirvane_panic_reason(k));
    } else if (streq(cmd, "crash")) {
        wrote = cirvane_obs_crash(out, cap, k->panic_reason, k->panic_mcause,
                                  k->panic_mepc);
    } else if (cmd[0] == 'e' && cmd[1] == 'v' && cmd[2] == 'i' && cmd[3] == 'd' &&
               cmd[4] == 'e' && cmd[5] == 'n' && cmd[6] == 'c' && cmd[7] == 'e' &&
               cmd[8] == ' ' && cmd[9] >= '0' && cmd[9] <= '7' && cmd[10] == 0) {
        uint8_t svc = (uint8_t)(cmd[9] - '0');
        wrote = cirvane_obs_evidence(out, cap, svc,
                                     cirvane_rtx_evidence(&k->rtx, svc));
    } else {
        wrote = write_refuse(out, cap);
    }
    cirvane_shell_init(sh);
    return wrote;
}
