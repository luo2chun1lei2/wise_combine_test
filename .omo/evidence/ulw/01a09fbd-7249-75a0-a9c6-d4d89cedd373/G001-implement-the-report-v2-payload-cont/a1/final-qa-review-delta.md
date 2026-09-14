# Replay/Integrity Delta — Final QA Review

## Final verdict

passed

## Review scope

- Frozen HEAD: `d04e650d8f425f29066c723d9e6656bf2e805d0b` (verified immediately before report generation).
- QA class: executable hands-on delta QA at the CLI/CTest surface.
- Temporary QA root: `/tmp/wct-delta-qa.BWV9nD`; build trees, generated reports, and logs were confined there.
- No source, test, doc, CMake, or README edits were made. `git diff --name-only -- src tests docs CMakeLists.txt README.md README.zh.md` was empty.
- Initial tracked-state exception: `.omo/ulw-loop/01a09fbd-7249-75a0-a9c6-d4d89cedd373/spawn-count.json` was already modified before QA execution. An untracked `final-code-review-delta.md` was observed later in the adjacent evidence directory; it was not read, edited, moved, or deleted.
- This requested report is the only repository write made by this QA run.

## Environment and build

| Item | Result |
|---|---|
| OS/kernel | Linux `Linux 5.15.0-139-generic x86_64 GNU/Linux` |
| Compiler | `c++ (Ubuntu 9.4.0-1ubuntu1~20.04.2) 9.4.0` |
| CMake/CTest | `cmake version 3.16.3` |
| Python cleanup | `Python 3.8.10` |
| Debug build | PASS — configure/build completed |
| ASan build | PASS — `-DWISE_COMBINE_ENABLE_SANITIZERS=ON` |
| Coverage build | PASS — `-DWISE_COMBINE_ENABLE_COVERAGE=ON` |

Build invocations used fresh build trees outside the repository:

```sh
cmake -S . -B /tmp/wct-delta-qa.BWV9nD/build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/wct-delta-qa.BWV9nD/build-debug --parallel
cmake -S . -B /tmp/wct-delta-qa.BWV9nD/build-asan -DCMAKE_BUILD_TYPE=Debug -DWISE_COMBINE_ENABLE_SANITIZERS=ON
cmake --build /tmp/wct-delta-qa.BWV9nD/build-asan --parallel
cmake -S . -B /tmp/wct-delta-qa.BWV9nD/build-coverage -DCMAKE_BUILD_TYPE=Debug -DWISE_COMBINE_ENABLE_COVERAGE=ON
cmake --build /tmp/wct-delta-qa.BWV9nD/build-coverage --parallel
```

## Scenario 1 — clean surface with relative adapter

**Result: PASS**

Invocation (from repository root; adapter is deliberately relative):

```sh
/tmp/wct-delta-qa.BWV9nD/build-debug/wise-combine run \
  /home/workspace_data/works/myprojects/wise_combine_test.lazycodex/tests/integration/relation_workflow.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports /tmp/wct-delta-qa.BWV9nD/clean/original --run-id relation

/tmp/wct-delta-qa.BWV9nD/build-debug/wise-combine verify-report-v2 \
  /tmp/wct-delta-qa.BWV9nD/clean/original/relation-0.v2.json

/tmp/wct-delta-qa.BWV9nD/build-debug/wise-combine replay \
  /tmp/wct-delta-qa.BWV9nD/clean/original/relation-0.v2.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports /tmp/wct-delta-qa.BWV9nD/clean/replay --run-id replay

/tmp/wct-delta-qa.BWV9nD/build-debug/wise-combine verify-report-v2 \
  /tmp/wct-delta-qa.BWV9nD/clean/replay/replay-0.v2.json
```

Observables:

| Check | Expected | Observed |
|---|---:|---:|
| `run` exit | 0 | 0 |
| Original v2 verification exit | 0 | 0 |
| `replay` exit | 0 | 0 |
| Replay v2 verification exit | 0 | 0 |
| Original run summary | `generation_status=dead_end`, `case_count=1`, `passed=1`, `failed=0` | Same |
| Original v2 verify output | all validity flags true | `{"valid":true,"integrity_verified":true,"payload_valid":true,"schema_version":2}` |
| Replay v2 verify output | all validity flags true | `{"valid":true,"integrity_verified":true,"payload_valid":true,"schema_version":2}` |
| Input SHA-256 before/after | unchanged | `bfccbdced7c2b747b5091630a6d66be6d149e0ef22b4666bbc073892c0a9359c` |
| Original artifacts | v1, TXT, v2, summary | All four present |
| Replay artifacts | v1, TXT, v2 | All three present |
| Original/replay v2 envelope SHA | identical | `a4746f899e26d9e60d057172f4ba33d7233de6f5ba99febcf7fed909c71f4272` |

## Scenario 2 — adversarial/safety matrix

**Result: PASS**

Focused invocation (the host CTest does not support `--test-dir`; the requested equivalent directory-specific form was used):

