# Final Code Review: Report v2 Payload and Replay

## Verdict

**BLOCK**

Reviewed SHA: `2ec151c08df54c65155c21d3a73b6811ed7d92c6`  
Comparison base: `e1d53a6`  
Review date: 2026-09-14  
Report artifact: `.omo/evidence/ulw/01a09fbd-7249-75a0-a9c6-d4d89cedd373/G001-implement-the-report-v2-payload-cont/a1/final-code-review.md`

The implementation is substantially complete and the required build/tests pass, but three success-criteria violations block approval. In particular, the current code can overwrite the input report through an output-path alias, and two integrity/semantic-validation requirements are not fully enforced.

## Scope

This was an executable review of frozen HEAD, emphasizing `git diff e1d53a6..HEAD -- CMakeLists.txt src tests README.md README.zh.md docs`. Source, tests, docs, and project state were not modified. Build/test commands were run as requested. The only file created by this review is the report named above; temporary adversarial probes used `tempfile.TemporaryDirectory` outside the repository and were removed automatically.

Out of scope as directed: stable report ID, truncation marker, external adapter state restoration, advanced constraints, and origin authentication.

## Verification Performed

| Command | Observable result |
| --- | --- |
| `git rev-parse HEAD` | `2ec151c08df54c65155c21d3a73b6811ed7d92c6` |
| `git status --short` | Two pre-existing dirty `.omo/ulw-loop` state files; no dirty source, test, or doc files |
| `git diff --check e1d53a6..HEAD` | Exit 0; no output |
| `cmake --build build --parallel` | Exit 0 |
| `(cd build && ctest -R 'cli_replay_v2|cli_verify_report_v2|integrity_v2' --output-on-failure)` | Exit 0; 3/3 passed (`integrity_v2`, `cli_verify_report_v2`, `cli_replay_v2`) |
| `(cd build && ctest --output-on-failure)` | Exit 0; 45/45 passed |
| `cmake --build build-asan --parallel` | Exit 0 |
| `(cd build-asan && ctest --output-on-failure)` | Exit 0; 45/45 passed, including sanitizer/leak checks |
| `cmake --build build-coverage --target coverage-check` | Exit 0; project source coverage 84% against the 80% gate |

The committed RED evidence exists at
`.omo/evidence/ulw/01a09fbd-7249-75a0-a9c6-d4d89cedd373/G001-implement-the-report-v2-payload-cont/a1/implementation-red.log`
and shows `cli_replay_v2` failing because `clean-0.v2.json` did not exist before implementation. A second RED artifact, `replay-relative-path-red.log`, shows the relative-adapter replay path failing before the fix.

## Blockers

### B1. Payload JSON is parsed before its digest is verified

**Criterion:** integrity must be checked before payload validation. ADR 0003 explicitly requires SHA-256 validation before payload parsing (`docs/adr/0003-report-and-replay-format.md:29-30`).

**Finding:** `verify_v2` parses and validates the decoded payload string before comparing its digest:

- `src/integrity/sha256.cpp:20-22` — after envelope parsing, it calls `spec::validate_json(envelope.payload)` before `sha256_hex(envelope.payload) != envelope.digest`.
- By contrast, the CLI-level semantic check is correctly sequenced after `integrity::verify_v2` at `src/cli/cli.cpp:270-281`, but this does not restore the required order inside the integrity primitive.

This lets malformed payload bytes with a matching digest enter the JSON parser. The behavior is not currently distinguishable in the CLI because all failures are collapsed into “integrity verification failed,” but it violates the explicit ordering contract and makes the primitive unsuitable as the boundary required by the ADR.

**Required fix:** perform strict envelope-shape validation, compute and compare SHA-256 over the exact decoded payload bytes, and only then validate/parse that payload as JSON. Update tests to observe the ordering boundary directly, not only through identical generic failures.

### B2. Semantic result validation accepts impossible empty failure results

**Criterion:** model, flow, and result must undergo semantic validation.

**Finding:** `verify_result` only applies final-step consistency checks when the result contains at least one step:

