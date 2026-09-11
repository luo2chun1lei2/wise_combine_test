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
```

`--mode state` executes each reachable transition once, using the input as
the example callback's actual result. `--mode relation` executes calls in
dependency order and prints the ordered call trace. A `$call` argument is an
implicit dependency and receives the producer callback's result. `--trace`
writes a versioned, checksummed text trace; `--replay` reruns the referenced
model and rejects model, step-count, exit-code, or trace-digest changes.
Production users should provide callbacks through the C API rather than
relying on these example callbacks. Invalid models and callback failures
return exit code 1; CLI usage errors return exit code 2.

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
