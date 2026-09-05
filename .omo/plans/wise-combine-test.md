# wise-combine-test - Work Plan

## TL;DR (For humans)
<!-- Fill this LAST, after the detailed plan below is written, so it summarizes the REAL plan. -->
<!-- Plain English for a non-engineer: NO file paths, NO todo numbers, NO wave/agent/tool names. -->

**What you'll get:** A Linux command-line combination-testing tool that reads a declarative state/function relationship specification, generates bounded deterministic flows, executes them through a safe adapter, and reports failures with reproducible traces. It will include tests, memory-safety checks, measurements, and user documentation.

**Why this approach:** A typed layered core keeps specification validation, generation, execution, and evidence independently testable while a standard-library-first C++17/CMake baseline honors the repository's minimal-dependency requirement.

**What it will NOT do:** It will not implement parameter-sweeping completeness analysis, arbitrary unsafe in-process symbol calls, or unrequested GUI/network/distributed features.

**Effort:** Large
**Risk:** High - the repository has no implementation and relation semantics plus combinatorial bounds must be made precise.
**Decisions I made for you:** C++17 standard-library-first core, CMake, versioned declarative JSON-like schema, explicit adapter/subprocess execution, deterministic seed and limits, sanitizer-first safety evidence, and optional Valgrind/coverage/CPU-memory hooks.

Your next move: run `$start-work wise-combine-test` in a worker session after this plan's mandatory review passes.

---

> TL;DR (machine): Large/high-risk bootstrap: typed C++17/CMake state/relation engine, deterministic generator, safe execution adapter, CLI/reports, tests, sanitizers, measurements, and README.

## Scope
### Must have
1. Describe object state graphs and test functions around state transitions.
2. Describe function relationships including parameter flow and call order, then generate and execute bounded combination flows.
3. Linux support with minimal dependencies, reproducible tests and measurements, leak/out-of-bounds checks, user instructions, and per-step commits.
4. Actionable diagnostics identifying sequence, state, relation, step, and observed failure.

### Normative contracts
- Schema version `1` is strict RFC 8259 JSON. Top-level keys are `version:1`, `states:[{id:string}]`, `initial_state:string`, `transitions:[{id,from,to,function,args?:{arg:string,value:scalar},expect?:{state:string}}]`, `functions:[{id,params:[{name,type}],returns:[{name,type}]}]`, `relations:[{kind:"argument",producer:{transition,output},consumer:{transition,arg}}|{kind:"before",before,after}]`, `limits:{max_cases:uint,max_steps:uint,max_subprocesses:uint}`, and `seed:uint`; normalization emits UTF-8, locale `C`, lexicographically sorted object keys/arrays by `id`, integers without leading zeroes, and shortest round-trippable decimal numbers.
- A state transition has `id`, `from`, `to`, `function`, optional `args`, and optional `expect`; state self-loops/cycles are legal. An argument relation names exactly one producer transition output and one consumer transition argument, permits one producer to feed many consumers but forbids many producers for one argument, and requires exact declared type match. Ordering relations are `before` edges over transition IDs, apply globally across a generated flow, and must be acyclic with no self-edge. A flow is identified by its ordered transition-ID sequence; deduplication compares that sequence, so a self-loop fixture with `max_steps=3` is one flow of length 3, not three duplicate flows.
- Limits are global `max_cases`, per-flow `max_steps`, and `max_subprocesses`; exhaustion is a reported non-error status distinct from adapter failure. Exit codes are 0 success, 2 parse/validation error, 3 generation exhaustion, 4 observed SUT failure, 5 timeout/crash, and 6 usage error.
- The adapter receives `{\"protocol\":1,\"flow_id\":string,\"step\":number,\"function\":string,\"args\":object}` on stdin and returns exactly `{\"protocol\":1,\"status\":\"ok\"|\"mismatch\"|\"error\",\"observed_state\":string|null,\"returns\":object,\"stderr\":string}` on stdout; malformed/extra JSON is a protocol error. Execution uses `execve` argv without a shell, an executable allowlist of fixture paths under `tests/fixtures/bin`, an allowlisted environment, fixed cwd, 2-second step/30-second total timeout, 16 MiB combined output cap, process-group cleanup, and SIGTERM then SIGKILL.
- Product reports are written to `reports/<run-id>.json` and `reports/<run-id>.txt`; QA copies exact command output and these reports into `.omo/evidence/wise-combine-test/`.
### Must NOT have (guardrails, anti-slop, scope boundaries)
1. The explicitly forbidden parameter-varying call-pattern completeness checker.
2. Arbitrary unsafe dynamic symbol invocation as the default runtime path.
3. GUI, network service, distributed execution, plugin marketplace, or other unrequested product surfaces.
4. Claims of exhaustive coverage when configured generation bounds are partial.

