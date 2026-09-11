#include "wct.h"
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string(const char *value) {
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, value, size);
    return copy;
}

static int contains_whitespace(const char *value) {
    if (!value) return 0;
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p)
        if (isspace(*p)) return 1;
    return 0;
}

static void usage(FILE *out) {
    fprintf(out, "Usage: wise-combine-test --model FILE [--mode state|relation]\n");
    fprintf(out, "       [--seed N] [--max-steps N] [--max-flows N]\n");
    fprintf(out, "       [--isolate] [--timeout-ms N]\n");
    fprintf(out, "       [--trace FILE] | --replay FILE\n");
    fprintf(out, "       wise-combine-test --help | --version\n");
}

typedef struct {
    FILE *trace;
    uint64_t hash;
    size_t step;
    int quiet;
    const wct_state_graph *state_graph;
    int state_current;
    const char *current_edge_id;
} cli_context;

static void hash_bytes(cli_context *context, const char *text) {
    if (!context) return;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        context->hash ^= *p;
        context->hash *= UINT64_C(1099511628211);
    }
}

static uint64_t hash_file(const char *path) {
    unsigned char buffer[4096];
    uint64_t hash = UINT64_C(1469598103934665603);
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, file)) != 0) {
        for (size_t i = 0; i < count; ++i) {
            hash ^= buffer[i];
            hash *= UINT64_C(1099511628211);
        }
    }
    if (ferror(file)) hash = 0;
    fclose(file);
    return hash;
}

static void trace_record(cli_context *context, const char *line) {
    if (!context) return;
    hash_bytes(context, line);
    if (context->trace) fprintf(context->trace, "%s\n", line);
}

static uint64_t trace_seed(void);
static uint64_t hash_text(const char *text) { cli_context c = {.hash = trace_seed()}; hash_bytes(&c, text ? text : ""); return c.hash; }

static int state_callback(const char *input, char **actual, void *ctx) {
    cli_context *context = ctx;
    *actual = input ? copy_string(input) : NULL;
    if (*actual && context) {
        char line[2048];
        context->step++;
        const char *edge_id = context->current_edge_id ? context->current_edge_id : "?";
        snprintf(line, sizeof line, "step %zu state %s %s edge %s edge_hash %016" PRIx64, context->step,
                 input ? input : "-", *actual, edge_id, hash_text(edge_id));
        trace_record(context, line);
    }
    return input && !*actual;
}

static int relation_callback(const char *id, const char *const *args, size_t argc,
                             char **result, void *ctx) {
    cli_context *context = ctx;
    if (!context || !context->quiet) {
        printf("call %s", id);
        for (size_t i = 0; i < argc; ++i) printf(" %s", args[i] ? args[i] : "-");
        putchar('\n');
    }
    *result = copy_string(id);
    if (*result && context) {
        char line[4096];
        int used = snprintf(line, sizeof line, "step %zu call %s %zu", ++context->step, id, argc);
        for (size_t i = 0; i < argc && used > 0 && (size_t)used < sizeof line; ++i)
            used += snprintf(line + used, sizeof line - (size_t)used, " %s", args[i] ? args[i] : "-");
        if (used > 0 && (size_t)used < sizeof line)
            snprintf(line + used, sizeof line - (size_t)used, " result %s call_hash %016" PRIx64, *result, hash_text(id));
        trace_record(context, line);
    }
    return !*result;
}

static uint64_t trace_seed(void) { return UINT64_C(1469598103934665603); }
static int state_reset(void *ctx) { (void)ctx; return 0; }
static int state_snapshot(void *ctx, void **snapshot, size_t *size) {
    (void)ctx; *snapshot = NULL; *size = 0; return 0;
}
static int state_restore(void *ctx, const void *snapshot, size_t size) {
    (void)ctx; (void)snapshot; return size == 0 ? 0 : -1;
}
static void state_observer(const char *id, void *ctx) { cli_context *c = ctx; if (c) c->current_edge_id = id; }

static void hash_field(cli_context *c, const char *s) {
    uint64_t n = s ? strlen(s) : 0;
    for (unsigned i = 0; i < 8; ++i) { c->hash ^= (unsigned char)(n >> (i * 8)); c->hash *= UINT64_C(1099511628211); }
    if (s) hash_bytes(c, s);
}
static void hash_num(cli_context *c, uint64_t n) { char b[32]; snprintf(b,sizeof b,"%" PRIu64,n); hash_field(c,b); }

