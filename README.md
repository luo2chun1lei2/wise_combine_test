# wise-combine-test

`wise-combine` is a Linux command-line combination tester. A JSON specification
describes an object state graph, typed functions, argument-flow relations, and
global ordering relations. The generator produces deterministic, bounded
transition flows; the runtime executes each step through an explicit adapter
process and writes reproducible failure reports.

The implementation uses C++20 and the standard library plus POSIX process APIs.
There is no runtime third-party dependency.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
(cd build && ctest --output-on-failure)
```

The tests include model, parser, generator, adapter, protocol, timeout, crash,
output-cap, and CLI integration cases. Build paths and test binaries are
ignored by git.

## CLI

```sh
./build/wise-combine validate tests/fixtures/state_valid.json
./build/wise-combine generate tests/fixtures/relation_valid.json
./build/wise-combine run tests/fixtures/relation_valid.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports reports --run-id relation
./build/wise-combine report reports/relation-0.txt
```

`validate` and `generate` print JSON containing wall time, CPU time, and peak
RSS. `run` writes one `<run-id>-N.json` and `<run-id>-N.txt` pair per flow plus
`<run-id>-summary.json` containing `case_count`, `passed`, `failed`, and the
same measurement fields. The report command prints a previously generated
report without changing it.

Exit codes are stable: `0` success, `2` malformed or invalid specification,
`3` bounded generation exhausted (`case_limit` or `step_limit`), `4` an
adapter-observed mismatch, `5` timeout/crash/protocol/launch failure, and `6`
invalid CLI usage. A flow produced before an exhaustion status is still
reported and executed; code 3 records that the configured bound was reached.

## Specification

The top-level JSON object has `version: 1`, `states`, `initial_state`,
`transitions`, `functions`, `relations`, `limits`, and `seed`. Objects and
arrays are normalized into stable lexicographic order where applicable.

```json
{
  "version": 1,
  "states": [{"id":"idle"},{"id":"done"}],
  "initial_state": "idle",
  "transitions": [{
    "id":"finish", "from":"idle", "to":"done", "function":"finish",
    "args": {}, "expect": {"state":"done"}
  }],
  "functions": [{"id":"finish","params":[],"returns":[]}],
  "relations": [],
  "limits": {"max_cases": 10, "max_steps": 8, "max_subprocesses": 1},
  "seed": 1
}
```

An argument relation connects exactly one producer return to one consumer
parameter and requires matching declared types. A `before` relation is a global
ordering edge between transition IDs. Self edges, duplicate bindings,
unknown references, type mismatches, and ordering cycles are rejected with a
diagnostic pointer. `max_cases` may be zero; `max_steps` and
`max_subprocesses` must be positive.

## Adapter contract and safety

The adapter is an executable receiving one line on stdin:

```json
{"protocol":1,"flow_id":"finish","step":0,"function":"finish","args":{}}
```

It must write exactly one JSON object to stdout:

```json
{"protocol":1,"status":"ok","observed_state":"done","returns":{},"stderr":""}
```

`status` may be `ok`, `mismatch`, or `error`; extra JSON, malformed output,
non-zero exits, and missing required fields are protocol failures. The runner
uses `execve` with no shell, a fixed environment (`PATH=/usr/bin:/bin`,
`LC_ALL=C`), a process group, a 2-second step timeout, a 30-second total
timeout, and a 16 MiB combined stdout/stderr cap. Only executable files named
`adapter_*` below `tests/fixtures/bin/` (or the built fixture executable) are
allowed.

## Memory and performance checks

Address, undefined-behavior, and leak sanitizers are enabled with:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DWISE_COMBINE_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
(cd build-asan && ctest --output-on-failure)
```

When installed, Valgrind can provide an additional leak check:

```sh
valgrind --leak-check=full --error-exitcode=1 \
  ./build/wise-combine validate tests/fixtures/state_valid.json
```

GNU `/usr/bin/time -v` reports peak memory and elapsed CPU/wall time for a
whole CLI invocation; the CLI's JSON measurement fields provide the same
values in machine-readable form. Coverage is supplementary and can be
collected with a compiler configured for `--coverage`, followed by
`gcovr`/`lcov` when those optional tools are available.