## Verification strategy
> Zero human intervention - all verification is agent-executed.
- Test decision: tests-after for the new repository, using CTest plus small standard-library test executables; each implementation todo includes its tests. Automated evidence is sufficient for technical acceptance; the user's final approval is a separate release gate.
- Evidence: `.omo/evidence/wise-combine-test/task-<N>-wise-combine-test.{log,json,md}`; commands must capture stdout/stderr and generated artifacts.

## Execution strategy
### Parallel execution waves
> Target 5-8 todos per wave. Fewer than 3 (except the final) means you under-split.

- Wave 1: repository/toolchain skeleton and domain model.
- Wave 2: schema parser/validator.
- Wave 3: deterministic generator.
- Wave 4: adapter/runtime and reporting.
- Wave 5: CLI, integration fixtures, documentation, measurements.
- Wave 6: final verification wave after all implementation waves. The serial waves are intentional because each layer's contract is required by the next; the 5-8 guideline is not applicable to this dependency chain.

### Dependency matrix
| Todo | Depends on | Blocks | Can parallelize with |
| --- | --- | --- | --- |
| 1 | none | 2 | none |
| 2 | 1 | 3 | none |
| 3 | 2 | 4 | none |
| 4 | 3 | 5 | none |
| 5 | 4 | 6 | none |
| 6 | 1, 2, 3, 4, 5 | final wave | none |

## Todos
> Implementation + Test = ONE todo. Never separate.
<!-- APPEND TASK BATCHES BELOW THIS LINE WITH edit/apply_patch - never rewrite the headers above. -->
- [ ] 1. Establish the CMake C++17 project skeleton and reproducible test harness
  What to do / Must NOT do: Add `CMakeLists.txt`, `src/`, `tests/`, compiler-warning policy, CTest registration, sanitizer presets, and a minimal smoke executable; do not add runtime third-party dependencies or product features.
  Parallelization: Wave 1 | Blocked by: none | Blocks: 2, 3
  References (executor has NO interview context - be exhaustive): `AGENTS.md:15-20,104-109`; `.gitignore:1-69`; planned `CMakeLists.txt`, `src/`, `tests/`
  Acceptance criteria (agent-executable): `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` exits 0; `cmake --build build --parallel` exits 0; `ctest --test-dir build --output-on-failure` reports the smoke test passed; compiler warnings are enabled and no third-party package is required.
  QA scenarios (name the exact tool + invocation): happy `ctest --test-dir build --output-on-failure`; failure `cmake -S . -B /tmp/wise-combine-invalid -G 'DefinitelyNotAGenerator'` must exit non-zero and its diagnostic is captured in `.omo/evidence/wise-combine-test/task-1-wise-combine-test.log`; Evidence `.omo/evidence/wise-combine-test/task-1-wise-combine-test.log`
  Commit: Y | build(skeleton): add reproducible CMake and CTest baseline
- [ ] 2. Implement typed state/function/relation domain model
  What to do / Must NOT do: Add headers and sources under `src/model/` for states, transitions, functions, parameters, argument edges, ordering constraints, limits, and observed outcomes with explicit invariants and error types. State-transition self-loops and longer cycles are valid; relation/order self-edges and contradictory ordering are invalid. Do not parse files or invoke external code here.
  Parallelization: Wave 1 | Blocked by: 1 | Blocks: 3
  References (executor has NO interview context - be exhaustive): `AGENTS.md:8-11`; planned `src/model/`; `.omo/drafts/wise-combine-test.md` Components/model and Decisions
  Acceptance criteria (agent-executable): model unit tests cover valid construction, valid state self-loop, duplicate states, unknown references, invalid relation/order self-edges, and contradictory ordering; all tests pass under Debug and sanitizer builds.
  QA scenarios (name the exact tool + invocation): happy `ctest --test-dir build -R model --output-on-failure`; failure fixture with an unknown state/function returns a typed validation error and non-zero test assertion; Evidence `.omo/evidence/wise-combine-test/task-2-wise-combine-test.json`
  Commit: Y | feat(model): add typed state and relation domain
