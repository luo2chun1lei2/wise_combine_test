# Release Readiness

This checklist maps the approved I0-I3 plan to the commands and artifacts used
for a local release review. It is intentionally explicit about features that
are not yet implemented so a passing smoke test is not mistaken for full
iteration acceptance.

## Current Release Status (2026-09-14)

Release status remains **BLOCKED** only for the final independent code and
architecture/product-boundary reviews. Iteration 16 automated gates completed
at source commit `f890b768aca48e37299a9ba2403e10b7b590620f`; see
[`evidence/iter-16/summary.json`](../evidence/iter-16/summary.json) and
[`evidence/iter-16/manifest.json`](../evidence/iter-16/manifest.json). The
current boundary and gates are indexed by
[`.omx/current-status.md`](../.omx/current-status.md),
[`.omx/capability-matrix.md`](../.omx/capability-matrix.md), and
[`.omx/release-blockers.md`](../.omx/release-blockers.md). The regression-to-oracle
map is [`docs/oracle-matrix.md`](oracle-matrix.md).

Release candidates use paired commits: source changes are frozen first, then
evidence and status artifacts are committed while retaining the tested source
hash in `evidence/iter-16/{head.txt,summary.json,manifest.json}`. A candidate
therefore refers to the source/evidence pair, not only the source commit.

The I15 section below is historical evidence for commit
`b8e5d68808ba573bb6e5c13b465ee214a2428043`. It is preserved unchanged and does
not describe the current HEAD or release candidate.

## I16 Automated Gates (2026-09-14)

The I16 manifest binds clean tests, ASan/UBSan and sentinels, Valgrind,
coverage, three state and relation measurements, valid/tampered replay, and the
84-run queue mutant matrix plus three generation probes. Its status is
`AUTOMATED_GATES_PASS`, not a release approval. Coverage remains a measured
boundary: `src/wct.c` at 78.78% lines and 81.49% branches; `tools/wct_cli.c`
at 91.89% lines and 96.37% branches. Six supplied mutants were detected, but
this does not establish generator completeness.

## Current Baseline

The checked-in I0 baseline is reproducible with:

```sh
make clean all
make test
make sanitize
make valgrind
make measure OUT=evidence/iter-0/measure.tsv
```

`make valgrind` records a `SKIP` when Valgrind is not installed. The sanitizer
target remains mandatory. The baseline artifacts currently checked in are
`evidence/iter-0/measure.tsv` and `evidence/iter-0/valgrind.txt`.

## Iteration Gates

| Iteration | Required behavior | Required evidence |
| --- | --- | --- |
| I0 | C11 build, versioned DSL, CLI smoke, C API contract | `evidence/iter-0/` build/test, sanitizer, Valgrind and measurement logs |
| I1 | Reachable state-edge coverage, unreachable-edge reporting, atomic callback failure, bounded cycle semantics | `evidence/iter-1/summary.json`, state logs, sanitizer/Valgrind output, measurement |
| I2 | DAG validation, deterministic tie-breaks, argument/result binding, relation-edge coverage, bounded flow reporting and replayable trace | `evidence/iter-2/summary.json`, relation logs/trace, sanitizer/Valgrind output, measurement |
| I3 | Clean-checkout regression, boundary/fuzz checks, compatibility replay and release manifest | `evidence/iter-3/summary.json`, coverage/replay logs, environment manifest and repeated measurements |

A later gate must not be reported as green until the prior iteration's
`summary.json` has `status: PASS` and all mandatory checks have fresh output.

## Review Notes

The current API/CLI tests cover linear success paths, callback failures,
simple validation errors and fixture parsing. Before an I1-I3 release claim,
add regression cases for:

- null graph/report pointers, allocation failures and malformed/overlong input;
- unreachable and branching state edges, expectation mismatch, cycle limits and
  failed-transition atomicity;
- duplicate relation edges, unknown calls, result-to-argument binding, arity
  and type validation, deterministic tie-breaks and flow-limit `uncovered`
  reporting;
- trace metadata/replay, fork/timeout isolation, DSL boundary/fuzz inputs and
  release manifests with artifact hashes.

The implementation deliberately does not search arbitrary parameter spaces or
infer undeclared relationships; those cases remain outside the product scope.

## User-Facing Verification

From a clean checkout, these commands are the minimum documented workflow:

```sh
make test
./bin/wise-combine-test --model fixtures/state_graph.model --mode state
./bin/wise-combine-test --model fixtures/function_relations.model --mode relation
```

The CLI prints completed `steps`, `covered` items and `failures`; diagnostics
are written to stderr and the process exits non-zero for invalid models or
callback/expectation failures.

## Historical I15 Final Verification (2026-09-11)

`evidence/iter-15/` records historical release verification. Clean test,
ASan/UBSan (including intentional OOB/leak sentinels), Valgrind, coverage,
trace/replay, and three repeated state and relation measurements all pass.
Valid traces replay successfully and tampered traces are rejected. Earlier
blocked iterations remain historical audit records and are not the current
release status.
