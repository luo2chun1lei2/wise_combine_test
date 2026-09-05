#define _POSIX_C_SOURCE 200809L

#include "wct.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                        (wct_limits){.max_steps = 2}, &report) == 0,
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
                        (wct_limits){.max_steps = 1}, &report) == -1,
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

    CHECK(wct_run_state(&graph, state_callback, &context, (wct_limits){0}, &report) == -1,
          "callback failure should fail state run");
    CHECK(report.failures == 1 && report.steps == 0 && report.covered == 0,
          "failed transition must not count as completed or covered");
    CHECK(report.error != NULL, "failed transition should provide an error");
    wct_report_free(&report);
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
    if (context->fail_id != NULL && strcmp(id, context->fail_id) == 0)
        return -1;
    *result = copy_string(id);
    return *result == NULL ? -1 : 0;
}

static void init_relation_graph(wct_relation_graph *graph)
{
    memset(graph, 0, sizeof *graph);
    graph->id = copy_string("pipeline");
    graph->call_count = 3;
    graph->calls = calloc(graph->call_count, sizeof *graph->calls);
    graph->calls[0] = (wct_call){copy_string("fetch"), calloc(1, sizeof(char *)), 1};
    graph->calls[1] = (wct_call){copy_string("transform"), calloc(1, sizeof(char *)), 1};
    graph->calls[2] = (wct_call){copy_string("store"), calloc(1, sizeof(char *)), 1};
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
                           (wct_limits){.max_flows = 2}, &report) == 0,
          "relation run should succeed under flow limit");
    CHECK(report.steps == 2 && report.covered == 2,
          "relation flow limit should bound completed calls");
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
    test_state_validation_errors();
    test_relation_order_and_failure();
    test_relation_cycle();
    test_parse_fixtures();
    if (failures != 0) {
        fprintf(stderr, "%d API contract test(s) failed\n", failures);
        return 1;
    }
    puts("API contract tests: PASS");
    return 0;
}