static uint64_t canonical_ir_digest(const wct_state_graph *state, const wct_relation_graph *relation, const char *mode) {
    cli_context c = {.hash = trace_seed()};
    hash_field(&c, mode);
    if (!strcmp(mode, "state") && state) {
        hash_field(&c, state->id); hash_field(&c, state->initial); hash_num(&c, state->state_count);
        for (size_t i = 0; i < state->state_count; ++i) hash_field(&c, state->states[i]);
        hash_num(&c, state->transition_count);
        for (size_t i = 0; i < state->transition_count; ++i) { const wct_transition *t=&state->transitions[i]; hash_field(&c,t->id); hash_field(&c,t->from); hash_field(&c,t->to); hash_field(&c,t->input); hash_field(&c,t->expect); }
    } else if (relation) {
        hash_field(&c, relation->id); hash_num(&c, relation->call_count);
        for (size_t i = 0; i < relation->call_count; ++i) { const wct_call *x=&relation->calls[i]; hash_field(&c,x->id); hash_num(&c,x->argc); for(size_t j=0;j<x->argc;++j) hash_field(&c,x->args[j]); hash_num(&c,x->expected_argc); hash_num(&c,x->arg_type_count); hash_num(&c,(uint64_t)x->contract_set); for(size_t j=0;j<x->arg_type_count;++j) hash_num(&c,(uint64_t)(int)x->arg_types[j]); }
        hash_num(&c, relation->relation_count); for (size_t i = 0; i < relation->relation_count; ++i) { hash_field(&c,relation->relations[i].from); hash_field(&c,relation->relations[i].to); }
    }
    return c.hash;
}

static uint64_t canonical_metadata_digest(const wct_state_graph *state, const wct_relation_graph *relation, const char *mode, unsigned seed) {
    cli_context c = {.hash = trace_seed()};
    char line[4096];
    snprintf(line, sizeof line, "selection %s", seed ? "seeded-xorshift" : "lexical-id-order");
    hash_bytes(&c, line);
    if (!strcmp(mode, "state") && state) {
        for (size_t i = 0; i < state->transition_count; ++i) {
            snprintf(line, sizeof line, "edge %s %s %s", state->transitions[i].id, state->transitions[i].from, state->transitions[i].to);
            hash_bytes(&c, line);
        }
    } else if (relation) {
        for (size_t i = 0; i < relation->relation_count; ++i) {
            snprintf(line, sizeof line, "edge %s->%s", relation->relations[i].from, relation->relations[i].to);
            hash_bytes(&c, line);
        }
    }
    return c.hash;
}

static int write_trace_header(FILE *trace, const char *model, const char *mode,
                              wct_limits limits, uint64_t ir_digest) {
    uint64_t model_digest = hash_file(model);
    if (!model_digest) return -1;
    return fprintf(trace, "WCT_TRACE 1\nmodel %s\nmodel_digest %016" PRIx64
                   "\nir_digest %016" PRIx64 "\nmode %s\nseed %u\nmax_steps %zu\nmax_flows %zu\n",
                   model, model_digest, ir_digest, mode, limits.seed, limits.max_steps,
                   limits.max_flows) < 0;
}

