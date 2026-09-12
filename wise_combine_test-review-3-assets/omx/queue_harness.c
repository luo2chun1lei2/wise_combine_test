#define _POSIX_C_SOURCE 200809L

#include "wct.h"
#include "../common/queue_sut.h"
#include "/home/workspace_data/works/myprojects/wise_combine_test.omx/src/wct_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_text(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int invoke(const char *id)
{
    if (!strcmp(id, "q_open") || !strcmp(id, "q_reopen")) return q_open();
    if (!strcmp(id, "q_push1")) return q_push1();
    if (!strcmp(id, "q_push1a") || !strcmp(id, "q_push1b")) return q_push1();
    if (!strcmp(id, "q_push2")) return q_push2();
    if (!strcmp(id, "q_pop") || !strcmp(id, "q_pop1") || !strcmp(id, "q_pop2")) return q_pop();
    if (!strcmp(id, "q_peek")) return q_peek();
    if (!strcmp(id, "q_size")) return q_size();
    if (!strcmp(id, "q_close")) return q_close();
    return -999;
}

static int callback(const char *id, const char *const *args, size_t argc,
                    char **result, void *ctx)
{
    (void)args;
    (void)argc;
    (void)ctx;
    char value[32];
    snprintf(value, sizeof value, "%d", invoke(id));
    *result = dup_text(value);
    return *result ? 0 : -1;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char error[256] = {0};
    if (wct_parse_file(argv[1], &state, &relation, error, sizeof error)) {
        fprintf(stderr, "parse_error=%s\n", error);
        return 3;
    }
    wct_report report = {0};
    int rc = wct_run_relation_in_process(&relation, callback, NULL,
                                         (wct_limits){.max_steps = 8}, &report);
    printf("rc=%d steps=%zu failures=%zu uncovered=%zu error=%s\n", rc,
           report.steps, report.failures, report.uncovered,
           report.error ? report.error : "");
    int ok = rc == 0 && report.failures == 0 && report.uncovered == 0;
    wct_report_free(&report);
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
    return ok ? 0 : 1;
}
