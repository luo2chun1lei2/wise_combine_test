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
- `wct_report_free` releases the diagnostic string and resets all counters.

## State graphs

`wct_validate_state` rejects a missing/unknown initial state, duplicate states,
unknown transition endpoints, and duplicate transition IDs. `wct_run_state`
computes reachability from `initial` and invokes each reachable transition at
most once, in declaration order, stopping at `limits.max_steps` (or its bounded
default). Edges whose source state cannot be reached are counted in
`report.uncovered`. A callback error or expectation mismatch returns `-1`,
records the failed step/scenario and expected/actual strings, and does not count
the failed transition as a completed step or covered edge.

## Function relations

`wct_validate_relation` rejects duplicate call IDs, unknown relation endpoints,
self-relations, duplicate edges, unknown `$call` argument references, and
cycles. `wct_run_relation` invokes calls only after all declared prerequisites
complete, using lexicographic call-ID tie-breaking for otherwise independent
calls, and stops at `limits.max_flows` (or the number of calls). A call argument
beginning with `$` (for example `$fetch`) is replaced with the prior callback
result from that call. Callback failures return `-1` with a diagnostic and
preserve the completed step count; bounded runs expose incomplete calls in
`report.uncovered`.

## Determinism and limits

Given identical graph declarations, callback behavior, and limits, execution is
deterministic. `wct_limits.seed` is copied into the report for trace metadata;
the current bounded executor does not randomize declaration order.

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
