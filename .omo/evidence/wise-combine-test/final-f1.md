# F1 Plan Compliance Audit

Status: **PASS**
Audited tree: `d520342848928da3c25941c445da4fb0eaa1a499` (`d520342`), Linux
working tree at refresh time. Unrelated pre-existing `.omo/ulw-loop/` files were
preserved. This artifact is the only file refreshed by this audit.

## Required RED gate

Before the audit, the required absent-artifact check was run:

```text
$ test -s .omo/evidence/wise-combine-test/final-f1.md
RED_CHECK_RC=1
ARTIFACT_ABSENT=1
```

The failing result is the expected RED state because this artifact did not yet
exist.

## Evidence inventory

- `AGENTS.md`: authoritative mandatory requirements and forbidden memorandum.
- `.omo/plans/wise-combine-test.md`: approved scope, normative contracts,
  todo acceptance criteria, and F1-F4 gate definition.
- `CMakeLists.txt`: C++20, warning policy, sanitizer option, CTest registration,
  and CLI integration tests.
- `src/model`, `src/spec`, `src/generate`, `src/runtime`, `src/report`, and
  `src/cli`: implementation of the model, parser, bounded generator, safe
  adapter, reports, measurements, and CLI.
- `tests/`: model, specification, generator, runtime/protocol, smoke, and
  integration fixtures.
- `README.md`: Linux build, CLI, schema, adapter, report, exit-code, and safety
  instructions.
- `.omo/evidence/wise-combine-test/task-1..6-*`: per-todo command and sanitizer
  evidence. Task 6 records the prior 18/18 Debug and sanitizer CTest passes;
  this refresh reran the current tree at `d520342` with 19/19 passes in both
  configurations, including `runtime_argument_relation`. The exact captured
  output is `final-f1-refresh.log` beside this artifact.

## AGENTS.md mandatory checklist

| Requirement | Evidence mapping | Verdict |
|---|---|---|
| Describe object state graphs and test functions around state transitions. | `src/model/model.*` defines states/transitions/functions and validation; `src/spec/spec.*` parses the declarative graph; `src/generate/generate.*` walks legal transitions; `tests/model_test.cpp`, `tests/spec_test.cpp`, `tests/generator_test.cpp`, `tests/fixtures/state_valid.json`, and `tests/integration/state_workflow.json` exercise it. Task-2/3/4 JSON and Task-6 MD record passing runs. | PASS |
| Describe function relationships (parameter flow and call order), generate and execute combination call flows. | `model::ArgumentRelation` and `OrderingRelation` validate typed producer/consumer bindings and order cycles; generator prerequisite handling is in `src/generate/generate.cpp`; runtime executes each generated transition via the adapter; relation fixtures are `tests/fixtures/relation_valid.json` and `tests/integration/relation_workflow.json`; model/generator/runtime/CLI CTests cover the flow. | PASS |
| Linux operation with minimal third-party dependencies. | `CMakeLists.txt` targets C++20, standard-library code and POSIX process APIs; no `find_package` or runtime third-party dependency is present. `README.md` documents the Linux CMake workflow. Fresh `cmake -S . -B build`, build, and CTest completed with exit 0. | PASS |
| Every program tested and measured. | Current Debug build and `(cd build && ctest --output-on-failure)` passed 19/19. CLI validate/generate/run emit `wall_time_ns`, `cpu_time_ns`, and `peak_rss_bytes`; run summary also emits case/pass/fail counts. Task-6 evidence and this refresh record the same fields and successful CLI checks. | PASS |
| Check memory leaks and out-of-bounds access. | Sanitizer flags (`address,undefined,leak`) are wired in `CMakeLists.txt`; Task-2/3/4/5 sanitizer evidence and this refresh's full sanitizer CTest run report clean runs (19/19). | PASS |
| Provide user instructions. | `README.md` documents build/test, all four CLI commands, JSON schema, limits, adapter protocol/safety, exit codes, reports, sanitizer, Valgrind, timing/RSS, and optional coverage commands. | PASS |
| Commit repository files at each step, including AI files; only binaries/build outputs may be excluded. | Implementation/evidence commits exist in dependency order: `dc912f1` (skeleton + Task 1 evidence), `97782d7` (model + Task 2), `f0ff73a` (spec + Task 3), `8f9bbcb` (generator + Task 4), `301585b`/`77fc4c3` (runtime + Task 5), `ea853c3` (CLI/docs + Task 6), and `82034fd` (integration fixtures). `git show --name-only` confirms `.omo/evidence` and plan/ledger files were committed with the corresponding steps. Build directories remain ignored artifacts. | PASS |

## Plan mandatory contract checklist

