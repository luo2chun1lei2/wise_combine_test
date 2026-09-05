# F3 Final CLI Manual-QA Audit

Status: **PASS**

Audit date: 2026-09-05 (Asia/Shanghai)  
Repository HEAD audited: `82034fd` (`test(integration): add workflow specifications`)  
Scope: read-only CLI/product tree plus this evidence artifact. No C++,
CMake, README, or plan files were changed.

Preflight state before the long timeout/crash command: `WORKING`.

## Failing-first artifact-presence capture (RED)

Before writing this artifact, the required evidence path was checked:

```text
$ test -e .omo/evidence/wise-combine-test/final-f3.md
artifact_presence_exit=1
```

The non-zero status is the intentional RED capture proving the F3 artifact was
absent before the audit result was recorded.

## Command matrix

All commands were run from the repository root against `./build/wise-combine`.
The exact exit-code contract observed was: validation/generation success `0`,
malformed specification `2`, mismatch `4`, and timeout/crash runtime failure
`5`. The CLI usage probe additionally returned `6`.

### Validate

```text
$ ./build/wise-combine validate tests/fixtures/state_valid.json
EXIT=0
{"valid":true,"version":1,"state_count":2,"transition_count":1,"wall_time_ns":579888,"cpu_time_ns":572353,"peak_rss_bytes":1687552}
```

The JSON contains valid schema counts and non-zero wall/CPU/RSS measurements.

### Generate

```text
$ ./build/wise-combine generate tests/fixtures/relation_valid.json
EXIT=0
{"status":"dead_end","case_count":1,"flows":[{"flow_id":"produce","transitions":["produce","consume"]}],"seed":9,"wall_time_ns":1186307,"cpu_time_ns":1111875,"peak_rss_bytes":1691648}
```

The generated flow preserves the required `produce -> consume` ordering and
reports measurements.

### Successful run

```text
$ ./build/wise-combine run tests/integration/relation_workflow.json --adapter tests/fixtures/bin/adapter_ok --reports /tmp/wise-combine-f3.dXSHIT/reports-ok --run-id success
EXIT=0
{"generation_status":"dead_end","case_count":1,"passed":1,"failed":0,"wall_time_ns":4109132,"cpu_time_ns":1219476,"peak_rss_bytes":3948544}
```

Files produced and inspected:

```text
success-0.json
success-0.txt
success-summary.json
```

`success-0.json`:

```json
{"flow_id":"produce","status":"passed","steps":[{"index":0,"transition":"produce","function":"produce","status":"passed","observed_state":"ready","stderr":"","exit_status":0,"detail":""},{"index":1,"transition":"consume","function":"consume","status":"passed","observed_state":"done","stderr":"","exit_status":0,"detail":""}]}
```

`success-0.txt`:

```text
flow produce: passed
step 0 produce: passed state=ready exit=0 
step 1 consume: passed state=done exit=0 
```

`success-summary.json`:

```json
{"generation_status":"dead_end","case_count":1,"passed":1,"failed":0,"wall_time_ns":4109132,"cpu_time_ns":1219476,"peak_rss_bytes":3948544}
```

### Mismatch run

```text
$ ./build/wise-combine run tests/integration/relation_workflow.json --adapter tests/fixtures/bin/adapter_mismatch --reports /tmp/wise-combine-f3.dXSHIT/reports-mismatch --run-id mismatch
EXIT=4
{"generation_status":"dead_end","case_count":1,"passed":0,"failed":1,"wall_time_ns":1875960,"cpu_time_ns":1070869,"peak_rss_bytes":4022272}
```

Inspected `mismatch-0.json`:

```json
{"flow_id":"produce","status":"mismatch","steps":[{"index":0,"transition":"produce","function":"produce","status":"mismatch","observed_state":"wrong","stderr":"","exit_status":0,"detail":"adapter reported mismatch"}]}
```

Inspected `mismatch-0.txt`:

```text
flow produce: mismatch
step 0 produce: mismatch state=wrong exit=0 adapter reported mismatch
```

The matching `mismatch-summary.json` was also present with
`passed:0`, `failed:1`, and the same wall/CPU/RSS fields.

### Malformed/invalid specification