static int parse_trace(const char *path, char *model, size_t model_len, char *mode,
                       size_t mode_len, wct_limits *limits, size_t *steps,
                       int *exit_code, uint64_t *digest, uint64_t *model_digest, uint64_t *ir_digest, uint64_t *metadata_digest, size_t *declared_edges, size_t *covered_edges, size_t *uncovered_edges, int *process_exit, int *process_signal, int *timed_out, char *selection, size_t selection_len) {
    (void)model_len;
    (void)mode_len;
    (void)selection_len;
    FILE *file = fopen(path, "r");
    char line[4096], key[64], value[2048];
    int version = 0, got_model = 0, got_mode = 0, got_digest = 0, got_model_digest = 0, got_ir = 0, got_selection = 0, got_report = 0, got_seed = 0, got_max_steps = 0, got_max_flows = 0, got_steps = 0, got_exit = 0;
    cli_context stored = {.hash = trace_seed()};
    cli_context metadata = {.hash = trace_seed()};
    int got_metadata_digest = 0;
    if (!file) return -1;
    while (fgets(line, sizeof line, file)) {
        if (sscanf(line, "WCT_TRACE %d", &version) == 1) continue;
        if (!strncmp(line, "model_digest ", 13) &&
            sscanf(line + 13, "%" SCNx64, model_digest) == 1) {
            got_model_digest = 1; continue;
        }
        if (!strncmp(line, "ir_digest ", 10) && sscanf(line + 10, "%" SCNx64, ir_digest) == 1) { got_ir = 1; continue; }
        if (!strncmp(line, "metadata_digest ", 16) && sscanf(line + 16, "%" SCNx64, metadata_digest) == 1) { got_metadata_digest = 1; continue; }
        if (!strncmp(line, "selection ", 10) && sscanf(line + 10, "%63s", selection) == 1) { line[strcspn(line, "\r\n")] = 0; hash_bytes(&metadata, line); got_selection = 1; continue; }
        if (!strncmp(line, "edge ", 5)) { line[strcspn(line, "\r\n")] = 0; hash_bytes(&metadata, line); continue; }
        if (!strncmp(line, "model ", 6) && sscanf(line + 6, "%2047s", model) == 1) {
            got_model = 1; continue;
        }
        if (sscanf(line, "mode %63s", mode) == 1) { got_mode = 1; continue; }
        if (sscanf(line, "seed %u", &limits->seed) == 1) { if (got_seed++) return -1; continue; }
        if (sscanf(line, "max_steps %zu", &limits->max_steps) == 1) { if (got_max_steps++) return -1; continue; }
        if (sscanf(line, "max_flows %zu", &limits->max_flows) == 1) { if (got_max_flows++) return -1; continue; }
        if (sscanf(line, "steps %zu", steps) == 1) { if (got_steps++) return -1; continue; }
        if (sscanf(line, "declared_edges %zu", declared_edges) == 1) { got_report |= 1; continue; }
        if (sscanf(line, "covered_edges %zu", covered_edges) == 1) { got_report |= 2; continue; }
        if (sscanf(line, "uncovered_edges %zu", uncovered_edges) == 1) { got_report |= 4; continue; }
        if (sscanf(line, "process_exit %d", process_exit) == 1) { got_report |= 8; continue; }
        if (sscanf(line, "process_signal %d", process_signal) == 1) { got_report |= 16; continue; }
        if (sscanf(line, "timed_out %d", timed_out) == 1) { got_report |= 32; continue; }
        if (sscanf(line, "exit %d", exit_code) == 1) { if (got_exit++) return -1; continue; }
        if (sscanf(line, "digest %" SCNx64, digest) == 1) { got_digest = 1; continue; }
        if (sscanf(line, "%63s %2047s", key, value) == 2 && !strcmp(key, "step")) {
            line[strcspn(line, "\r\n")] = '\0';
            trace_record(&stored, line);
            continue;
        }
        return -1;
    }
    fclose(file);
    if (!got_model || !got_mode || !got_digest || !got_model_digest || got_report != 63 || !got_seed || !got_max_steps || !got_max_flows || !got_steps || !got_exit ||
        version != 1 || !*model || !*mode || !got_ir || !got_selection || !got_metadata_digest || stored.hash != *digest || metadata.hash != *metadata_digest)
        return -1;
    return 0;
}