- `src/report/report.cpp:242-246` correctly requires a complete all-passed result for `status: passed`.
- `src/report/report.cpp:247-254` guards the failure checks with `else if (!result.steps.empty())`; therefore an empty result can declare any failure status.

Runtime cannot produce `mismatch`, `protocol_error`, `adapter_error`, or `crashed` with zero steps: those statuses arise only from a step (`src/runtime/runtime.cpp:153-166`), and each completed/failed step is appended before the result returns (`src/runtime/runtime.cpp:194`). A zero-step `timeout` is possible at the total-timeout boundary (`src/runtime/runtime.cpp:181-182`), but an empty `mismatch` is not.

Reproduced at the CLI with a valid model/adapter and a payload containing `"result":{"status":"mismatch","flow_id":"produce","steps":[]}`. `verify-report-v2` returned:

```text
exit=0
stdout={"valid":true,"integrity_verified":true,"payload_valid":true,"schema_version":2}
```

The related exemption at `src/report/report.cpp:249-253` also permits `result.status == timeout` with an arbitrary final step status. Runtime sets the result status directly from the final failed step (`src/runtime/runtime.cpp:194`), except for a timeout caused before beginning another step; semantic validation should represent that state machine precisely.

**Required fix:** validate every status against the executed-prefix state machine. At minimum, reject impossible empty failure statuses and ensure `timeout` result/final-step combinations match runtime behavior. Add both positive and failing-first regression cases.

### B3. Replay can overwrite the input report through a different output path

**Criterion:** replay must not overwrite the input report; preflight must prevent unsafe output paths.

**Finding:** collision detection considers only two cases:

- input directory equals output directory (`src/replay/replay.cpp:50`);
- the planned `.v2.json` path is equivalent to the input report (`src/replay/replay.cpp:52-55`).

It does not check the `.json` and `.txt` paths that `report::write` also opens and truncates before writing `.v2.json` (`src/report/report.cpp:401-405`). Consequently, an output `.json` path in a different directory can be a symlink or hard link to the input `.v2.json`, bypass the planned `.v2.json` comparison, and mutate the input.

Reproduced with the built CLI:

1. Generated a clean report at `<tmp>/input/clean-0.v2.json`.
2. Created `<tmp>/output/alias-0.json` as a symlink to that input report.
3. Ran replay with `--reports <tmp>/output --run-id alias`.

Observed:

```text
replay_exit=0
input_bytes_before=2175
input_bytes_after=437
input_unchanged=false
RESULT=OVERWROTE
```

The input SHA-256 changed from `a4746f899e26d9e60d057172f4ba33d7233de6f5ba99febcf7fed909c71f4272` to `bece01276d178a5cffb37629ca1ce426531be6a712a30401a26b1fd066a478b6`. Existing tests cover only same-directory collision and equivalence of the planned v2 path (`tests/cli_replay_v2.cmake:149-157`), so they do not cover all three output artifacts or alias links.

**Required fix:** before spawning or writing, resolve and safety-check every planned output artifact (`<run-id>-0.json`, `<run-id>-0.txt`, and `<run-id>-0.v2.json`) against the canonical input report and its aliases. Use creation-safe/open-with-`O_EXCL`-style writing or explicitly require a new output directory. Add symlink, hard-link, and partial-output failure regression tests. Synchronize ADR/README wording after selecting the policy.

## Non-Blocking Notes

