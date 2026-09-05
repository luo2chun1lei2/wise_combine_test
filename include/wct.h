#ifndef WCT_H
#define WCT_H

#include <stddef.h>
#include <stdint.h>

#define WCT_SCHEMA_VERSION 1u

typedef enum { WCT_INT, WCT_BOOL, WCT_STRING, WCT_BYTES, WCT_REF } wct_value_type;
typedef struct {
    wct_value_type type;
    union { int64_t integer; int boolean; const char *string; struct { const unsigned char *data; size_t len; } bytes; const char *ref; } as;
} wct_value;

typedef struct { char *id; char *from; char *to; char *input; char *expect; } wct_transition;
typedef int (*wct_transition_fn)(const char *input, char **actual, void *ctx);
typedef struct { char *id; char *initial; char **states; size_t state_count; wct_transition *transitions; size_t transition_count; } wct_state_graph;

typedef struct { char *id; char **args; size_t argc; } wct_call;
typedef struct { char *from; char *to; } wct_relation;
typedef int (*wct_call_fn)(const char *id, const char *const *args, size_t argc, char **result, void *ctx);
typedef struct { char *id; wct_call *calls; size_t call_count; wct_relation *relations; size_t relation_count; } wct_relation_graph;

typedef struct { size_t max_steps; size_t max_flows; unsigned seed; } wct_limits;
typedef struct { size_t steps; size_t covered; size_t failures; char *error; } wct_report;

void wct_state_graph_free(wct_state_graph *g);
void wct_relation_graph_free(wct_relation_graph *g);
void wct_report_free(wct_report *r);
int wct_parse_file(const char *path, wct_state_graph *state, wct_relation_graph *rel, char *err, size_t errlen);
int wct_validate_state(const wct_state_graph *g, char *err, size_t errlen);
int wct_validate_relation(const wct_relation_graph *g, char *err, size_t errlen);
int wct_run_state(const wct_state_graph *g, wct_transition_fn fn, void *ctx, wct_limits limits, wct_report *report);
int wct_run_relation(const wct_relation_graph *g, wct_call_fn fn, void *ctx, wct_limits limits, wct_report *report);

#endif