```sh
(cd /tmp/wct-delta-qa.BWV9nD/build-debug && \
  ctest -R 'integrity_v2|cli_replay_v2' --output-on-failure)
```

Observed: `100% tests passed, 0 tests failed out of 2` (`integrity_v2` and `cli_replay_v2`). A second `--verbose` run passed both tests and captured the exact CTest commands.

CTest suppresses successful CMake-script assertion stdout, so the binary PASS was cross-checked against the exact frozen assertions executed by `cli_replay_v2`. Any failed assertion would have failed CTest.

| Required adversarial class | Frozen assertion location / observable | Focused result |
|---|---|---|
| Malformed payload with correct envelope digest | `tests/integrity_v2_test.cpp:15-23` constructs malformed payload `{"broken":}`, wraps it with its correct SHA-256, and requires `verify_v2` false with empty decoded payload. | PASS |
| Empty mismatch payload rejection | `tests/cli_replay_v2.cmake:86-111` wraps an empty mismatch payload and requires `verify-report-v2` exit 2. | PASS |
| Adapter digest mismatch, no child | `tests/cli_replay_v2.cmake:113-139` requires replay exit 5, `digest does not match`, marker child file absent, and output report directory absent. | PASS |
| Missing recorded workdir | `tests/cli_replay_v2.cmake:141-155` creates a nonexistent recorded working directory and requires rejection before child start. | PASS |
| Output collision rejection | `tests/cli_replay_v2.cmake:178-185` replays into the occupied input report directory and requires exit 5 plus `overwrite the input`. | PASS |
| Alias output rejection | `tests/cli_replay_v2.cmake:157-176` uses a symlink output alias, requires exit 5, unchanged input SHA, and no new v2. Independently re-run below with a hard link. | PASS |

### Independent hard-link alias case

**Result: PASS**

Invocation:

```sh
ln /tmp/wct-delta-qa.BWV9nD/clean/original/relation-0.v2.json \
  /tmp/wct-delta-qa.BWV9nD/alias/output/alias-0.json

/tmp/wct-delta-qa.BWV9nD/build-debug/wise-combine replay \
  /tmp/wct-delta-qa.BWV9nD/clean/original/relation-0.v2.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports /tmp/wct-delta-qa.BWV9nD/alias/output --run-id alias
```

Observables:

- Exit code: `5` (expected `5`).
- Stderr match: `output report path already exists`.
- Input SHA-256 unchanged: `a4746f899e26d9e60d057172f4ba33d7233de6f5ba99febcf7fed909c71f4272`.
- Input inode and hard-link count unchanged.
- `alias-0.v2.json` was not created.

## Scenario 3 — regression, sanitizer, and coverage gates

**Result: PASS**

| Gate | Invocation | Expected | Observed |
|---|---|---|---|
| Full Debug CTest | `(cd /tmp/wct-delta-qa.BWV9nD/build-debug && ctest --output-on-failure)` | 45/45 | `100% tests passed, 0 tests failed out of 45` |
| Full ASan CTest | `(cd /tmp/wct-delta-qa.BWV9nD/build-asan && ctest --output-on-failure)` | 45/45, no diagnostics | `100% tests passed, 0 tests failed out of 45` |
| Leak/out-of-bounds diagnostics | grep ASan/LSan/runtime/Sanitizer-summary patterns over ASan output | no matches | No matches |
| Coverage tests | `(cd /tmp/wct-delta-qa.BWV9nD/build-coverage && ctest --output-on-failure)` | 45/45 | `100% tests passed, 0 tests failed out of 45` |
| Coverage threshold | `cmake --build /tmp/wct-delta-qa.BWV9nD/build-coverage --target coverage-check` | >=80% | `project source coverage: 84% (1169 executable lines)` |

## Cleanup receipt

Temporary root was removed with Python standard-library deletion, not shell recursion:

```sh
python3 -c 'import shutil; shutil.rmtree("/tmp/wct-delta-qa.BWV9nD")'
test ! -e /tmp/wct-delta-qa.BWV9nD
```

Observed after deletion: `/tmp/wct-delta-qa.BWV9nD` is absent. The notepad and all QA-only build/report/log artifacts were contained in that removed directory. No child process, server, browser, tmux session, container, or bound port was used by this QA.

## Review limitations and self-review

The `omo:review-work` skill was surveyed because this is an explicit review assignment, but this session exposes no `multi_agent_v1.*` or flat subagent spawn tool, so its required reviewer-child workflow cannot be instantiated. The completed gate is therefore direct, evidence-bound hands-on review.

Self-review checks:

1. Frozen HEAD was asserted before the final cleanup/report phase.
2. Every requested scenario has an explicit invocation and binary observable.
3. Every required adversarial class is mapped to an executed focused-test assertion; the alias case was independently reproduced with a hard link.
4. Debug, ASan, coverage, source-scope, and cleanup gates all passed.
5. Temporary state is absent, and no project source/test/docs state was changed.

## Decision

All required scenarios and cleanup checks passed. Final verdict: **passed**.
