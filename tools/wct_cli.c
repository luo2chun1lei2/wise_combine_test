#include "wct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *value) {
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, value, size);
    return copy;
}

static void usage(FILE *out) {
    fprintf(out, "Usage: wise-combine-test --model FILE [--mode state|relation]\n");
    fprintf(out, "       [--seed N] [--max-steps N] [--max-flows N]\n");
    fprintf(out, "       wise-combine-test --help | --version\n");
}

static int state_callback(const char *input, char **actual, void *ctx) {
    (void)ctx;
    *actual = input ? copy_string(input) : NULL;
    return input && !*actual;
}

static int relation_callback(const char *id, const char *const *args, size_t argc,
                             char **result, void *ctx) {
    (void)ctx;
    printf("call %s", id);
    for (size_t i = 0; i < argc; ++i) printf(" %s", args[i]);
    putchar('\n');
    *result = copy_string(id);
    return !*result;
}

int main(int argc, char **argv) {
    const char *model = NULL, *mode = "state";
    wct_limits limits = {0};
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) { usage(stdout); return 0; }
        if (!strcmp(argv[i], "--version")) { puts("wise-combine-test 0.1.0 schema 1"); return 0; }
        if (!strcmp(argv[i], "--model") && i + 1 < argc) { model = argv[++i]; continue; }
        if (!strcmp(argv[i], "--mode") && i + 1 < argc) { mode = argv[++i]; continue; }
        if ((!strcmp(argv[i], "--seed") || !strcmp(argv[i], "--max-steps") ||
             !strcmp(argv[i], "--max-flows")) && i + 1 < argc) {
            char *end = NULL;
            unsigned long value = strtoul(argv[++i], &end, 10);
            if (!*argv[i] || *end || value > (unsigned long)SIZE_MAX) {
                fprintf(stderr, "error: numeric option requires a non-negative integer\n");
                return 2;
            }
            if (!strcmp(argv[i - 1], "--seed")) limits.seed = (unsigned)value;
            else if (!strcmp(argv[i - 1], "--max-steps")) limits.max_steps = (size_t)value;
            else limits.max_flows = (size_t)value;
            continue;
        }
        fprintf(stderr, "error: unknown or incomplete option '%s'\n", argv[i]);
        usage(stderr);
        return 2;
    }
    if (!model) { fprintf(stderr, "error: --model is required\n"); usage(stderr); return 2; }
    if (strcmp(mode, "state") && strcmp(mode, "relation")) {
        fprintf(stderr, "error: --mode must be state or relation\n"); return 2;
    }

    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char err[256] = {0};
    if (wct_parse_file(model, &state, &relation, err, sizeof err)) {
        fprintf(stderr, "error: %s\n", err[0] ? err : "failed to parse model");
        return 1;
    }
    wct_report report = {0};
    int rc;
    if (!strcmp(mode, "state")) {
        rc = wct_run_state(&state, state_callback, NULL, limits, &report);
    } else {
        rc = wct_run_relation(&relation, relation_callback, NULL, limits, &report);
    }
    printf("steps=%zu covered=%zu failures=%zu uncovered=%zu seed=%u\n",
           report.steps, report.covered, report.failures, report.uncovered, report.seed);
    if (report.error) fprintf(stderr, "error: %s\n", report.error);
    if (report.scenario)
        fprintf(stderr, "scenario=%s step=%zu\n", report.scenario, report.failed_step);
    wct_report_free(&report);
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
    return rc ? 1 : 0;
}