1. **TOCTOU around adapter validation** — Replay checks the explicit adapter allowlist and digest at `src/replay/replay.cpp:68-77`, but the child later opens/executes by path at `src/runtime/runtime.cpp:119-127`. A concurrent replacement between those operations can invalidate the checked digest. This is outside the reviewed criteria as stated, but an open descriptor or equivalent atomic validation would make the safety guarantee stronger.
2. **Allowlist path matching is substring-based** — `allowed_executable` accepts any executable whose canonical path contains `/tests/fixtures/bin/` and whose name starts appropriately (`src/runtime/runtime.cpp:101-109`). This is pre-existing behavior and digest matching limits accidental misuse, but anchoring the path to the project fixture root would be more robust.
3. **Documentation wording differs slightly** — ADR says `--reports NEW_DIR` (`docs/adr/0003-report-and-replay-format.md:33`), while README accepts an existing `DIR` subject to input-report protection (`README.md:90-95`; `README.zh.md:79-83`). The current implementation matches README more closely than ADR, but B3’s selected policy should make these statements exact in both languages.
4. **Pre-existing dirty AI state** — At review start, `git status --short` reported modified `.omo/ulw-loop/01a09fbd-7249-75a0-a9c6-d4d89cedd373/spawn-count.json` and `.omo/ulw-loop/ulw-20260914-195325.R5EdTB.md`. They are outside the reviewed source diff and do not alter the frozen source/tests/docs being reviewed.
5. **LSP diagnostics were not a usable gate** — The configured clang LSP emitted missing-include/C++17 diagnostics because it lacked this CMake project’s include/C++20 configuration. CMake Debug and ASan builds were clean, so this was treated as tooling configuration, not a product defect.

## Criteria Assessment

| Criterion | Assessment |
| --- | --- |
| Integrity before payload validation | **Blocked by B1** |
| Strict exact keys/types | Implemented for envelope and payload; parser also rejects duplicate keys |
| Semantic model/flow/result validation | **Blocked by B2**; model/flow checks are otherwise present |
| Explicit allowlisted adapter digest match | Implemented at `src/replay/replay.cpp:68-77`; see TOCTOU note |
| Pre-spawn safety checks | Mostly implemented; working directory, allowlist, and digest precede execution (`src/replay/replay.cpp:62-77`) |
| Saved-flow-only execution | Implemented; replay parses the saved model and executes `metadata.flow` (`src/replay/replay.cpp:83-85`) |
| No input overwrite | **Blocked by B3** |
| Fixed environment | Payload requires exactly `PATH=/usr/bin:/bin`, `LC_ALL=C` (`src/report/report.cpp:357-366`); child uses the same fixed environment (`src/runtime/runtime.cpp:123-127`) |
| Bilingual docs | Present and broadly synchronized; clarify output-dir wording with B3 |
| Existing exit codes | Preserved: 2 invalid/report-parse, 4 observed mismatch, 5 runtime/safety failure (`src/cli/cli.cpp:297-303`) |
| Failing-first RED evidence | Present in `implementation-red.log`; focused relative-path RED also present |

## Exact Evidence Commands

Requested verification:

```sh
git rev-parse HEAD
git status --short
git diff --check e1d53a6..HEAD
cmake --build build --parallel
(cd build && ctest -R 'cli_replay_v2|cli_verify_report_v2|integrity_v2' --output-on-failure)
```

Supplemental regression and memory/coverage evidence:

```sh
(cd build && ctest --output-on-failure)
cmake --build build-asan --parallel
(cd build-asan && ctest --output-on-failure)
cmake --build build-coverage --target coverage-check
```

RED evidence inspection:

```sh
cat .omo/evidence/ulw/01a09fbd-7249-75a0-a9c6-d4d89cedd373/G001-implement-the-report-v2-payload-cont/a1/implementation-red.log
cat .omo/evidence/ulw/01a09fbd-7249-75a0-a9c6-d4d89cedd373/G001-implement-the-report-v2-payload-cont/a1/replay-relative-path-red.log
```

Source/order inspection:

```sh
nl -ba src/integrity/sha256.cpp | sed -n '17,23p'
nl -ba src/report/report.cpp | sed -n '197,255p'
nl -ba src/report/report.cpp | sed -n '308,396p'
nl -ba src/report/report.cpp | sed -n '398,411p'
nl -ba src/replay/replay.cpp | sed -n '35,105p'
nl -ba src/runtime/runtime.cpp | sed -n '101,198p'
nl -ba docs/adr/0003-report-and-replay-format.md | sed -n '27,45p'
```

### B2 semantic probe command

