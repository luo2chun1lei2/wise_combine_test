# C API Contract

The public interface is declared in `include/wct.h` and uses caller-owned model
structures plus explicit free functions. The tests in `tests/test_api.c` are a
framework-free executable contract for the first release.

## Ownership

- `wct_parse_file` allocates graph IDs, state/call IDs, arguments, transitions,
  and relation endpoints. The caller releases them with
  `wct_state_graph_free` and `wct_relation_graph_free`, even when a parser
  error is returned after partial allocation.
- A callback allocates `actual`/`result` strings. The executor owns and frees
  each returned string after consuming it. Callback failures may leave the
  output pointer unset.
- `wct_report_free` releases the diagnostic string plus any `scenario`,
  `expected`, and `actual` snapshots, then resets all counters and pointers.

## State graphs

`wct_validate_state` rejects a missing/unknown initial state, duplicate states,
unknown transition endpoints, and duplicate transition IDs. `wct_run_state`
computes states reachable from `initial`, invokes each reachable transition at
most once, and stops at `limits.max_steps` (or its bounded default). The report
contains the input seed, completed `steps`, covered edges, and an `uncovered`
count for unreachable or limit-truncated edges. A callback error or expectation
mismatch returns `-1`, increments `failures`, records `failed_step`,
`scenario`, `expected`, and (when present) `actual`, and does not count the
failed transition as completed or covered.

## Function relations

`wct_validate_relation` rejects duplicate call IDs, unknown relation endpoints,
self-relations, and cycles. `wct_run_relation` invokes calls only after all
declared prerequisites complete, chooses otherwise-independent calls by
lexical call ID, and stops at `limits.max_flows` (or the number of calls).
Arguments are copied into an immutable callback view for each invocation. An
argument written as `$call-id` is replaced by the completed result string from
that prerequisite call; literal arguments are passed unchanged. The report
records incomplete calls in `uncovered`. Callback failures return `-1`, record
the failed step and scenario, and preserve the completed step count.

## Determinism and limits

Given identical graph declarations, callback behavior, and limits, execution is
deterministic. The current executor records `wct_limits.seed` in the report but
does not randomize selection; lexical ID tie-breaking therefore remains stable
across runs.

## Fixtures and verification

Run from the repository root so fixture paths resolve:

```sh
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -Iinclude tests/test_api.c src/wct.c -o /tmp/wct-api-tests
/tmp/wct-api-tests
```

The same command can be rebuilt with `-fsanitize=address,undefined` to check
the explicit free paths. A Valgrind run is optional when the executable is
installed in the environment.
