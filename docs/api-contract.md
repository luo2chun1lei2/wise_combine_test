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
starts at `initial`, invokes each not-yet-covered outgoing transition, and
stops at `limits.max_steps` (or its bounded default). A callback error or
expectation mismatch returns `-1`, increments `failures`, and does not count
the failed transition as a completed step or covered edge.

## Function relations

`wct_validate_relation` rejects duplicate call IDs, unknown relation endpoints,
self-relations, and cycles. `wct_run_relation` invokes calls only after all
declared prerequisites complete, preserving declaration order for otherwise
independent calls, and stops at `limits.max_flows` (or the number of calls).
Callback failures return `-1` with a diagnostic and preserve the completed step
count. Arguments are passed as the immutable `const char *const *` view from
each call node.

## Determinism and limits

Given identical graph declarations, callback behavior, and limits, execution is
deterministic. `wct_limits.seed` is reserved for future seed-driven sampling;
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
