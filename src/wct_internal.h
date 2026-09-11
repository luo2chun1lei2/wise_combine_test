#ifndef WCT_INTERNAL_H
#define WCT_INTERNAL_H

#include "wct.h"

/* Test/trace harness entry points.  These are intentionally not part of the
 * public API: production callers always use whole-scenario isolation. */
int wct_run_state_in_process(const wct_state_graph *, wct_transition_fn, void *, wct_limits, wct_report *);
int wct_run_relation_in_process(const wct_relation_graph *, wct_call_fn, void *, wct_limits, wct_report *);

#endif
