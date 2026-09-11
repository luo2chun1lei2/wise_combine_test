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
computes reachability from `initial` and invokes transitions in valid graph
order, stopping at `limits.max_steps` (or a replay-safe bounded default).
State execution requires paired `state_snapshot` and `state_restore` hooks so
opaque callback context has explicit transactional semantics.
Branch coverage may replay a prefix from `initial`; provide the optional
`wct_limits.state_reset` hook to restore mutable callback context before each
scenario. For mutable contexts, pair `wct_limits.state_snapshot` and
`wct_limits.state_restore` to make each transition transactional: successful
isolated callbacks commit their serialized post-state only after the callback
result satisfies the transition expectation; callback failures and expectation
mismatches leave caller context unchanged. The restore hook applies serialized
state for either commit or rollback. Edges whose source state cannot be reached are counted in
`report.uncovered`. A callback error or expectation mismatch returns `-1`,
records the failed step/scenario and expected/actual strings, and does not count
the failed transition as a completed step or covered edge.

A successful snapshot is valid even when it is represented by `NULL` and zero
bytes; the runner still invokes restore. Restore failures are reported as
explicit commit/rollback failures. Without snapshot/restore hooks, isolated
callbacks cannot propagate opaque context mutations between transitions;
branch replay therefore rejects that configuration.

## Function relations

`wct_validate_relation` rejects duplicate call IDs, unknown relation endpoints,
self-relations, duplicate edges, unknown `$call` argument references, and
cycles, including cycles formed by result references. `wct_run_relation`
invokes calls only after all declared prerequisites and implicit `$call`
dependencies complete, using lexicographic call-ID tie-breaking for otherwise
independent calls. A call argument beginning with `$` (for example `$fetch`)
is replaced with the prior callback result from that call. Callback failures
return `-1` with a diagnostic and preserve the completed step count; bounded
runs expose incomplete calls in `report.uncovered` and return `-1`.

## Determinism and limits

Given identical graph declarations, callback behavior, and limits, execution is
deterministic. With `seed == 0`, independent calls use lexicographic ID order;
with a non-zero `wct_limits.seed`, the executor uses deterministic xorshift
sampling among ready calls. The seed is copied into the report for trace
metadata, so seeded alternatives remain reproducible.

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


## Call contracts and diagnostics

`wct_call.expected_argc` optionally declares an exact argument count; zero leaves
legacy callers unrestricted. `arg_types`/`arg_type_count` optionally declare one
type per argument. Use `WCT_ANY` to accept any inferred type. Literal arguments
are inferred as integer (decimal), boolean (`true`/`false`), bytes (`0x...`),
string, or reference (`$call`). Validation rejects arity or type mismatches
before callbacks run. DSL models can declare the same metadata after a call with
`contract <call-id> <argc> <type...>`; supported names are `int`, `bool`,
`string`, `bytes`, `ref`, and `any`. The parser reports syntax failures with a one-based line and
column (currently column 1 for directive-level errors), making malformed models
easier to locate.

Contracts may append `result=<type>` to validate a callback's returned value and
`expect=<text>` for an exact result assertion. A producer's declared result type
is also used when checking `$producer` argument references. Both fields are
optional and preserve legacy contract syntax.
