# wise_combine_test

`wise-combine-test` is a small C11/Linux combination-test runner. Models are
plain text and contain a versioned schema, state transitions, and function
relations. The public C API is declared in `include/wct.h`.

## Build and run

```sh
make
./bin/wise-combine-test --help
./bin/wise-combine-test --model fixtures/smoke.model --mode state
./bin/wise-combine-test --model fixtures/relation.model --mode relation
# capture and replay a deterministic execution
./bin/wise-combine-test --model fixtures/function_relations.model --mode relation \
  --trace /tmp/function.trace
./bin/wise-combine-test --replay /tmp/function.trace
```

The DSL is whitespace-separated:

```text
schema 1
state_graph graph-id initial-state
state state-id
transition transition-id from-state to-state input expected-output
relation_graph calls-id
call function-id argument...
relation prerequisite dependent
contract function-id argc type...
```

`schema 1` is the on-disk compatibility boundary. New directives and
contract attributes are append-only; an older reader must reject an unknown
required directive rather than silently changing execution. Trace files carry
their own `WCT_TRACE 1` version and canonical model/IR/metadata digests, so a
replay is accepted only when both the schema and all recorded inputs match.

Call contracts are optional. For example, `contract fetch 2 string int`
requires two arguments and validates their declared literal types (`int`,
`bool`, `string`, `bytes`, `ref`, or `any`).

Contracts may append `result=<type>` to validate the callback result and
`expect=<text>` to require an exact result. A producer's declared result type
also constrains `$producer` reference arguments. Both attributes are optional,
so existing schema-1 contracts remain valid.

`--mode state` executes reachable transitions in valid graph order. Branches
may replay a prefix from the initial state; callers with mutable state should
provide `wct_limits.state_reset` before scenario replay. `--mode relation` executes calls in
dependency order and prints the ordered call trace. A `$call` argument is an
implicit dependency and receives the producer callback's result. `--trace`
writes a versioned, checksummed text trace; `--replay` reruns the referenced
model and rejects model, step-count, exit-code, or trace-digest changes.
Production users should provide callbacks through the C API rather than
relying on these example callbacks. Invalid models and callback failures
return exit code 1; CLI usage errors return exit code 2.

The C API and CLI isolate each complete scenario in a POSIX child by default,
including trace capture and replay. `--isolate` is retained for compatibility
and does not add a second isolation boundary. `--timeout-ms N` is independent of
that flag and makes the whole-scenario monotonic deadline explicit. Isolation
status is included in the report (`process_exit`, `process_signal`,
`timed_out`). Trace capture writes unbuffered child output and recomputes the
step digest from the trace file, so callback side effects remain isolated while
replay stays deterministic.

Trace digests are unkeyed 64-bit consistency checks. Replay detects accidental
editing and deterministic mutation of recorded fields; it does not authenticate
a trace or defend against an attacker who rewrites all fields and digests.
Replay deliberately opens the model path recorded in the trace, so replay a
trace only when that path is trusted. Timeout settings are execution controls,
not recorded replay inputs; a replayed scenario has no captured original
deadline.

State isolation is transactional and scenario-scoped: the parent owns state
snapshots, and a successful state child commits its serialized post-state;
failed scenarios, failed callbacks, timeouts, assertion mismatches, and failed
final commits roll back atomically. Relation scenarios execute entirely
in the child and deliberately do not commit arbitrary callback context (callers
must return results through the callback contract). The child executes
the callback under a monotonic deadline. Serialized state is committed only for
state scenarios after the result satisfies the transition contract. Failed callbacks,
timeouts, and assertion mismatches are discarded. Mutable API contexts must
provide paired snapshot/restore hooks; branch replay uses the reset hook to
begin each scenario from the declared initial state.

## Verification

`make test` runs the standalone smoke checks and the controlled queue-oracle
regression (`tests/test_queue_oracle.sh`): 84 clean/mutant matrix runs plus
cycle/subset/repeat probes. `make sanitize` enables
AddressSanitizer and UndefinedBehaviorSanitizer and runs isolated intentional
OOB/leak sentinels; expected sanitizer findings are classified separately from
product failures. `make valgrind` runs when
Valgrind is installed and otherwise records an explicit skip. A measurement
TSV can be generated with `LC_ALL=C make measure OUT=evidence/iter-0/measure.tsv`.
It repeats the fixed command three times by default, records wall/user/system
CPU and maximum RSS, and writes a companion median/min/max/range table. Use
`REPEAT=N`, `FIXTURE=...`, and `MODE=...` for controlled comparisons.

The iteration gates and verification evidence are tracked in
[`docs/release-readiness.md`](docs/release-readiness.md). `make coverage`
cleans stale `*.gcda`, `*.gcno`, and report files, instruments the shared
library object used by both the CLI and API test harness, runs the CLI/API/fuzz
queue-oracle suite, and writes one gcov report per discovered profile plus
`coverage/summary.txt`. It fails if the expected `src/wct.c` and
`tools/wct_cli.c` profiles are absent.

The last local coverage measurement used GCC/gcov 9.4.0 on Linux:

| Source | Lines executed | Branches executed | Branches taken at least once |
| --- | ---: | ---: | ---: |
| `src/wct.c` | 78.52% (717 lines) | 81.55% (1138 branches) | 58.35% |
| `tools/wct_cli.c` | 92.01% (338 lines) | 96.45% (620 branches) | 67.10% |

`make coverage` exports `WCT_COVERAGE_ROOT`. Every normal fork exit calls
`child_exit()`; when the coverage root is present it sets GCC's `GCOV_PREFIX`
to a child-PID directory, then calls `__gcov_dump` before `_exit`. This avoids
the usual fork-coverage loss where a parent's later profile write overwrites
the child. `tools/merge-coverage.sh` filters profiles to object files present in
`build/`, merges them with GCC's `gcov-tool`, and copies the merged product
profiles back before `gcov` reports. The harness's own `test_api` profile is
intentionally excluded because the release measurement targets `src/wct.c` and
`tools/wct_cli.c`. A child that calls `exec` would have to dump before
replacement; this runner does not replace its fork children with `exec`.

The measured numbers above are the current baseline, not an 80% coverage
claim: the library line total remains below 80%, and both branch-taken totals
remain below 80%. Valgrind is optional at runtime; `make valgrind` records an
explicit skip when it is not installed.
