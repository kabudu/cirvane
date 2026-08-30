/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: Apache-2.0
 */

#include "obs.h"

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

static void feed_line(cirvane_shell_t *sh, const char *s)
{
    while (*s) {
        cirvane_shell_push(sh, (uint8_t)*s);
        s++;
    }
    cirvane_shell_push(sh, (uint8_t)'\n');
}

static void test_formats(void)
{
    char line[CIRVANE_OBS_LINE_MAX];
    cirvane_evidence_t ev;
    unsigned i;

    memset(&ev, 0, sizeof(ev));
    ev.epoch = 2;
    ev.generation = 1;
    ev.outcome = CIRVANE_RTX_RESTART;
    ev.reason = CIRVANE_RTX_REASON_FAULT;
    ev.reclaimed = 1;
    ev.health_after = CIRVANE_HEALTH_OK;
    expect(cirvane_obs_crash(line, sizeof(line), 1, 0xabcdu, 0x100u) > 0, "crash");
    expect(strstr(line, "cirvane crash reason=1") != 0, "crash reason");
    expect(strstr(line, "mcause=0000abcd") != 0, "mcause");
    expect(strlen(line) < CIRVANE_OBS_LINE_MAX, "crash bound");
    expect(cirvane_obs_evidence(line, sizeof(line), 1, &ev) > 0, "ev");
    expect(strstr(line, "cirvane evidence svc=1 epoch=2") != 0, "ev fields");
    expect(strstr(line, "health=0") != 0, "health");
    expect(cirvane_obs_info(line, sizeof(line), 0) > 0, "info");
    expect(strstr(line, "services=8") != 0, "svc count");
    expect(strstr(line, "slots=32") != 0, "slot count");
    for (i = 0; i < strlen(line); i++) {
        expect(line[i] != '\n', "single line");
    }
}

static void test_shell(void)
{
    cirvane_kernel_t k;
    cirvane_shell_t sh;
    char out[CIRVANE_OBS_LINE_MAX];
    unsigned i;

    cirvane_kernel_init(&k);
    cirvane_shell_init(&sh);
    feed_line(&sh, "help");
    expect(cirvane_shell_run(&sh, &k, out, sizeof(out)) > 0, "help");
    expect(strstr(out, "cirvane help") != 0, "help text");
    feed_line(&sh, "info");
    cirvane_shell_run(&sh, &k, out, sizeof(out));
    expect(strstr(out, "cirvane info panic=0") != 0, "info");
    expect(strstr(out, "heartbeat") == 0, "quiet");
    feed_line(&sh, "crash");
    cirvane_shell_run(&sh, &k, out, sizeof(out));
    expect(strstr(out, "cirvane crash reason=0") != 0, "crash cmd");
    feed_line(&sh, "evidence 0");
    cirvane_shell_run(&sh, &k, out, sizeof(out));
    expect(strstr(out, "cirvane evidence svc=0") != 0, "evidence");
    feed_line(&sh, "evidence 9");
    cirvane_shell_run(&sh, &k, out, sizeof(out));
    expect(strstr(out, "cirvane refuse") != 0, "bad svc");
    feed_line(&sh, "nosuch");
    cirvane_shell_run(&sh, &k, out, sizeof(out));
    expect(strstr(out, "cirvane refuse") != 0, "unknown");
    for (i = 0; i < CIRVANE_SHELL_CMD_MAX; i++) {
        cirvane_shell_push(&sh, (uint8_t)'x');
    }
    cirvane_shell_push(&sh, (uint8_t)'\n');
    cirvane_shell_run(&sh, &k, out, sizeof(out));
    expect(strstr(out, "cirvane refuse") != 0, "oversize");
}

int main(void)
{
    test_formats();
    test_shell();
    if (failures != 0) {
        fprintf(stderr, "%d obs checks failed\n", failures);
        return 1;
    }
    puts("kernel obs checks passed");
    return 0;
}
