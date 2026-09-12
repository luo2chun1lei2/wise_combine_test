# wise-combine-test

`wise-combine` is a Linux command-line combination tester. A JSON specification
describes an object state graph, typed functions, argument-flow relations, and
global ordering relations. The generator produces deterministic, bounded
transition flows; the runtime executes each step through an explicit adapter
process and writes reproducible failure reports.

The implementation uses C++20, POSIX process APIs, and OpenSSL libcrypto for
SHA-256 report integrity work. Install the platform's OpenSSL development
package before building.

## Documentation sync

This repository maintains English and Chinese usage documentation in
`README.md` and `README.zh.md`. Whenever build, CLI, specification, adapter,
report, exit-code, safety, or measurement usage changes, update both files in
the same commit. Keep commands, JSON fields, limits, and behavioral guarantees
aligned; only the explanatory language should differ.

The ongoing improvement roadmap, based on the independent evaluation, is
recorded in [`docs/improvement-roadmap.md`](docs/improvement-roadmap.md).
It lists current boundaries, release blockers, planned enhancements, and
acceptance conditions; implementation status must be updated there as work
lands.
The planned report v2/replay contract is recorded in
[`docs/adr/0003-report-and-replay-format.md`](docs/adr/0003-report-and-replay-format.md).

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
(cd build && ctest --output-on-failure)
```

On Debian/Ubuntu, install the dependency with `sudo apt install libssl-dev`.

The tests include model, parser, generator, adapter, protocol, timeout, crash,
output-cap, and CLI integration cases. Build paths and test binaries are
ignored by git.

Optional coverage instrumentation is available with
`-DWISE_COMBINE_ENABLE_COVERAGE=ON`; run CTest and consume compiler-generated
coverage files with `gcov` or `llvm-cov`.
The configured `coverage-check` target aggregates project source lines and
fails below the required 80% threshold.

The frozen Q1-Q6 queue oracle is also registered as `evaluation_q1` through
`evaluation_q6`; each test runs a clean adapter and its matching single-mutant
adapter, requiring exit codes 0 and 4 respectively. These are supplied-trigger
regressions, not proof of exhaustive automatic coverage.

## CLI

```sh
./build/wise-combine validate tests/fixtures/state_valid.json
./build/wise-combine generate tests/fixtures/relation_valid.json
./build/wise-combine run tests/fixtures/relation_valid.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports reports --run-id relation
./build/wise-combine report reports/relation-0.txt
./build/wise-combine verify-report reports/relation-0.json
./build/wise-combine verify-report reports/relation-summary.json
```

`validate` and `generate` print JSON containing wall time, CPU time, and peak
RSS. `run` writes one `<run-id>-N.json` and `<run-id>-N.txt` pair per flow plus
`<run-id>-summary.json` containing `case_count`, `passed`, `failed`, and the
same measurement fields plus the input `seed` and `schema_version: 1`. The report command prints a previously generated
report without changing it.
Each report step includes `expected_state` alongside `observed_state`, so a
mismatch report records the state oracle used for that step.
`verify-report` also validates run summaries: counters, seed and measurements
must be non-negative integers, and `passed + failed` must equal `case_count`.
This checks structure and consistency, not authenticity or cryptographic integrity.

Exit codes are stable: `0` success, `2` malformed or invalid specification,
`3` bounded generation exhausted (`case_limit` or `step_limit`), `4` an
adapter-observed mismatch, `5` timeout/crash/protocol/launch failure, and `6`
invalid CLI usage. A flow produced before an exhaustion status is still
reported and executed; code 3 records that the configured bound was reached.
If `--reports` cannot be created as a directory (including a regular-file path),
`run` prints a diagnostic to stderr and exits with code `5` before executing flows.
`--run-id` must be a non-empty single file-name component; path separators and
`.`/`..` are rejected with usage exit code `6`.

## Specification

State transitions, including non-self cycles, may repeat within `max_steps`.
Global `before` relations still apply: once the `after` transition occurs,
its `before` transition cannot occur again in that flow.

Every declared function parameter used by a transition must have a literal
argument or an incoming argument relation. Missing sources are rejected during
model validation. This tightens validation of previously accepted incomplete models.

The generator uses `seed` to deterministically shuffle multiple legal choices:
the same specification and seed produce the same flows, while different seeds
may choose a different order when alternatives exist.

If a valid model has no transitions and `max_cases` is positive, the generator
emits one executable empty flow with an empty `flow_id`; a zero `max_cases`
budget still emits no flows.

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

An adapter `status` of `mismatch` yields exit code `4`. A valid `error`
response is reported as `adapter_error` with exit code `5`; an unknown status
is reported as `protocol_error`, also with exit code `5`.

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
