#define _POSIX_C_SOURCE 200809L

#include "wct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

static int failures;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, (message)); \
            failures++; \
        } \
    } while (0)

static char *copy_string(const char *value)
{
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy != NULL)
        memcpy(copy, value, length);
    return copy;
}

typedef struct {
    const char *inputs[8];
    size_t count;
    int fail;
} state_context;

static int state_callback(const char *input, char **actual, void *opaque)
{
    state_context *context = opaque;
    if (context->count < sizeof context->inputs / sizeof context->inputs[0])
        context->inputs[context->count++] = input;
    if (context->fail)
        return -1;
    *actual = copy_string("ok");
    return *actual == NULL ? -1 : 0;
}

static int state_reset(void *opaque) { (void)opaque; return 0; }
static int snapshot_count(void *opaque, void **snapshot, size_t *size) {
    state_context *context = opaque;
    int *copy = malloc(sizeof *copy);
    if (!copy) return -1;
    *copy = (int)context->count; *snapshot = copy; *size = sizeof *copy; return 0;
}
static int restore_count(void *opaque, const void *snapshot, size_t size) {
    state_context *context = opaque;
    if (!snapshot || size != sizeof(int)) return -1;
    context->count = (size_t)*(const int *)snapshot; return 0;
}
static int zero_restore_calls;
static int snapshot_zero(void *opaque, void **snapshot, size_t *size) {
    (void)opaque; *snapshot = NULL; *size = 0; return 0;
}
static int restore_zero(void *opaque, const void *snapshot, size_t size) {
    (void)opaque; (void)snapshot;
    if (size != 0) return -1;
    zero_restore_calls++; return 0;
}
static int restore_fails(void *opaque, const void *snapshot, size_t size) {
    (void)opaque; (void)snapshot; (void)size; return -1;
}
static int state_callback_fail_second(const char *input, char **actual, void *opaque) {
    state_context *context = opaque;
    (void)input;
    context->count++;
    if (context->count == 2) return -1;
    *actual = copy_string("ok");
    return *actual ? 0 : -1;
}
static int state_callback_bad_result(const char *input, char **actual, void *opaque) {
    state_context *context = opaque; (void)input; context->count++;
    *actual = copy_string("wrong"); return *actual ? 0 : -1;
}

static void init_state_graph(wct_state_graph *graph)
{
    memset(graph, 0, sizeof *graph);
    graph->id = copy_string("session");
    graph->initial = copy_string("idle");
    graph->state_count = 3;
    graph->states = calloc(graph->state_count, sizeof *graph->states);
    graph->transition_count = 2;
    graph->transitions = calloc(graph->transition_count, sizeof *graph->transitions);
    graph->states[0] = copy_string("idle");
    graph->states[1] = copy_string("running");
    graph->states[2] = copy_string("done");
    graph->transitions[0] = (wct_transition){
        copy_string("start"), copy_string("idle"), copy_string("running"),
        copy_string("begin"), copy_string("ok")
    };
    graph->transitions[1] = (wct_transition){
        copy_string("finish"), copy_string("running"), copy_string("done"),
        copy_string("finish"), copy_string("ok")
    };
}

