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

Call contracts are optional. For example, `contract fetch 2 string int`
requires two arguments and validates their declared literal types (`int`,
`bool`, `string`, `bytes`, `ref`, or `any`).

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

Use `--isolate --timeout-ms N` to execute each callback in a POSIX child
process and terminate callbacks that exceed the deadline. Isolation status is
included in the report (`process_exit`, `process_signal`, `timed_out`). Trace
capture is intentionally mutually exclusive with isolation because callback
side effects and child-process output are not replayable in the parent.

## Verification

`make test` runs the standalone smoke checks. `make sanitize` enables
AddressSanitizer and UndefinedBehaviorSanitizer. `make valgrind` runs when
Valgrind is installed and otherwise records an explicit skip. A measurement
TSV can be generated with `make measure OUT=evidence/iter-0/measure.tsv`.

The iteration gates and verification evidence are tracked in
[`docs/release-readiness.md`](docs/release-readiness.md). `make coverage`
produces a gcov report when GCC's coverage tools are available. Valgrind is
optional at runtime; `make valgrind` records an explicit skip when it is not
installed.