- [ ] 3. Define and validate the versioned declarative specification
  What to do / Must NOT do: Add `src/spec/` strict RFC 8259 JSON parser for schema version `1`, canonical key ordering/number formatting, normalization, and semantic validation for state graphs, function signatures, argument flow, ordering, expected observations, seed, and generation limits; provide JSON-pointer diagnostics; do not accept comments/trailing commas or silently coerce invalid relations.
  Parallelization: Wave 2 | Blocked by: 1, 2 | Blocks: 4
  References (executor has NO interview context - be exhaustive): `AGENTS.md:8-20`; planned `src/spec/`; `.omo/drafts/wise-combine-test.md` Open assumptions/input format and Decisions
  Acceptance criteria (agent-executable): valid fixture normalizes deterministically; malformed syntax, unsupported version, duplicate IDs, unknown endpoints, invalid argument bindings, and impossible order constraints are rejected with stable diagnostics and non-zero exit.
  QA scenarios (name the exact tool + invocation): happy `ctest --test-dir build -R spec_valid --output-on-failure`; failure `ctest --test-dir build -R spec_invalid --output-on-failure` verifies each negative fixture and diagnostic path; Evidence `.omo/evidence/wise-combine-test/task-3-wise-combine-test.json`
  Commit: Y | feat(spec): add versioned parser and semantic validation
- [ ] 4. Build deterministic bounded state and relation flow generation
  What to do / Must NOT do: Add `src/generate/` to enumerate legal state-transition paths and relation-respecting call flows using a seed, maximum path length, and global case count; use stable sorted choices, deduplicate identical flows, treat zero as no cases and one as at most one case, and report cycle/dead-end exhaustion; do not claim exhaustive coverage beyond limits or implement forbidden parameter sweeping.
  Parallelization: Wave 3 | Blocked by: 3 | Blocks: 5
  References (executor has NO interview context - be exhaustive): `AGENTS.md:8-11,37-42`; planned `src/generate/`; `.omo/drafts/wise-combine-test.md` generation policy and Scope OUT
  Acceptance criteria (agent-executable): same normalized spec/seed/limits produce byte-identical sequences; reference fixtures assert exact flow IDs/counts (linear=1, two-branch with `max_cases=2`=2, one valid self-loop with `max_steps=3` yields one length-3 flow, ordering cycle=validation error); generated flows never violate state or relation constraints; limit zero/one and cyclic/dead-end graphs terminate with explicit outcomes.
  QA scenarios (name the exact tool + invocation): happy `ctest --test-dir build -R generator_determinism --output-on-failure`; failure `ctest --test-dir build -R generator_limits --output-on-failure` proves bounded termination and rejects invalid limits; Evidence `.omo/evidence/wise-combine-test/task-4-wise-combine-test.json`
  Commit: Y | feat(generate): add bounded deterministic flow generation
- [ ] 5. Implement safe adapter-based runtime execution and structured reporting
  What to do / Must NOT do: Add `src/runtime/` and `src/report/` around a normative no-shell subprocess adapter: `fork`/`execve` with explicit argv, allowlisted environment, fixed working directory, 2-second per-step and 30-second total timeout, 16 MiB output cap, and SIGTERM then SIGKILL cleanup. Track state, compare expected-vs-observed outcomes, and emit JSON/human reports; do not default to arbitrary dynamic symbol calls.
  Parallelization: Wave 4 | Blocked by: 4 | Blocks: 6
  References (executor has NO interview context - be exhaustive): `AGENTS.md:8-20,33-34`; planned `src/runtime/`, `src/report/`; `.omo/drafts/wise-combine-test.md` invocation contract and evidence mapping
  Acceptance criteria (agent-executable): passing adapter fixture executes all steps and records expected final state; failing, malformed-response, timeout, and crash fixtures identify sequence index, function, relation/state mismatch, stderr/exit status, and produce machine-readable plus human-readable artifacts; process groups and inherited file descriptors are closed and runner never hangs.
  QA scenarios (name the exact tool + invocation): happy `ctest --test-dir build -R runtime_pass --output-on-failure`; failure `ctest --test-dir build -R runtime_failure --output-on-failure` validates mismatch, timeout, and non-zero subprocess reports; Evidence `.omo/evidence/wise-combine-test/task-5-wise-combine-test.json`
  Commit: Y | feat(runtime): add adapter execution and failure reports