Run from repository root. The decisive observable is `RESULT=ACCEPTED`; correct semantic validation should produce `RESULT=REJECTED`.

```sh
python3 - <<'PY'
import hashlib, json, pathlib, subprocess, tempfile
repo = pathlib.Path.cwd()
cli = (repo / 'build/wise-combine').resolve()
spec = (repo / 'tests/integration/relation_workflow.json').read_text()
adapter = (repo / 'tests/fixtures/bin/adapter_ok').resolve()
with tempfile.TemporaryDirectory(prefix='wct-review-') as td:
    root = pathlib.Path(td)
    payload = {
        'model': json.loads(spec),
        'generator': {'strategy':'seeded-dfs-v1','termination_status':'dead_end'},
        'flow': {'flow_id':'produce','transition_ids':['produce','consume']},
        'adapter': {'path':str(adapter), 'sha256':hashlib.sha256(adapter.read_bytes()).hexdigest(),
                    'arguments':[], 'working_directory':str(root)},
        'runtime': {'step_timeout_ms':2000,'total_timeout_ms':30000,
                    'output_limit_bytes':16777216,
                    'environment':['PATH=/usr/bin:/bin','LC_ALL=C']},
        'result': {'status':'mismatch','flow_id':'produce','steps':[]},
    }
    raw = json.dumps(payload, separators=(',',':'), ensure_ascii=False)
    envelope = {'schema_version':2, 'payload':raw,
                'integrity':{'algorithm':'sha256',
                             'digest':hashlib.sha256(raw.encode()).hexdigest()}}
    report = root/'empty-mismatch.v2.json'
    report.write_text(json.dumps(envelope, separators=(',',':')))
    proc = subprocess.run([str(cli), 'verify-report-v2', str(report)],
                          text=True, capture_output=True)
    print(f'exit={proc.returncode} stdout={proc.stdout!r} stderr={proc.stderr!r}')
    print('RESULT=' + ('ACCEPTED' if proc.returncode == 0 else 'REJECTED'))
PY
```

### B3 overwrite probe command

Run from repository root. The decisive observable is `RESULT=UNCHANGED`; HEAD produced `RESULT=OVERWROTE`.

```sh
python3 - <<'PY'
import hashlib, pathlib, subprocess, tempfile
repo = pathlib.Path.cwd()
cli = (repo/'build/wise-combine').resolve()
spec = (repo/'tests/integration/relation_workflow.json').resolve()
adapter = (repo/'tests/fixtures/bin/adapter_ok').resolve()
with tempfile.TemporaryDirectory(prefix='wct-overwrite-') as td:
    root=pathlib.Path(td); input_dir=root/'input'; output_dir=root/'output'
    input_dir.mkdir(); output_dir.mkdir()
    run=subprocess.run([str(cli),'run',str(spec),'--adapter',str(adapter),
                        '--reports',str(input_dir),'--run-id','clean'],
                       capture_output=True, text=True)
    assert run.returncode == 0, run.stderr
    input_report=input_dir/'clean-0.v2.json'
    before=input_report.read_bytes()
    before_hash=hashlib.sha256(before).hexdigest()
    (output_dir/'alias-0.json').symlink_to(input_report)
    replay=subprocess.run([str(cli),'replay',str(input_report),'--adapter',str(adapter),
                           '--reports',str(output_dir),'--run-id','alias'],
                          capture_output=True, text=True)
    after=input_report.read_bytes()
    print(f'replay_exit={replay.returncode} stderr={replay.stderr!r}')
    print(f'input_sha_before={before_hash}')
    print(f'input_sha_after={hashlib.sha256(after).hexdigest()}')
    print('RESULT=' + ('UNCHANGED' if before == after else 'OVERWROTE'))
PY
```

## Final Review Conclusion

Do not approve at this SHA. Fix B1-B3, rerun the requested build/test matrix plus ASan and coverage at the new commit, and obtain a fresh final review bound to that new SHA. The current passing tests demonstrate useful coverage but are insufficient because they do not observe the integrity ordering boundary, impossible empty failure states, or output-artifact aliases.