static int replay_trace(const char *path) {
    char model[2048] = {0}, mode[64] = {0};
    size_t expected_steps = 0;
    int expected_exit = 0;
    uint64_t expected_digest = 0, expected_model_digest = 0, expected_ir_digest = 0, expected_metadata_digest = 0;
    size_t expected_declared = 0, expected_covered = 0, expected_uncovered = 0;
    int expected_process_exit = 0, expected_process_signal = 0, expected_timed_out = 0;
    char selection[64] = {0};
    wct_limits limits = {0};
    if (parse_trace(path, model, sizeof model, mode, sizeof mode, &limits,
                    &expected_steps, &expected_exit, &expected_digest,
                    &expected_model_digest, &expected_ir_digest, &expected_metadata_digest, &expected_declared, &expected_covered, &expected_uncovered, &expected_process_exit, &expected_process_signal, &expected_timed_out, selection, sizeof selection)) {
        fprintf(stderr, "error: invalid trace\n");
        return 1;
    }
    limits.state_reset = state_reset;
    limits.state_snapshot = state_snapshot;
    limits.state_restore = state_restore;
    limits.transition_observer = state_observer;
    if (hash_file(model) != expected_model_digest) {
        fprintf(stderr, "error: trace model checksum mismatch\n");
        return 1;
    }
    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char error[256] = {0};
    if (wct_parse_file(model, &state, &relation, error, sizeof error)) {
        fprintf(stderr, "error: %s\n", error[0] ? error : "failed to parse trace model");
        return 1;
    }
    if (strcmp(selection, limits.seed ? "seeded-xorshift" : "lexical-id-order")) { fprintf(stderr, "error: trace selection mismatch\n"); wct_state_graph_free(&state); wct_relation_graph_free(&relation); return 1; }
    if (canonical_ir_digest(&state, &relation, mode) != expected_ir_digest) { fprintf(stderr, "error: trace IR checksum mismatch\n"); wct_state_graph_free(&state); wct_relation_graph_free(&relation); return 1; }
    if (canonical_metadata_digest(&state, &relation, mode, limits.seed) != expected_metadata_digest) { fprintf(stderr, "error: trace metadata checksum mismatch\n"); wct_state_graph_free(&state); wct_relation_graph_free(&relation); return 1; }
    cli_context context = {.hash = trace_seed(), .quiet = 1, .state_graph = &state, .state_current = 0};
    wct_report report = {0};
    int rc = !strcmp(mode, "state")
        ? wct_run_state(&state, state_callback, &context, limits, &report)
        : !strcmp(mode, "relation")
            ? wct_run_relation(&relation, relation_callback, &context, limits, &report)
            : -1;
    int ok = (rc ? 1 : 0) == expected_exit && report.steps == expected_steps &&
             context.hash == expected_digest && report.declared_edges == expected_declared && report.covered_edges == expected_covered && report.uncovered_edges == expected_uncovered && report.process_exit == expected_process_exit && report.process_signal == expected_process_signal && report.timed_out == expected_timed_out;
    printf("replay=%s steps=%zu digest=%016" PRIx64 "\n", ok ? "PASS" : "FAIL",
           report.steps, context.hash);
    if (!ok) fprintf(stderr, "error: trace mismatch\n");
    wct_report_free(&report);
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    const char *model = NULL, *mode = "state", *trace_path = NULL, *replay_path = NULL;
    wct_limits limits = {0};
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) { usage(stdout); return 0; }
        if (!strcmp(argv[i], "--version")) { puts("wise-combine-test 0.1.0 schema 1"); return 0; }
        if (!strcmp(argv[i], "--replay") && i + 1 < argc) { replay_path = argv[++i]; continue; }
        if (!strcmp(argv[i], "--trace") && i + 1 < argc) { trace_path = argv[++i]; continue; }
        if (!strcmp(argv[i], "--model") && i + 1 < argc) { model = argv[++i]; continue; }
        if (!strcmp(argv[i], "--mode") && i + 1 < argc) { mode = argv[++i]; continue; }
        if ((!strcmp(argv[i], "--seed") || !strcmp(argv[i], "--max-steps") ||
             !strcmp(argv[i], "--max-flows") || !strcmp(argv[i], "--timeout-ms")) && i + 1 < argc) {
            char *end = NULL;
            if (argv[i + 1][0] == '-') {
                fprintf(stderr, "error: numeric option requires a non-negative integer\n");
                return 2;
            }
            errno = 0;
            unsigned long value = strtoul(argv[++i], &end, 10);
            if (!*argv[i] || *end || errno == ERANGE || value > (unsigned long)SIZE_MAX ||
                (!strcmp(argv[i - 1], "--seed") && value > (unsigned long)UINT_MAX) ||
                (!strcmp(argv[i - 1], "--timeout-ms") && value > (unsigned long)INT_MAX)) {
                fprintf(stderr, "error: numeric option requires a non-negative integer\n");
                return 2;
            }
            if (!strcmp(argv[i - 1], "--seed")) limits.seed = (unsigned)value;
            else if (!strcmp(argv[i - 1], "--max-steps")) limits.max_steps = (size_t)value;
            else if (!strcmp(argv[i - 1], "--max-flows")) limits.max_flows = (size_t)value;
            else limits.timeout_ms = (unsigned)value;
            continue;
        }
        if (!strcmp(argv[i], "--isolate")) { limits.isolate = 1; continue; }
        fprintf(stderr, "error: unknown or incomplete option '%s'\n", argv[i]);
        usage(stderr);
        return 2;
    }
    if (replay_path) {
        if (model || trace_path) { fprintf(stderr, "error: --replay cannot be combined with --model/--trace\n"); return 2; }
        return replay_trace(replay_path);
    }
    if (!model) { fprintf(stderr, "error: --model is required\n"); usage(stderr); return 2; }
    if (strcmp(mode, "state") && strcmp(mode, "relation")) {
        fprintf(stderr, "error: --mode must be state or relation\n"); return 2;
    }
    limits.state_reset = state_reset;
    limits.state_snapshot = state_snapshot;
    limits.state_restore = state_restore;
    limits.transition_observer = state_observer;
    if (trace_path && limits.isolate) {
        fprintf(stderr, "error: --trace cannot be combined with --isolate; replay the isolated run separately\n");
        return 2;
    }
    /* The trace format stores the model path as a whitespace-delimited field.
       Reject such paths rather than emitting a trace that cannot be replayed. */
    if (trace_path && contains_whitespace(model)) {
        fprintf(stderr, "error: model paths containing whitespace are not supported with --trace\n");
        return 2;
    }

    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char err[256] = {0};
    if (wct_parse_file(model, &state, &relation, err, sizeof err)) {
        fprintf(stderr, "error: %s\n", err[0] ? err : "failed to parse model");
        return 1;
    }
    FILE *trace = NULL;
    cli_context context = {.hash = trace_seed(), .state_graph = &state, .state_current = 0};
    if (trace_path) {
        trace = fopen(trace_path, "w");
        if (!trace || write_trace_header(trace, model, mode, limits, canonical_ir_digest(&state, &relation, mode))) {
            if (trace) fclose(trace);
            fprintf(stderr, "error: cannot write trace\n");
            wct_state_graph_free(&state); wct_relation_graph_free(&relation);
            return 1;
        }
        context.trace = trace;
        cli_context metadata = {.hash = trace_seed()};
        char metadata_line[4096];
        snprintf(metadata_line, sizeof metadata_line, "selection %s", limits.seed ? "seeded-xorshift" : "lexical-id-order");
        fprintf(trace, "%s\n", metadata_line); hash_bytes(&metadata, metadata_line);
        if (!strcmp(mode, "state")) {
            for (size_t i = 0; i < state.transition_count; ++i) { snprintf(metadata_line, sizeof metadata_line, "edge %s %s %s", state.transitions[i].id, state.transitions[i].from, state.transitions[i].to); fprintf(trace, "%s\n", metadata_line); hash_bytes(&metadata, metadata_line); }
        } else {
            for (size_t i = 0; i < relation.relation_count; ++i) { snprintf(metadata_line, sizeof metadata_line, "edge %s->%s", relation.relations[i].from, relation.relations[i].to); fprintf(trace, "%s\n", metadata_line); hash_bytes(&metadata, metadata_line); }
        }
        fprintf(trace, "metadata_digest %016" PRIx64 "\n", metadata.hash);
    }
    wct_report report = {0};
    int rc;
    if (!strcmp(mode, "state")) {
        rc = wct_run_state(&state, state_callback, &context, limits, &report);
    } else {
        rc = wct_run_relation(&relation, relation_callback, &context, limits, &report);
    }
    printf("steps=%zu covered=%zu failures=%zu uncovered=%zu edges=%zu/%zu uncovered_edges=%zu seed=%u",
           report.steps, report.covered, report.failures, report.uncovered,
           report.covered_edges, report.declared_edges, report.uncovered_edges, report.seed);
    if (limits.isolate) printf(" process_exit=%d process_signal=%d timed_out=%d",
                               report.process_exit, report.process_signal, report.timed_out);
    putchar('\n');
    if (report.error) fprintf(stderr, "error: %s\n", report.error);
    if (report.scenario)
        fprintf(stderr, "scenario=%s step=%zu\n", report.scenario, report.failed_step);
    if (trace) {
        fprintf(trace, "steps %zu\ndeclared_edges %zu\ncovered_edges %zu\nuncovered_edges %zu\nexit %d\nprocess_exit %d\nprocess_signal %d\ntimed_out %d\ndigest %016" PRIx64 "\n", report.steps, report.declared_edges, report.covered_edges, report.uncovered_edges,
                rc ? 1 : 0, report.process_exit, report.process_signal, report.timed_out, context.hash);
        fclose(trace);
    }
    wct_report_free(&report);
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
    return rc ? 1 : 0;
}
