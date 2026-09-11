#ifndef WCT_H
#define WCT_H

#include <stddef.h>
#include <stdint.h>

#define WCT_SCHEMA_VERSION 1u

typedef enum { WCT_INT, WCT_BOOL, WCT_STRING, WCT_BYTES, WCT_REF, WCT_ANY = -1 } wct_value_type;
typedef struct {
    wct_value_type type;
    union { int64_t integer; int boolean; const char *string; struct { const unsigned char *data; size_t len; } bytes; const char *ref; } as;
} wct_value;

typedef struct { char *id; char *from; char *to; char *input; char *expect; } wct_transition;
typedef int (*wct_transition_fn)(const char *input, char **actual, void *ctx);
typedef void (*wct_transition_observer_fn)(const char *id, void *ctx);
/* Optional hook invoked before replaying a state scenario from the initial state. */
typedef int (*wct_state_reset_fn)(void *ctx);
/* Optional transactional hooks. snapshot allocates an opaque copy that the
 * runner frees with free(); restore must revert ctx to the snapshot. */
typedef int (*wct_state_snapshot_fn)(void *ctx, void **snapshot, size_t *size);
typedef int (*wct_state_restore_fn)(void *ctx, const void *snapshot, size_t size);
typedef struct { char *id; char *initial; char **states; size_t state_count; wct_transition *transitions; size_t transition_count; } wct_state_graph;

typedef struct {
    char *id;
    char **args;
    size_t argc;
    /* Optional contract metadata; contract_set preserves an explicit zero-arity contract. */
    size_t expected_argc;
    wct_value_type *arg_types;
    size_t arg_type_count;
    int contract_set;
    /* Optional callback-result contract. expected_result is an exact assertion. */
    wct_value_type result_type;
    int result_type_set;
    char *expected_result;
} wct_call;
typedef struct { char *from; char *to; } wct_relation;
typedef int (*wct_call_fn)(const char *id, const char *const *args, size_t argc, char **result, void *ctx);
typedef struct { char *id; wct_call *calls; size_t call_count; wct_relation *relations; size_t relation_count; } wct_relation_graph;

typedef struct {
    size_t max_steps;
    size_t max_flows;
    unsigned seed;
    /* Optional POSIX callback isolation.  A zero timeout means no deadline. */
    unsigned timeout_ms;
    int isolate;
    wct_state_reset_fn state_reset;
    wct_state_snapshot_fn state_snapshot;
    wct_state_restore_fn state_restore;
    wct_transition_observer_fn transition_observer;
} wct_limits;
typedef struct {
    size_t steps;
    size_t covered;
    size_t failures;
    size_t uncovered;
    size_t declared_edges;
    size_t covered_edges;
    size_t uncovered_edges;
    int process_exit;
    int process_signal;
    int timed_out;
    size_t failed_step;
    unsigned seed;
    char *scenario;
    char *expected;
    char *actual;
    char *error;
} wct_report;

void wct_state_graph_free(wct_state_graph *g);
void wct_relation_graph_free(wct_relation_graph *g);
void wct_report_free(wct_report *r);
int wct_parse_file(const char *path, wct_state_graph *state, wct_relation_graph *rel, char *err, size_t errlen);
int wct_validate_state(const wct_state_graph *g, char *err, size_t errlen);
int wct_validate_relation(const wct_relation_graph *g, char *err, size_t errlen);
int wct_run_state(const wct_state_graph *g, wct_transition_fn fn, void *ctx, wct_limits limits, wct_report *report);
int wct_run_relation(const wct_relation_graph *g, wct_call_fn fn, void *ctx, wct_limits limits, wct_report *report);

#endif
