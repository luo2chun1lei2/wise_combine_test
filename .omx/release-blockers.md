# Release blockers

Status: **BLOCKED**. This page tracks gates for the current candidate, not the
historical I15 result. See [`current-status.md`](current-status.md) and
[`docs/release-readiness.md`](../docs/release-readiness.md).

## Closed parser fixes

These fixes are covered by fresh iteration 18 automated evidence. Final
independent review remains required before release.

| Priority / issue | Closing commit | Regression or behavior lock |
| --- | --- | --- |
| P0 — strict DSL and trace numeric-token boundaries | [`df051a1`](https://github.com/luo2chun1lei2/wise_combine_test/commit/df051a14baf43eda1bf57dcf46a0b8a3754ce2a3) (`df051a1`, parser boundaries and runtime hardening) | Numeric/schema boundary handling in the parser and replay path; later CLI/replay regression work is continuing in the current branch. The closed parser portion is bound to this commit. |
| P1 — reject duplicate schema / `state_graph` / `relation_graph` declarations | [`419fd8e`](https://github.com/luo2chun1lei2/wise_combine_test/commit/419fd8e588cfde3dcd7c675741f64a5c0c3be2c3) (`419fd8e`) | [`tests/test_api.c`](../tests/test_api.c) :: `test_parser_strict_and_duplicate_declarations`: rejects unsupported schema, invalid call contract, duplicate schema, duplicate state graph, and duplicate relation graph with line-first diagnostics. |
| P1 — aggregate child-process gcov output rather than silently dropping fork coverage | [`911084b`](https://github.com/luo2chun1lei2/wise_combine_test/commit/911084be6aa2002ada7d1d8b9aae259c7d6bd42f) (`911084b`) | `make coverage` uses per-child `GCOV_PREFIX` directories plus `gcov-tool` aggregation; [`evidence/iter-18/coverage.log`](../evidence/iter-18/coverage.log) and [`coverage/summary.txt`](../coverage/summary.txt) record the measured result. |
| P2 — formalize controlled queue oracle and generation probes | [`41098fb`](https://github.com/luo2chun1lei2/wise_combine_test/commit/41098fbea30cb4720f648e07369110e5da1b58d5) (`41098fb`) | [`tests/test_queue_oracle.sh`](../tests/test_queue_oracle.sh) requires 84 matrix runs, clean 12/12, mapped mutant detection 6/6, repeat equality, and cycle/subset/repeat probe outcomes; I18 `queue-results.tsv` records execution. |

## Completed automated gate

[`evidence/iter-18/`](../evidence/iter-18/) binds the final source candidate and
contains a manifest covering all of the following:

1. Clean build and complete local regression suite.
2. AddressSanitizer/UndefinedBehaviorSanitizer and intentional-fault sentinel
   results.
3. Valgrind memory-error/leak results (or an explicit environment skip only if
   the release policy permits it).
4. Coverage and repeated state/relation measurement artifacts.
5. Valid trace capture/replay plus rejected malformed/tampered replay traces.
6. Artifact hashes and source commit in `evidence/iter-18/summary.json` and
   `manifest.json`.

Post-fix independent review must also be completed for the same frozen commit:

- independent code review;
- independent architecture and product-boundary review;
- explicit reconciliation of the result with
  [`current-status.md`](current-status.md).

No release claim may cite historical I15 PASS as a substitute for these gates.