static void test_state_success_and_limit(void)
{
    wct_state_graph graph;
    wct_report report;
    state_context context = {0};
    init_state_graph(&graph);

    CHECK(wct_validate_state(&graph, NULL, 0) == 0, "valid state graph should validate");
    CHECK(wct_run_state(&graph, state_callback, &context,
                        (wct_limits){.max_steps = 2, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == 0,
          "state run should succeed when all edges fit the limit");
    CHECK(report.steps == 2 && report.covered == 2 && report.uncovered == 0 &&
              report.failures == 0,
          "state run should report complete coverage");
    CHECK(context.count == 2 && strcmp(context.inputs[0], "begin") == 0 &&
              strcmp(context.inputs[1], "finish") == 0,
          "state callback should receive transitions in graph order");
    wct_report_free(&report);
    wct_state_graph_free(&graph);

    init_state_graph(&graph);
    context = (state_context){0};
    CHECK(wct_run_state(&graph, state_callback, &context,
                        (wct_limits){.max_steps = 1, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == -1,
          "a bounded run with uncovered edges should be reported as incomplete");
    CHECK(report.steps == 1 && report.covered == 1 && report.uncovered == 1 &&
              report.failures == 0,
          "state limit should preserve bounded progress and uncovered count");
    wct_report_free(&report);
    wct_state_graph_free(&graph);
}

static void test_state_callback_failure(void)
{
    wct_state_graph graph;
    wct_report report;
    state_context context = {.fail = 1};
    init_state_graph(&graph);

    CHECK(wct_run_state(&graph, state_callback, &context, (wct_limits){.state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == -1,
          "callback failure should fail state run");
    CHECK(report.failures == 1 && report.steps == 0 && report.covered == 0,
          "failed transition must not count as completed or covered");
    CHECK(report.error != NULL, "failed transition should provide an error");
    wct_report_free(&report);
    wct_state_graph_free(&graph);
}

static void test_state_failure_atomic_snapshot(void)
{
    wct_state_graph graph; wct_report report; state_context context = {.fail = 1};
    init_state_graph(&graph);
    CHECK(wct_run_state(&graph, state_callback, &context,
                        (wct_limits){.state_snapshot = snapshot_count,
                                    .state_restore = restore_count}, &report) == -1,
          "snapshot-enabled callback failure should fail run");
    CHECK(context.count == 0, "failed callback must roll back opaque context");
    wct_report_free(&report); wct_state_graph_free(&graph);
}

static void test_isolated_state_snapshot_commit(void)
{
    wct_state_graph graph; wct_report report; state_context context = {0};
    init_state_graph(&graph);
    CHECK(wct_run_state(&graph, state_callback, &context,
                        (wct_limits){.isolate = 1, .timeout_ms = 100,
                                    .state_snapshot = snapshot_count,
                                    .state_restore = restore_count}, &report) == 0,
          "isolated state run with transaction hooks should succeed");
    CHECK(context.count == 2, "isolated transitions should commit state between callbacks");
    wct_report_free(&report); wct_state_graph_free(&graph);

    init_state_graph(&graph); context = (state_context){0};
    CHECK(wct_run_state(&graph, state_callback_fail_second, &context,
                        (wct_limits){.isolate = 1, .timeout_ms = 100,
                                    .state_snapshot = snapshot_count,
                                    .state_restore = restore_count}, &report) == -1,
          "isolated callback failure should fail run");
    CHECK(context.count == 1, "failed isolated transition must not commit child state");
    wct_report_free(&report); wct_state_graph_free(&graph);

    init_state_graph(&graph); context = (state_context){0};
    CHECK(wct_run_state(&graph, state_callback_bad_result, &context,
                        (wct_limits){.isolate = 1, .timeout_ms = 100,
                                    .state_snapshot = snapshot_count,
                                    .state_restore = restore_count}, &report) == -1,
          "isolated expectation mismatch should fail run");
    CHECK(context.count == 0, "expectation mismatch must not commit child state");
    wct_report_free(&report); wct_state_graph_free(&graph);
}

static void test_zero_snapshot_and_rollback_failure(void)
{
    wct_state_graph graph; wct_report report; state_context context = {0};
    init_state_graph(&graph); zero_restore_calls = 0;
    CHECK(wct_run_state(&graph, state_callback_bad_result, &context,
                        (wct_limits){.state_snapshot = snapshot_zero,
                                    .state_restore = restore_zero}, &report) == -1,
          "zero-length snapshot run should retain transactional behavior");
    CHECK(zero_restore_calls == 1, "zero-length successful snapshot must be restored");
    wct_report_free(&report); wct_state_graph_free(&graph);

    init_state_graph(&graph); context = (state_context){0};
    CHECK(wct_run_state(&graph, state_callback_bad_result, &context,
                        (wct_limits){.state_snapshot = snapshot_zero,
                                    .state_restore = restore_fails}, &report) == -1,
          "restore failure should fail the run");
    CHECK(report.error && strstr(report.error, "rollback failed"),
          "restore failure during rollback should be explicit");
    wct_report_free(&report); wct_state_graph_free(&graph);
}

static int state_callback_slow(const char *input, char **actual, void *opaque)
{
    (void)opaque;
    if (input && !strcmp(input, "begin")) { struct timespec ts = {.tv_nsec = 50000000}; nanosleep(&ts, NULL); }
    *actual = copy_string("ok");
    return *actual == NULL ? -1 : 0;
}

static void test_isolation_timeout(void)
{
    wct_state_graph graph;
    wct_report report;
    init_state_graph(&graph);
    CHECK(wct_run_state(&graph, state_callback_slow, NULL,
                        (wct_limits){.isolate = 1, .timeout_ms = 5, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == -1,
          "isolated callback timeout should fail run");
    CHECK(report.error && strstr(report.error, "timeout") != NULL,
          "isolated timeout should be reported");
    CHECK(report.timed_out == 1 && report.process_signal != 0,
          "isolated timeout should retain process termination metadata");
    wct_report_free(&report);
    wct_state_graph_free(&graph);
}

static void test_timeout_range_and_zero_arity_contract(void)
{
    wct_state_graph state;
    wct_report report;
    init_state_graph(&state);
    CHECK(wct_run_state(&state, state_callback, NULL,
                        (wct_limits){.isolate = 1, .timeout_ms = UINT_MAX, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == -1,
          "timeouts beyond poll's signed range should be rejected");
    CHECK(report.error && strstr(report.error, "timeout exceeds poll limit") != NULL,
          "out-of-range timeout should provide a precise error");
    wct_report_free(&report);
    wct_state_graph_free(&state);

    wct_relation_graph graph = {0};
    char error[128] = {0};
    graph.call_count = 1;
    graph.calls = calloc(1, sizeof *graph.calls);
    graph.calls[0].id = copy_string("zero");
    graph.calls[0].argc = 1;
    graph.calls[0].args = calloc(1, sizeof *graph.calls[0].args);
    graph.calls[0].args[0] = copy_string("extra");
    graph.calls[0].contract_set = 1;
    graph.calls[0].expected_argc = 0;
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1 &&
              strstr(error, "arity mismatch") != NULL,
          "explicit zero-arity contract should reject an extra argument");
    wct_relation_graph_free(&graph);
}

static void test_bare_result_reference_rejected(void)
{
    wct_relation_graph graph = {0};
    char error[128] = {0};
    graph.call_count = 1;
    graph.calls = calloc(1, sizeof *graph.calls);
    graph.calls[0].id = copy_string("consumer");
    graph.calls[0].argc = 1;
    graph.calls[0].args = calloc(1, sizeof(char *));
    graph.calls[0].args[0] = copy_string("$");
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1,
          "bare result reference should be rejected");
    wct_relation_graph_free(&graph);
}

static void test_state_branching_coverage(void)
{
    wct_state_graph graph = {0};
    wct_report report;
    state_context context = {0};
    graph.id = copy_string("branch");
    graph.initial = copy_string("idle");
    graph.state_count = 3;
    graph.states = calloc(graph.state_count, sizeof *graph.states);
    graph.states[0] = copy_string("idle");
    graph.states[1] = copy_string("left");
    graph.states[2] = copy_string("right");
    graph.transition_count = 2;
    graph.transitions = calloc(graph.transition_count, sizeof *graph.transitions);
    graph.transitions[0] = (wct_transition){copy_string("z-right"), copy_string("idle"),
        copy_string("right"), copy_string("r"), copy_string("ok")};
    graph.transitions[1] = (wct_transition){copy_string("a-left"), copy_string("idle"),
        copy_string("left"), copy_string("l"), copy_string("ok")};
    CHECK(wct_run_state(&graph, state_callback, &context,
                        (wct_limits){.max_steps = 2, .seed = 7, .state_reset = state_reset, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == 0,
          "branching state graph should cover both reachable edges");
    CHECK(report.covered == 2 && report.uncovered == 0 && report.seed == 7,
          "branching state graph should report complete coverage and seed");
    wct_report_free(&report);
    wct_state_graph_free(&graph);
}

static void test_seed_sampling_determinism(void)
{
    wct_state_graph graph = {0};
    wct_report a = {0}, b = {0}, c = {0};
    state_context ca = {0}, cb = {0}, cc = {0};
    graph.id = copy_string("seed"); graph.initial = copy_string("idle");
    graph.state_count = 2; graph.states = calloc(2, sizeof *graph.states);
    graph.states[0] = copy_string("idle"); graph.states[1] = copy_string("done");
    graph.transition_count = 3; graph.transitions = calloc(3, sizeof *graph.transitions);
    const char *ids[] = {"a", "b", "c"};
    for (size_t i = 0; i < 3; ++i) {
        graph.transitions[i] = (wct_transition){copy_string(ids[i]), copy_string("idle"),
            copy_string("done"), copy_string(ids[i]), copy_string("ok")};
    }
    CHECK(wct_run_state(&graph, state_callback, &ca,
                        (wct_limits){.max_steps = 3, .seed = 17, .state_reset = state_reset, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &a) == 0,
          "seeded state sampling should complete");
    CHECK(wct_run_state(&graph, state_callback, &cb,
                        (wct_limits){.max_steps = 3, .seed = 17, .state_reset = state_reset, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &b) == 0,
          "repeated seeded state sampling should complete");
    CHECK(ca.count == cb.count && ca.count == 3 &&
              memcmp(ca.inputs, cb.inputs, ca.count * sizeof(ca.inputs[0])) == 0,
          "same seed should produce identical transition order");
    CHECK(a.declared_edges == 3 && a.covered_edges == 3 && a.uncovered_edges == 0,
          "state report should expose declared and covered edge counts");
    CHECK(wct_run_state(&graph, state_callback, &cc,
                        (wct_limits){.max_steps = 3, .seed = 48, .state_reset = state_reset, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &c) == 0,
          "alternate seeded state sampling should complete");
    CHECK(memcmp(ca.inputs, cc.inputs, ca.count * sizeof(ca.inputs[0])) != 0,
          "different seeds should alter sampling order for branching graph");
    wct_report_free(&a); wct_report_free(&b); wct_report_free(&c);
    wct_state_graph_free(&graph);
}

static void test_state_validation_errors(void)
{
    wct_state_graph graph;
    char error[64] = {0};
    init_state_graph(&graph);
    free(graph.transitions[1].to);
    graph.transitions[1].to = copy_string("missing");
    CHECK(wct_validate_state(&graph, error, sizeof error) == -1,
          "unknown transition endpoint should be rejected");
    CHECK(strstr(error, "unknown state") != NULL,
          "unknown endpoint error should identify the state problem");
    wct_state_graph_free(&graph);

    init_state_graph(&graph);
    free(graph.transitions[1].id);
    graph.transitions[1].id = copy_string("start");
    memset(error, 0, sizeof error);
    CHECK(wct_validate_state(&graph, error, sizeof error) == -1,
          "duplicate transition ID should be rejected");
    CHECK(strstr(error, "duplicate transition") != NULL,
          "duplicate transition error should be diagnosable");
    wct_state_graph_free(&graph);
}

static void init_branching_state_graph(wct_state_graph *graph)
{
    memset(graph, 0, sizeof *graph);
    graph->id = copy_string("branching");
    graph->initial = copy_string("idle");
    graph->state_count = 5;
    graph->states = calloc(graph->state_count, sizeof *graph->states);
    graph->states[0] = copy_string("idle");
    graph->states[1] = copy_string("left");
    graph->states[2] = copy_string("right");
    graph->states[3] = copy_string("done");
    graph->states[4] = copy_string("unreachable");
    graph->transition_count = 5;
    graph->transitions = calloc(graph->transition_count, sizeof *graph->transitions);
    graph->transitions[0] = (wct_transition){
        copy_string("to-left"), copy_string("idle"), copy_string("left"),
        copy_string("left"), copy_string("ok")
    };
    graph->transitions[1] = (wct_transition){
        copy_string("to-right"), copy_string("idle"), copy_string("right"),
        copy_string("right"), copy_string("ok")
    };
    graph->transitions[2] = (wct_transition){
        copy_string("left-done"), copy_string("left"), copy_string("done"),
        copy_string("left-done"), copy_string("ok")
    };
    graph->transitions[3] = (wct_transition){
        copy_string("right-done"), copy_string("right"), copy_string("done"),
        copy_string("right-done"), copy_string("ok")
    };
    graph->transitions[4] = (wct_transition){
        copy_string("never"), copy_string("unreachable"), copy_string("done"),
        copy_string("never"), copy_string("ok")
    };
}

static void test_state_branching_and_unreachable(void)
{
    wct_state_graph graph;
    wct_report report;
    state_context context = {0};
    init_branching_state_graph(&graph);

    CHECK(wct_validate_state(&graph, NULL, 0) == 0,
          "branching state graph should validate before execution");
    CHECK(wct_run_state(&graph, state_callback, &context,
                        (wct_limits){.max_steps = 16, .seed = 7, .state_reset = state_reset, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == -1,
          "unreachable declared edge should make the run incomplete");
    CHECK(report.covered == 4 && report.uncovered == 1 && report.failures == 0,
          "state execution should cover both reachable branches and report one unreachable edge");
    wct_report_free(&report);
    wct_state_graph_free(&graph);
}

static void test_parser_diagnostics(void)
{
    const char *path = "/tmp/wct-malformed.model";
    FILE *file = fopen(path, "w");
    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char error[128] = {0};
    CHECK(file != NULL, "malformed fixture should be writable");
    if (file == NULL)
        return;
    fputs("schema 1\nstate_graph bad idle\nstate idle\ntransition only two\n", file);
    fclose(file);
    CHECK(wct_parse_file(path, &state, &relation, error, sizeof error) == -1,
          "malformed transition should be rejected");
    CHECK(strstr(error, "line 4 column 1") != NULL,
          "malformed transition diagnostic should include line and column");
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
    remove(path);
}

static void test_empty_ids_rejected(void)
{
    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char error[64] = {0};
    state.initial = copy_string("");
    state.state_count = 1;
    state.states = calloc(1, sizeof *state.states);
    state.states[0] = copy_string("idle");
    CHECK(wct_validate_state(&state, error, sizeof error) == -1,
          "empty initial state should be rejected");
    wct_state_graph_free(&state);

    relation.call_count = 1;
    relation.calls = calloc(1, sizeof *relation.calls);
    relation.calls[0].id = copy_string("");
    memset(error, 0, sizeof error);
    CHECK(wct_validate_relation(&relation, error, sizeof error) == -1,
          "empty call ID should be rejected");
    wct_relation_graph_free(&relation);
}

typedef struct {
    const char *ids[8];
    size_t count;
    const char *fail_id;
} relation_context;

static int relation_callback(const char *id, const char *const *args, size_t argc,
                             char **result, void *opaque)
{
    relation_context *context = opaque;
    if (context->count < sizeof context->ids / sizeof context->ids[0])
        context->ids[context->count++] = id;
    if (strcmp(id, "fetch") == 0)
        CHECK(argc == 1 && strcmp(args[0], "url") == 0, "fetch arguments should be preserved");
    if (strcmp(id, "transform") == 0)
        CHECK(argc == 1 && strcmp(args[0], "json") == 0, "transform arguments should be preserved");
    if (strcmp(id, "store") == 0)
        CHECK(argc == 1 && strcmp(args[0], "db") == 0, "store arguments should be preserved");
    if (strcmp(id, "consume") == 0)
        CHECK(argc == 1 && strcmp(args[0], "produce-result") == 0,
              "prior call result should bind through a $ reference");
    if (context->fail_id != NULL && strcmp(id, context->fail_id) == 0)
        return -1;
    *result = copy_string(strcmp(id, "produce") == 0 ? "produce-result" : id);
    return *result == NULL ? -1 : 0;
}

static void test_relation_result_binding(void)
{
    wct_relation_graph graph = {0};
    wct_report report;
    relation_context context = {0};
    graph.id = copy_string("binding");
    graph.call_count = 2;
    graph.calls = calloc(graph.call_count, sizeof *graph.calls);
    graph.calls[0].id = copy_string("produce");
    graph.calls[0].argc = 0;
    graph.calls[1].id = copy_string("consume");
    graph.calls[1].argc = 1;
    graph.calls[1].args = calloc(1, sizeof(char *));
    graph.calls[1].args[0] = copy_string("$produce");
    graph.relation_count = 1;
    graph.relations = calloc(1, sizeof *graph.relations);
    graph.relations[0] = (wct_relation){copy_string("produce"), copy_string("consume")};
    CHECK(wct_run_relation(&graph, relation_callback, &context,
                           (wct_limits){.seed = 11, .state_snapshot = snapshot_zero, .state_restore = restore_zero}, &report) == 0,
          "relation result binding should execute successfully");
    CHECK(report.steps == 2 && report.uncovered == 0 && report.seed == 11,
          "relation result binding should report complete deterministic flow");
    wct_report_free(&report);
    wct_relation_graph_free(&graph);
}

static void init_relation_graph(wct_relation_graph *graph)
{
    memset(graph, 0, sizeof *graph);
    graph->id = copy_string("pipeline");
    graph->call_count = 3;
    graph->calls = calloc(graph->call_count, sizeof *graph->calls);
    graph->calls[0] = (wct_call){.id = copy_string("fetch"), .args = calloc(1, sizeof(char *)), .argc = 1};
    graph->calls[1] = (wct_call){.id = copy_string("transform"), .args = calloc(1, sizeof(char *)), .argc = 1};
    graph->calls[2] = (wct_call){.id = copy_string("store"), .args = calloc(1, sizeof(char *)), .argc = 1};
    graph->calls[0].args[0] = copy_string("url");
    graph->calls[1].args[0] = copy_string("json");
    graph->calls[2].args[0] = copy_string("db");
    graph->relation_count = 2;
    graph->relations = calloc(graph->relation_count, sizeof *graph->relations);
    graph->relations[0] = (wct_relation){copy_string("fetch"), copy_string("transform")};
    graph->relations[1] = (wct_relation){copy_string("transform"), copy_string("store")};
}

static void test_relation_order_and_failure(void)
{
    wct_relation_graph graph;
    wct_report report;
    relation_context context = {0};
    init_relation_graph(&graph);

    CHECK(wct_validate_relation(&graph, NULL, 0) == 0, "valid relation graph should validate");
    CHECK(wct_run_relation(&graph, relation_callback, &context,
                           (wct_limits){.max_flows = 2}, &report) == -1,
          "relation run should report incomplete coverage under flow limit");
    CHECK(report.steps == 2 && report.covered == 2 && report.uncovered == 1,
          "relation flow limit should bound calls and expose incomplete coverage");
    CHECK(context.count == 2 && strcmp(context.ids[0], "fetch") == 0 &&
              strcmp(context.ids[1], "transform") == 0,
          "relation calls should follow prerequisite order");
    wct_report_free(&report);
    wct_relation_graph_free(&graph);

    init_relation_graph(&graph);
    context = (relation_context){.fail_id = "transform"};
    CHECK(wct_run_relation(&graph, relation_callback, &context, (wct_limits){0}, &report) == -1,
          "relation callback failure should fail run");
    CHECK(report.failures == 1 && report.steps == 1,
          "relation failure should preserve completed step count");
    wct_report_free(&report);
    wct_relation_graph_free(&graph);
}

static void test_relation_cycle(void)
{
    wct_relation_graph graph;
    char error[64] = {0};
    init_relation_graph(&graph);
    free(graph.relations[1].to);
    graph.relations[1].to = copy_string("fetch");
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1,
          "relation cycle should be rejected");
    CHECK(strstr(error, "cycle") != NULL, "cycle rejection should be diagnosable");
    wct_relation_graph_free(&graph);

    init_relation_graph(&graph);
    free(graph.relations[0].to);
    graph.relations[0].to = copy_string("missing");
    memset(error, 0, sizeof error);
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1,
          "unknown relation endpoint should be rejected");
    CHECK(strstr(error, "unknown call") != NULL,
          "unknown endpoint error should identify the call problem");
    wct_relation_graph_free(&graph);
}

static void test_relation_lexical_tie_break(void)
{
    wct_relation_graph graph = {0};
    wct_report report;
    relation_context context = {0};
    graph.id = copy_string("tie-break");
    graph.call_count = 3;
    graph.calls = calloc(graph.call_count, sizeof *graph.calls);
    graph.calls[0].id = copy_string("zeta");
    graph.calls[1].id = copy_string("alpha");
    graph.calls[2].id = copy_string("middle");

    CHECK(wct_validate_relation(&graph, NULL, 0) == 0,
          "independent calls should validate");
    CHECK(wct_run_relation(&graph, relation_callback, &context,
                           (wct_limits){.max_flows = 3, .seed = 0}, &report) == 0,
          "independent calls should execute");
    CHECK(context.count == 3 && strcmp(context.ids[0], "alpha") == 0 &&
              strcmp(context.ids[1], "middle") == 0 &&
              strcmp(context.ids[2], "zeta") == 0,
          "independent relation calls should use stable lexical ID order");
    wct_report_free(&report);
    wct_relation_graph_free(&graph);
}

static void test_relation_reference_dependency(void)
{
    wct_relation_graph graph = {0};
    wct_report report;
    relation_context context = {0};
    graph.id = copy_string("implicit-dependency");
    graph.call_count = 2;
    graph.calls = calloc(graph.call_count, sizeof *graph.calls);
    graph.calls[0].id = copy_string("consume");
    graph.calls[0].argc = 1;
    graph.calls[0].args = calloc(1, sizeof(char *));
    graph.calls[0].args[0] = copy_string("$produce");
    graph.calls[1].id = copy_string("produce");

    CHECK(wct_validate_relation(&graph, NULL, 0) == 0,
          "result reference should define a valid implicit dependency");
    CHECK(wct_run_relation(&graph, relation_callback, &context,
                           (wct_limits){0}, &report) == 0,
          "implicit result dependency should execute successfully");
    CHECK(context.count == 2 && strcmp(context.ids[0], "produce") == 0 &&
              strcmp(context.ids[1], "consume") == 0,
          "producer should run before its result consumer without a redundant edge");
    wct_report_free(&report);

    free(graph.calls[1].id);
    graph.calls[1].id = copy_string("produce");
    graph.calls[1].argc = 1;
    graph.calls[1].args = calloc(1, sizeof(char *));
    graph.calls[1].args[0] = copy_string("$consume");
    char error[64] = {0};
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1 &&
              strstr(error, "cycle") != NULL,
          "cycles formed only by result references should be rejected");
    wct_relation_graph_free(&graph);
}


static void test_relation_arity_and_types(void)
{
    wct_relation_graph graph = {0};
    char error[128] = {0};
    graph.call_count = 1;
    graph.calls = calloc(1, sizeof *graph.calls);
    graph.calls[0].id = copy_string("typed");
    graph.calls[0].argc = 1;
    graph.calls[0].args = calloc(1, sizeof(char *));
    graph.calls[0].args[0] = copy_string("true");
    graph.calls[0].expected_argc = 2;
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1 &&
          strstr(error, "arity mismatch") != NULL,
          "declared call arity mismatch should be rejected");
    graph.calls[0].expected_argc = 1;
    graph.calls[0].arg_type_count = 1;
    graph.calls[0].arg_types = calloc(1, sizeof *graph.calls[0].arg_types);
    graph.calls[0].arg_types[0] = WCT_INT;
    memset(error, 0, sizeof error);
    CHECK(wct_validate_relation(&graph, error, sizeof error) == -1 &&
          strstr(error, "type mismatch") != NULL,
          "declared argument type mismatch should be rejected");
    graph.calls[0].arg_types[0] = WCT_BOOL;
    CHECK(wct_validate_relation(&graph, error, sizeof error) == 0,
          "matching bool argument type should validate");
    wct_relation_graph_free(&graph);
}

static void test_parser_call_contract(void) {
    const char *path = "/tmp/wct-contract.model"; FILE *f=fopen(path,"w"); wct_state_graph st={0}; wct_relation_graph g={0}; char e[128]={0};
    CHECK(f != NULL, "contract fixture writable"); if (!f) return; fputs("schema 1\nrelation_graph r\ncall fetch true\ncontract fetch 1 bool\n",f); fclose(f);
    CHECK(wct_parse_file(path,&st,&g,e,sizeof e)==0 && g.calls[0].expected_argc==1 && g.calls[0].arg_type_count==1 && g.calls[0].arg_types[0]==WCT_BOOL, "parser should load call contracts");
    wct_state_graph_free(&st); wct_relation_graph_free(&g); remove(path);
}

static void test_parser_boundaries(void)
{
    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char error[128] = {0};
    CHECK(wct_parse_file("fixtures/smoke.model", NULL, &relation,
                         error, sizeof error) == -1,
          "parser should reject a null state output");

    const char *path = "/tmp/wct-overlong.model";
    FILE *file = fopen(path, "w");
    CHECK(file != NULL, "overlong fixture should be writable");
    if (file == NULL) return;
    fputs("schema 1\n#", file);
    for (size_t i = 0; i < 4096; ++i) fputc('x', file);
    fputc('\n', file);
    fclose(file);
    memset(error, 0, sizeof error);
    CHECK(wct_parse_file(path, &state, &relation, error, sizeof error) == -1 &&
              strstr(error, "line too long") != NULL,
          "overlong DSL lines should be rejected without truncation");
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
    remove(path);
}

static void test_parse_fixtures(void)
{
    wct_state_graph state = {0};
    wct_relation_graph relation = {0};
    char error[128] = {0};
    CHECK(wct_parse_file("fixtures/state_graph.model", &state, &relation,
                         error, sizeof error) == 0, "state fixture should parse");
    CHECK(state.state_count == 3 && state.transition_count == 2,
          "state fixture should populate graph members");
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);

    memset(error, 0, sizeof error);
    CHECK(wct_parse_file("fixtures/function_relations.model", &state, &relation,
                         error, sizeof error) == 0, "relation fixture should parse");
    CHECK(relation.call_count == 3 && relation.relation_count == 2,
          "relation fixture should populate calls and edges");
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);

    memset(error, 0, sizeof error);
    CHECK(wct_parse_file("fixtures/missing_schema.model", &state, &relation,
                         error, sizeof error) == -1,
          "a model without schema should be rejected");
    CHECK(strstr(error, "missing schema") != NULL && state.id == NULL &&
              relation.id == NULL,
          "missing-schema failure should release partial graph allocations");
    wct_state_graph_free(&state);
    wct_relation_graph_free(&relation);
}

int main(void)
{
    test_state_success_and_limit();
    test_state_callback_failure();
    test_state_failure_atomic_snapshot();
    test_isolated_state_snapshot_commit();
    test_zero_snapshot_and_rollback_failure();
    test_isolation_timeout();
    test_timeout_range_and_zero_arity_contract();
    test_bare_result_reference_rejected();
    test_state_branching_coverage();
    test_state_validation_errors();
    test_seed_sampling_determinism();
    test_state_branching_and_unreachable();
    test_parser_diagnostics();
    test_empty_ids_rejected();
    test_relation_order_and_failure();
    test_relation_result_binding();
    test_relation_cycle();
    test_relation_lexical_tie_break();
    test_relation_reference_dependency();
    test_relation_arity_and_types();
    test_parser_call_contract();
    test_parser_boundaries();
    test_parse_fixtures();
    if (failures != 0) {
        fprintf(stderr, "%d API contract test(s) failed\n", failures);
        return 1;
    }
    puts("API contract tests: PASS");
    return 0;
}