| Plan item | Concrete source/test/evidence | Verdict |
|---|---|---|
| Strict version-1 JSON schema and RFC 8259 parsing. | `src/spec/spec.cpp` parser, strict key checks, version/semantic validation, canonical normalization; `tests/spec_test.cpp` covers reordered valid input, trailing-comma rejection, unsupported version, duplicate key, and unknown initial state; Task-3 JSON reports Debug and ASAN passes. | PASS |
| State/transition/function/argument/expectation semantics, legal self-loops/cycles, invalid references and relation constraints. | `src/model/model.cpp`; model tests cover valid graph, self-loop/longer cycle, duplicate IDs, unknown references, relation self-edges, duplicate binding, type mismatch, and order cycle; Task-2 evidence. | PASS |
| Deterministic bounded generation, deduplication, zero/one limits, cycle/dead-end outcomes, and no exhaustive-coverage claim. | `src/generate/generate.cpp`; generator tests assert deterministic linear flow, sorted two-branch case limit, one bounded self-loop flow, order-cycle rejection, and zero cases; `README.md` explicitly describes configured bounds and status 3; Task-4 log/JSON. | PASS |
| Global limits and stable exit codes. | `src/cli/cli.cpp` maps parse=2, exhaustion=3, observed mismatch=4, runtime/protocol failure=5, usage=6; `README.md` documents the mapping; CMake CLI tests cover valid and expected-failure paths; manual CLI transcript below confirms 0/4/2. | PASS |
| Normative adapter protocol and safe subprocess execution. | `src/runtime/runtime.cpp` sends protocol-1 JSON, uses `fork`/`execve` without a shell, fixed allowlisted environment, executable allowlist, process group, 2s step/30s total options, 16 MiB cap, SIGTERM/SIGKILL timeout cleanup, and injects parsed producer returns into consumer arguments; runtime tests cover pass, mismatch, malformed, extra JSON, timeout, crash, cap, and argument relation; Task-5 evidence, this refresh, and README. | PASS |
| Machine-readable/human-readable reports with actionable context. | `src/report/report.cpp` writes JSON/text per flow; `src/cli/cli.cpp` writes run summary and measurements; runtime report fields include sequence index, transition/function, status, observed state, stderr, exit status, and detail. Task-5/6 evidence inspects these artifacts. | PASS |
| Reproducible tests and measurements documented for Linux. | `CMakeLists.txt` registers smoke/model/spec/generator/runtime/CLI tests; README gives exact configure/build/CTest and sanitizer commands plus optional Valgrind/coverage and `/usr/bin/time -v`; Task-1..6 evidence captures outputs. | PASS |

## Todo acceptance coverage

1. **Todo 1 / skeleton:** `CMakeLists.txt`, smoke target, warnings, CTest, and sanitizer option; Task-1 log records configure/build, portable CTest result, and invalid-generator nonzero check. PASS.
2. **Todo 2 / model:** typed state/function/relation model and negative invariants; `tests/model_test.cpp` and Task-2 JSON including sanitizer run. PASS.
3. **Todo 3 / spec:** strict parser, normalization, semantic diagnostics; `tests/spec_test.cpp` and Task-3 Debug/ASAN evidence. PASS.
4. **Todo 4 / generator:** deterministic bounded flows and explicit exhaustion; `tests/generator_test.cpp` and Task-4 Debug/ASAN log. PASS.
5. **Todo 5 / runtime/report:** safe adapter, failure matrix, timeout/cap/process cleanup, JSON/text reports; `tests/runtime_test.cpp`, `tests/fixture_adapter.cpp`, Task-5 evidence. PASS.
6. **Todo 6 / CLI/integration/docs/measurements/safety:** `src/cli`, integration fixtures, README, CTest registrations, measurement fields, sanitizer commands, and optional-tool guidance; Task-6 evidence and final 19/19 CTest run. PASS.

## Forbidden-scope checks

The plan and AGENTS.md forbid parameter-varying completeness analysis,
arbitrary unsafe symbol invocation, GUI/network/distributed/plugin surfaces,
and claims of exhaustive coverage beyond configured bounds. The product tree
was scanned with:

```text
$ rg -ni --glob 'src/**' --glob 'tests/**' --glob 'README.md' --glob 'CMakeLists.txt' \
    'parameter.?sweep|completeness checker|dynamic symbol|GUI|network service|distributed execution|plugin marketplace' .
rg_exit=1 (1 means no forbidden matches)
```

The implementation invokes only allowlisted adapter executables through
`execve`; it contains no GUI, network service, distributed runner, plugin
marketplace, or parameter-sweeping checker. `README.md` describes generation as
bounded and does not claim exhaustive coverage. **All forbidden-scope checks:
PASS.**

## Auxiliary CLI transcript

Run against the final Debug binary and repository fixtures:

```text
validate tests/fixtures/state_valid.json: exit=0
{"valid":true,"version":1,"state_count":2,"transition_count":1,"wall_time_ns":421937,"cpu_time_ns":421220,"peak_rss_bytes":1699840}

generate tests/fixtures/relation_valid.json: exit=0
{"status":"dead_end","case_count":1,"flows":[{"flow_id":"produce","transitions":["produce","consume"]}],"seed":9,...measurement fields...}

run relation_valid.json with adapter_ok: exit=0; case_count=1, passed=1, failed=0
run relation_valid.json with adapter_mismatch: exit=4; case_count=1, passed=0, failed=1
report generated relation-0.txt: exit=0; flow produce passed, steps produce/consume passed
validate tests/fixtures/invalid.json: exit=2; diagnostic identifies unknown initial state `missing`
```

The successful run summary contained `case_count`, `passed`, `failed`,
`wall_time_ns`, `cpu_time_ns`, and `peak_rss_bytes`; the report contained both
flow and step details.

## Verification and cleanup

```text
$ git diff --check
# no output; exit=0

$ test -s .omo/evidence/wise-combine-test/final-f1.md
# after writing this artifact: exit=0

Temporary CLI path `/tmp/wise-f1-cli.TcP1TP` was removed with `unlink`/`rmdir`;
cleanup retry reported `remaining=absent`. No child processes or listening
ports were left running. Existing ignored `build/` and `build-asan/` outputs
were retained for reproducibility and were not treated as repository files.
```

## Final conclusion

Every mandatory AGENTS.md requirement and every mandatory item in the approved
plan has a concrete implementation, test, README, commit, or evidence mapping.
Forbidden scope is absent, required safety/measurement evidence is present, and
there are **zero unresolved checklist items**. **F1 PASS.**
