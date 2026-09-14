# Oracle and regression matrix

This matrix maps existing regression functions and commands to the current
combination-testing semantics. It is an audit map, not a fresh execution
result. Current release status remains
[`.omx/current-status.md`](../.omx/current-status.md).

## Reading the columns

- **Generator coverage** asks what call/path generation is exercised.
- **Executor fault detection** asks how a known bad callback, model, digest, or
  result is made observable.
- **Stateful oracle coverage** asks whether callback context/report state is
  checked, not merely whether a command exits successfully.
- **Replay determinism** asks whether explicit trace/replay or repeated seeded
  execution is checked.

The repository now also contains a controlled queue-oracle regression. It
provides executor fault detection and stateful oracle evidence, but does not
prove that the generator enumerates all possible models or paths.

## Mapped dimensions

| Dimension | Existing regression / command | Generator coverage | Executor fault detection | Stateful oracle coverage | Replay / repeat determinism |
| --- | --- | --- | --- | --- | --- |
| Cycle rejection | [`tests/test_api.c`](../tests/test_api.c) :: `test_relation_cycle`, `test_relation_reference_dependency`; public `wct_validate_relation` | Explicit relation cycle and implicit `$produce` / `$consume` cycle are constructed; generation stops at validation. | Nonzero validation result and diagnostic containing `cycle`. | Diagnostic identifies invalid graph; no callback is run and no partial flow is reported as success. | Not trace-based: an invalid model has no accepted trace. |
| Flow limit and uncovered subset | `test_state_success_and_limit`, `test_state_branching_and_unreachable`, `test_relation_order_and_failure`, `test_multiple_bounded_relation_flows`; CLI `--mode state` / `--mode relation` | State prefixes, unreachable declared edges, one legal relation flow, and two legal relation flows under `max_flows` are exercised. | Bounded/incomplete runs fail nonzero; report exposes `steps`, `covered`, `uncovered`, `declared_edges`, `covered_edges`, and `uncovered_edges`. | Callback order/count and report counters distinguish progress from complete coverage. | Seeded paths are deterministic where exercised; these cases do not by themselves prove trace/replay completeness. |
| Repeat and tie-break | `test_seed_sampling_determinism`, `test_relation_lexical_tie_break` | A one-source/three-edge branch graph and an independent three-call relation are generated with controlled seeds. | Exact callback order is asserted; an alternate seed is shown to change state sampling order. | Callback context sequences and report edge counters are checked, not just exit status. | Same seed repeats the same state order; independent relation calls use lexical order. Full trace replay is covered separately below. |
| Parameter/result binding | `test_relation_result_binding`, `test_relation_result_contracts`, `test_bare_result_reference_rejected`, `test_relation_arity_and_types`, `test_parser_call_contract`, `test_parser_result_contract` | Producer-to-consumer edges, implicit `$producer` references, typed arguments/results, and exact expected results are generated. | Arity/type parser/runtime failures, bare reference rejection, result type mismatch, and exact-result mismatch fail with step/expected/actual diagnostics. | Report step/edge counters and callback order verify the producer-consumer queue; exact result oracle is checked. | Dependency order is deterministic in the exercised graphs; trace-specific checks are below. |
| Failed-transition atomicity | `test_state_callback_failure`, `test_state_failure_atomic_snapshot`, `test_isolated_state_snapshot_commit`, `test_zero_snapshot_and_rollback_failure` | Successful and intentionally failing transition scenarios include callback failure, bad expected result, timeout, and snapshot/restore failure. | Failed callback/expectation/timeout/restore produces nonzero status, failure counters, or a precise error. | Context count/state is checked before and after: successful state commits; failed scenarios roll back; zero-length snapshots still restore. | Replay is not the atomicity mechanism; isolation and snapshot/restore are tested directly. |
| Branch replay / reset | `test_state_branching_coverage`, `test_seed_sampling_determinism` | Both edges of a branch and multiple same-source transitions are generated; optional `state_reset` is supplied. | Coverage counters and exact callback sequences detect a lost or duplicated branch scenario. | `state_reset` returns the callback fixture to the initial context before each branch scenario; declared/covered/uncovered edge counts are checked. | Same seeded run is repeatable. This is semantic branch replay, not the checksummed trace path. |
| Trace capture and replay determinism | [`tests/test_cli.sh`](../tests/test_cli.sh) :: relation `--trace`, then `--replay`; tampering loops for header/schema, digest, model, mode, seed, limits, counters, metadata, and selection | A valid relation flow is captured and re-executed from the trace's recorded model and selection. | Missing/duplicate fields, malformed numbers, altered edge metadata, changed model digest, trailing singleton data, and garbage are rejected. | Replay compares steps, declared/covered/uncovered edges, exit/process/signal/timeout metadata, selection, and canonical digests before printing `replay=PASS`. | The recomputed step digest must equal the recorded digest; repeated execution and replay are checksum-bound. |

## Queue mutant regression

`make test` runs `tests/test_queue_oracle.sh`. The fixed 84-run matrix executes
six clean Queue cases twice against clean and mutant 1–6 harnesses (84 rows),
then repeats clean/probe checks. The SUT has no assertions; every expected
Queue result is declared with `contract ... result=int expect=...`, so the
product's result contract performs the judgment.

| Measurement | Contract and result |
| --- | --- |
| Generator coverage | The six review cases, non-self cycle, declared subset, and alias repeat are supplied models. `probe-cycle` must be rejected; `probe-subset` and `probe-repeat` must succeed. This is not exhaustive generation. |
| Executor fault detection | Each of mutants 1–6 must detect its mapped case twice. A mutant may detect additional Queue violations; extra detections are not false positives. The clean harness must pass all twelve clean runs. |
| Oracle coverage | The model/result contract compares exact Queue return values. The report must show failures and uncovered calls on mutation detection. |
| Repeat determinism | Every mutant/case pair must produce byte-identical output for both repetitions. |

The mapped defects are: empty pop wrong value (M1/Q1), LIFO instead of FIFO
(M2, also detected by Q5), reopen retaining old state (M3/Q3), peek consuming
(M4/Q4), second pop retaining count (M5/Q5), and close failing to close (M6/Q6).

## Generation-completeness boundary

All six supplied mutants are detected, but that measures executor/oracle fault
detection for supplied flows. It does not prove generator completeness, reveal
all equivalent mutants, or enumerate undeclared parameter domains. New generators
must be measured separately from executor mutation score.