- [ ] 6. Expose the CLI, integration fixtures, documentation, measurements, and safety gates
  What to do / Must NOT do: Add `src/cli/` (or `src/main.cpp`) commands for validate/generate/run/report, `tests/integration/` fixtures for both mandatory workflows, `README.md`, sanitizer/Valgrind/coverage/CPU-memory measurement scripts or documented invocations, and commit all non-binary artifacts; do not add unrequested service/UI surfaces.
  Parallelization: Wave 5 | Blocked by: 5 | Blocks: final wave
  References (executor has NO interview context - be exhaustive): `AGENTS.md:8-34,70-79,104-109`; planned `src/cli/`, `tests/integration/`, `tests/fixtures/`, `README.md`; `.omo/drafts/wise-combine-test.md` evidence mapping
  Acceptance criteria (agent-executable): documented commands build and run both state and relation fixtures on Linux; `ctest --test-dir build --output-on-failure` passes; `cmake -S . -B build-asan -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined,leak -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined,leak' && cmake --build build-asan && ctest --test-dir build-asan --output-on-failure` is clean; each executable records wall time (ns), CPU time (ns), peak RSS (bytes), case count, and result counts using `clock_gettime`/`getrusage` into JSON evidence; Valgrind/coverage are supplementary and may be marked unavailable; README contains schema, limits, adapter contract, examples, failure interpretation, and safety commands.
  QA scenarios (name the exact tool + invocation): happy `cmake --build build && ctest --test-dir build --output-on-failure` plus `./build/wise-combine validate tests/fixtures/state_valid.json` and `./build/wise-combine run tests/fixtures/relation_valid.json --adapter tests/fixtures/bin/adapter_ok`; failure malformed CLI/spec and failing adapter produce exit codes 2/4 and actionable report; Evidence `.omo/evidence/wise-combine-test/task-6-wise-combine-test.md`
  Commit: Y | feat(cli): deliver executable workflows and verification docs

## Final verification wave
> Runs in parallel after ALL todos. ALL must APPROVE. Surface results and wait for the user's explicit okay before declaring complete.
- [ ] F1. Plan compliance audit
- [ ] F2. Code quality review
- [ ] F3. Real manual QA
- [ ] F4. Scope fidelity

  F1: inspect every changed path against `AGENTS.md` and this plan; verify mandatory requirements have a corresponding test/evidence artifact and forbidden scope is absent. Acceptance: a signed checklist at `.omo/evidence/wise-combine-test/final-f1.md` with zero unaddressed items.
  F2: run `cmake --build build-asan && ctest --test-dir build-asan --output-on-failure` and static review over `src/` and `tests/`; reject leaks, out-of-bounds access, unchecked parse failures, nondeterminism, or unsafe default invocation. Acceptance: sanitizer log and review report at `.omo/evidence/wise-combine-test/final-f2.log` with exit 0.
  F3: act as a Linux user: run the exact CLI commands from Todo 6 for valid/invalid specs and passing/failing/timeout adapters, then inspect product reports plus measurement JSON under `.omo/evidence/wise-combine-test/`. Acceptance: transcript and report hashes at `.omo/evidence/wise-combine-test/final-f3.md`.
  F4: compare the final tree and README with Scope IN/OUT; confirm no GUI/network/distributed surface or forbidden parameter-sweeping checker was introduced. Acceptance: scope checklist at `.omo/evidence/wise-combine-test/final-f4.md` with no violations.

## Commit strategy

One atomic commit per implementation todo, in dependency order (`build`, `model`, `spec`, `generate`, `runtime`, `cli/docs`). Each commit includes its tests and documentation for that behavior, excludes binaries/build directories, and includes any `.omo/evidence/` artifact produced for that step because `AGENTS.md:20` requires per-step repository commits. Final verification does not create product commits unless it finds a defect; fixes are committed as focused follow-ups.

## Success criteria

- A clean Linux checkout configures and builds with the documented CMake command and no runtime third-party dependency.
- Valid state-graph and function-relation specifications validate, generate deterministic bounded flows, execute through the explicit adapter, and produce reproducible reports.
- Invalid specifications, impossible relations, state mismatches, adapter failures, and timeouts fail fast with actionable diagnostics.
- CTest functional/integration tests pass; sanitizer evidence is clean; supplementary measurement and Valgrind/coverage results are captured or tool unavailability is explicit.
- README documents installation, schema, commands, limits, adapters, reports, and troubleshooting; all mandatory and optional requirements are mapped to evidence.
