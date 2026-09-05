# Test Specification: wise_combine_test 迭代验证

## Test Contract

- 所有命令从仓库根目录执行；测试脚本必须返回非零表示失败。
- 每个迭代使用固定 fixture、seed 和流程/深度上限，记录 `git rev-parse HEAD`、编译器版本、内核/架构、命令和退出码。
- 证据目录只存文本/JSON/TSV/日志，不提交编译产物；建议路径 `evidence/iter-N/{commands.txt,summary.json,stdout.log,stderr.log,measure.tsv,sanitizer.log,valgrind.log}`。
- 后一迭代的入口脚本先检查前一迭代 `summary.json` 为 `status: PASS`，否则退出 2（blocked）。

## Iteration Matrix

### I0: baseline

**Focus:** 构建、CLI smoke、报告/证据脚本。

**Commands**

```sh
make clean all
make test TEST_FILTER=smoke
make sanitize TEST_FILTER=smoke
make measure FIXTURE=fixtures/smoke.model OUT=evidence/iter-0/measure.tsv
```

**Expected signals:** all commands exit 0；`--help`/`--version` 输出非空且退出 0；`summary.json.status == PASS`；sanitizer 无 `ERROR`/`runtime error`；测量包含 CPU 与最大 RSS 字段。

**Evidence:** `evidence/iter-0/summary.json`, `stdout.log`, `stderr.log`, `measure.tsv`, `sanitizer.log`。

### I1: state graph

**Focus:** 解析/校验、迁移覆盖、状态断言、错误诊断。

**Commands**

```sh
make test TEST_FILTER='state_*'
make integration TEST_FIXTURE=fixtures/state_graph
make sanitize TEST_FILTER='state_*'
make valgrind TEST_FILTER='state_*'  # unavailable => explicit SKIP evidence
make measure FIXTURE=fixtures/state_graph SEED=7 OUT=evidence/iter-1/measure.tsv
```

**Fixtures/assertions:** valid graph covers each declared edge; invalid graph cases (duplicate/unknown state, missing target, malformed input) fail validation with line/column; callback failure reports model ID, seed, step, expected and actual state; cycle stops at configured max depth.

**Expected signals:** all available required commands exit 0; negative fixtures return documented non-zero without crash; sanitizer/Valgrind logs have zero findings; coverage summary includes parser, validator, generator and executor paths.

**Evidence:** `evidence/iter-1/summary.json`, `state-*.log`, `sanitizer.log`, `valgrind.log` or `valgrind.skip`, `measure.tsv`.

### I2: function relations

**Focus:** parameter binding, result passing, ordering and relation errors.

**Commands**

```sh
make test TEST_FILTER='relation_*'
make integration TEST_FIXTURE=fixtures/function_relations
make sanitize TEST_FILTER='relation_*'
make valgrind TEST_FILTER='relation_*'
make measure FIXTURE=fixtures/function_relations SEED=11 OUT=evidence/iter-2/measure.tsv
```

**Fixtures/assertions:** valid linear and branching graphs receive expected arguments/results in topological order; cycle, unknown function, arity/type mismatch and unsatisfied prerequisite are rejected; same seed reproduces identical ordered call trace; process/depth limits are honored.

**Expected signals:** exit 0 for valid cases; documented non-zero for invalid cases; no memory findings; call trace contains monotonically increasing step IDs and source relation IDs; measure is comparable to I1.

**Evidence:** `evidence/iter-2/summary.json`, `relation-*.log`, `call-trace.jsonl`, sanitizer/Valgrind logs, `measure.tsv`.

### I3: release regression

**Focus:** full regression, deterministic replay, docs and clean build.

**Commands**

```sh
make clean all
make test
make sanitize
make valgrind
make coverage
make measure FIXTURE=fixtures/all SEED=11 OUT=evidence/iter-3/measure.tsv
./bin/wise-combine-test --replay evidence/iter-2/call-trace.jsonl
```

**Expected signals:** clean build and all tests exit 0; replay matches stored trace checksum; sanitizer has no findings; Valgrind has no leaks/errors (or explicit environment blocker); coverage report is emitted (80% is a stretch target, not a blocker unless adopted); README commands work in a clean checkout.

**Evidence:** `evidence/iter-3/summary.json`, `coverage/`, replay log, all tool logs, `measure.tsv`, environment manifest.

## Required Test Categories

1. Unit: tokenization/parsing, model validation, deterministic seed, bounded generator, report formatting.
2. Integration: end-to-end state graph and relation fixtures with real callbacks and expected traces.
3. Negative/robustness: malformed files, duplicate IDs, dangling references, cycles, limits, callback errors, non-zero exit codes.
4. Memory safety: ASan/UBSan on every iteration from I0 onward; Valgrind leak/error check on Linux where installed.
5. Measurement: fixed workload, repeated runs (at least 3 in I3), CPU time and peak RSS recorded with tool/version metadata.

## Stop/Block Criteria

- Any mandatory command fails, sanitizer reports an error, or a valid fixture is nondeterministic: stop before next iteration and write `status: BLOCKED` with reproducer.
- Missing Valgrind executable is an environment limitation, not permission to omit memory checking: retain sanitizer evidence and a `SKIP` record naming the missing tool.
- No test may claim detection of unmodeled parameter-space omissions; such scenarios are out of scope.

## Architect Refinements (apply to the matrix above)

- **I0 contract freeze:** freeze a versioned schema, CLI flags/exit codes, and C API/ABI snapshots; parse into a canonical IR. The IR test vectors must cover value types (integer, boolean, string, bytes, reference), ownership/free responsibility, and per-node/per-flow/input/output byte, depth, and count limits. I3 performs compatibility regression only; it must not extend the schema.
- **State semantics (I1):** start generation at the declared initial state and report reachable-edge coverage plus explicitly unreachable edges. A failed transition is atomic (state/context remain equal to the pre-step snapshot). Tests pin cycle/repeat semantics and max-depth termination. Execute each scenario in a POSIX `fork` child; parent uses monotonic-clock polling with `waitpid`, kills and reaps on timeout, and records signal/exit status.
- **Relation semantics (I2):** normalize declarations to a DAG, require coverage of every declared edge (or report un-sampled edges), and pin stable-ID lexical tie-breaks plus seed-driven sampling. Every trace records schema/IR hash, seed, limits, selected edge IDs and selection reason. Include fork/waitpid isolation and timeout fixtures.
- **Memory gates:** use fixed `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` from I0 onward. Run intentional OOB and leak sentinels; the harness must detect and classify those findings as expected before the gate can pass. Missing sanitizer compiler/runtime, failed sentinel detection, or unavailable POSIX process harness blocks the iteration. Valgrind absence is an explicit blocking/`SKIP` record and never a silent omission.
- **Evidence manifest:** each `evidence/iter-N/manifest.json` contains prior `summary.json` digest (null for I0), current HEAD, every command and exit code, tool versions, Linux environment, git commit/push result, and SHA-256 for each artifact. Missing any field blocks progression.
- **Measurement/replay/fuzz:** run measurements with `LC_ALL=C`, identical fixture/seed/limits and normalized case/step counts; repeat at least three times in I3 and report median/range. Freeze a trace schema containing input/IR hash, per-step hash, metadata and expected process exit code; replay fails on hash or exit-code mismatch. I0 and I3 run DSL boundary and fuzz tests, treating crash or timeout as failure.