```text
$ ./build/wise-combine validate tests/fixtures/invalid.json
EXIT=2
STDERR:
spec error: : semantic validation failed: unknown initial state: missing
: semantic validation failed: unknown initial state: missing
```

The invalid document is rejected before execution with the documented parse
error code.

### Timeout fixture

The built fixture deliberately sleeps longer than the two-second step limit:

```text
$ ./build/wise-combine run tests/integration/state_workflow.json --adapter ./build/wise-combine-runtime-fixture --arg timeout --reports /tmp/wise-combine-f3.MZ9tzG/reports-timeout --run-id timeout
EXIT=5
{"generation_status":"case_limit","case_count":1,"passed":0,"failed":1,"wall_time_ns":2051648648,"cpu_time_ns":882614,"peak_rss_bytes":3948544}
```

Inspected files:

```text
timeout-0.json
timeout-0.txt
timeout-summary.json
```

`timeout-0.json` records `status:"timeout"`, `exit_status:-1`, and
`detail:"step timeout"`; the text report says:

```text
flow finish: timeout
step 0 finish: timeout state= exit=-1 step timeout
```

### Crash fixture

```text
$ ./build/wise-combine run tests/integration/state_workflow.json --adapter ./build/wise-combine-runtime-fixture --arg crash --reports /tmp/wise-combine-f3.MZ9tzG/reports-crash --run-id crash
EXIT=5
{"generation_status":"case_limit","case_count":1,"passed":0,"failed":1,"wall_time_ns":2458355,"cpu_time_ns":1026455,"peak_rss_bytes":4083712}
```

`crash-0.json` records `status:"crashed"`, `exit_status:9`, and
`detail:"subprocess exit failure"`; `crash-0.txt` says:

```text
flow finish: crashed
step 0 finish: crashed state= exit=9 subprocess exit failure
```

The matching `crash-summary.json` was present with `passed:0` and `failed:1`.

### Report replay

Using the successful run's generated files:

```text
$ ./build/wise-combine report /tmp/wise-combine-f3.MZ9tzG/reports/replay-0.json
EXIT=0
{"flow_id":"produce","status":"passed","steps":[{"index":0,"transition":"produce","function":"produce","status":"passed","observed_state":"ready","stderr":"","exit_status":0,"detail":""},{"index":1,"transition":"consume","function":"consume","status":"passed","observed_state":"done","stderr":"","exit_status":0,"detail":""}]}

$ ./build/wise-combine report /tmp/wise-combine-f3.MZ9tzG/reports/replay-0.txt
EXIT=0
flow produce: passed
step 0 produce: passed state=ready exit=0 
step 1 consume: passed state=done exit=0 
```

Both JSON and text reports replay unchanged and return success.

## Cleanup receipt

The temporary QA directories were removed with targeted, bounded operations:

```text
$ find "$qa_dir" -depth -type f -delete
$ find "$qa_dir" -depth -type d -empty -delete
TEMP_EXISTS_AFTER_CLEANUP=1
```

The final repository-wide temporary-directory scan was empty:

```text
$ find /tmp -maxdepth 1 -type d -name 'wise-combine-f3.*' -print
# no output
```

Exact-name process scans after timeout/crash and after cleanup were also empty:

```text
$ pgrep -x wise-combine-runtime-fixture || true
$ pgrep -x wise-combine || true
# no output
```

No QA child process or F3 temporary directory remains. The artifact is
non-empty and records PASS.

CURRENT-TREE RE-RUN
Tree: ae7de11 (`d520342`)
validate integration state workflow: exit 0
generate integration relation workflow: exit 0
run relation workflow with `adapter_ok`: exit 0, passed=1, failed=0
run relation workflow with `adapter_mismatch`: exit 4
validate malformed spec: exit 2
timeout/crash runtime fixture assertions: exit 0 (asserted timeout/crashed)
report generated TXT report: exit 0

The current successful report shows `produce` then `consume`; the consumer
fixture has no literal argument and only passes after receiving
`from-producer`. The summary contains case/pass/fail counts and wall/CPU/RSS
measurements. Cleanup: `/tmp/wise-final-f3.emvEKb` was removed and no exact
`wise-combine` or fixture process remained.
