# Delta Code Review: Report v2 B1-B3 Fixes

## Verdict

**APPROVE**

Reviewed SHA: `d04e650d8f425f29066c723d9e6656bf2e805d0b`
Delta range: `2ec151c08df54c65155c21d3a73b6811ed7d92c6..d04e650d8f425f29066c723d9e6656bf2e805d0b`
Review date: 2026-09-14
Prior review: `.omo/evidence/ulw/01a09fbd-7249-75a0-a9c6-d4d89cedd373/G001-implement-the-report-v2-payload-cont/a1/final-code-review.md`

This review covers only the B1-B3 delta. Criteria approved in the prior full review remain out of scope, and no regression in those criteria was observed. There are no remaining criterion-cited blockers.

## Scope and State

- The worktree HEAD was exactly `d04e650d8f425f29066c723d9e6656bf2e805d0b`.
- Review inputs were the prior B1-B3 report and the named delta. Source, tests, and docs were not modified by this review.
- Before this report was written, the only tracked dirty file was the pre-existing orchestration state file `.omo/ulw-loop/01a09fbd-7249-75a0-a9c6-d4d89cedd373/spawn-count.json`.
- Build and test execution changed ignored build artifacts only. Temporary B3 probes ran outside the repository and their temporary directories were removed automatically.

## Blocker Resolution

### B1: integrity ordering - RESOLVED

Prior requirement: validate the envelope, verify the exact payload digest, and only then validate or parse the payload JSON.

At `src/integrity/sha256.cpp:20-25`, `verify_v2` now follows the required sequence:

1. `spec::parse_integrity_envelope(document)` performs strict envelope parsing.
2. `sha256_hex(envelope.payload)` is compared with `envelope.digest`.
3. `spec::validate_json(envelope.payload)` runs only after a matching digest.
4. The output payload is assigned only after payload validation succeeds.

`tests/integrity_v2_test.cpp:15-24` adds a payload containing malformed JSON (`{"broken":}`) with a correct SHA-256 digest. The test requires `verify_v2` to fail and leave the output payload empty. The ADR contract and implementation ordering now agree at `docs/adr/0003-report-and-replay-format.md:29-31`.

Black-box ordering limitation: the public `verify_v2` result cannot distinguish digest rejection from malformed-payload rejection. Consequently, the new test proves the correct externally visible result but cannot itself prove which internal comparison executed first. Internal source order is the direct evidence for ordering; the test is the public-result regression.

### B2: empty failure result semantics - RESOLVED

Prior requirement: reject impossible empty step-derived failure results, retain valid empty timeouts, and leave passed-result semantics unchanged.

`src/report/report.cpp:201-203` rejects every non-`passed` result with zero steps unless its status is `timeout`. This covers `mismatch`, `protocol_error`, `adapter_error`, and `crashed`. The pre-existing passed-result branch remains unchanged at `src/report/report.cpp:245-249`: a non-empty flow still requires a complete all-passed prefix.

`tests/cli_replay_v2.cmake:106-111` rejects an empty `mismatch` result. `tests/cli_replay_v2.cmake:141-147` uses an empty `timeout` result and expects verification to succeed before replay independently rejects its missing working directory. This distinguishes the retained empty-timeout case from the newly rejected empty step-derived failures.

### B3: replay output artifact aliases - RESOLVED

Prior requirement: before spawning, reject aliases or existing files on every output artifact so replay cannot overwrite the input report.

`path_exists` at `src/replay/replay.cpp:34-39` uses `symlink_status`, so it detects ordinary files, symlinks to existing files, hard links, and broken symlinks. The preflight loop at `src/replay/replay.cpp:70-77` checks all three write targets: `<run-id>-0.json`, `<run-id>-0.txt`, and `<run-id>-0.v2.json`. These checks complete before `runtime::execute` at `src/replay/replay.cpp:101` and before `report::write` at `src/replay/replay.cpp:105`.

The planned `.v2.json` input-equivalence check remains at `src/replay/replay.cpp:58-68`. For `.v2.json` aliases, that check can reject by inode or symlink resolution before the generic existing-path loop. `tests/cli_replay_v2.cmake:157-176` creates a symlink from an output `.json` path to the input `.v2.json`, requires replay exit code 5 with `output report path already exists`, verifies the input SHA-256 is unchanged, and verifies no `.v2.json` output was created.

An additional temporary adversarial matrix exercised all three suffixes crossed with symlink, hard link, and broken symlink: nine cases total. Every case was rejected with exit code 5 and left the input bytes unchanged. The exact replay shape was:

```sh
build/wise-combine replay TMP/input/clean-0.v2.json \
  --adapter tests/fixtures/bin/adapter_ok \
  --reports TMP/out --run-id alias
```

The probe output ended with `B3_THREE_ARTIFACT_ALIAS_MATRIX=PASS`.

## Verification Commands

All commands ran from the repository root at the reviewed SHA.

| Command | Exit | Observable result |
| --- | ---: | --- |
| `git rev-parse HEAD` | 0 | `d04e650d8f425f29066c723d9e6656bf2e805d0b` |
| `git status --porcelain=v1` | 0 | Only the pre-existing tracked `.omo/ulw-loop/01a09fbd-7249-75a0-a9c6-d4d89cedd373/spawn-count.json` modification |
| `git diff --check 2ec151c..d04e650` | 2 | Five trailing-whitespace findings, all in the two prior review evidence Markdown files; no product source/test/build-file findings |
| `cmake --build build --parallel` | 0 | Debug build completed |
| `(cd build && ctest -R 'integrity_v2\|cli_replay_v2' --output-on-failure)` | 0 | 2/2 passed: `integrity_v2`, `cli_replay_v2` |
| `(cd build-asan && ctest -R 'integrity_v2\|cli_replay_v2' --output-on-failure)` | 0 | 2/2 passed under the sanitizer build |
| `cmake --build build-coverage --target coverage-check --verbose` | 0 | Project source coverage: 84% of 1169 executable lines, above the 80% gate |

Sanitizer and coverage artifacts were already newer than the changed product and test sources. The mandated commands did not request a separate sanitizer configure/build step.

## Notes

1. The `git diff --check` findings are confined to prior review evidence Markdown. Two spaces before a newline are intentional Markdown hard breaks in part of that content, and captured test output also preserves terminal spacing. This is not a B1-B3 functional regression and is therefore non-blocking.
2. The B3 preflight is not a concurrent-claim lock. A path that does not exist during preflight could be created by another process before report writing. The reviewed requirement was preflight rejection before spawn; callers requiring an exclusive output directory should continue to provide one. This residual TOCTOU is a hardening note, not a delta blocker.

## Conclusion

B1, B2, and B3 are fixed at `d04e650d8f425f29066c723d9e6656bf2e805d0b`. The required targeted debug tests, targeted sanitizer tests, and coverage gate pass. No criterion-cited blocker remains.
