# Current release status

**Release: BLOCKED (2026-09-14).**

The remaining block is explicit: independent code and architecture/product
boundary review is still pending for candidate
`f890b768aca48e37299a9ba2403e10b7b590620f`. Iteration 16 automated gates exist
in [`evidence/iter-16/summary.json`](../evidence/iter-16/summary.json) with
status `AUTOMATED_GATES_PASS`. This status does not inherit the PASS recorded by
iteration 15.

## Auditable baseline

- Current release candidate: [`f890b76`](https://github.com/luo2chun1lei2/wise_combine_test/commit/f890b768aca48e37299a9ba2403e10b7b590620f)
  (`fix: clean coverage compiler side artifacts`); independent review pending.
- Current capability boundary: [`capability-matrix.md`](capability-matrix.md).
- Remaining release gates: [`release-blockers.md`](release-blockers.md).
- Verification procedure: [`docs/release-readiness.md`](../docs/release-readiness.md).
- Fresh automated evidence: [`evidence/iter-16/summary.json`](../evidence/iter-16/summary.json)
  and [`evidence/iter-16/manifest.json`](../evidence/iter-16/manifest.json).
- Historical completed evidence: [`evidence/iter-15/summary.json`](../evidence/iter-15/summary.json).

## Why iteration 15 is historical only

Iteration 15 records `PASS` for source commit
`b8e5d68808ba573bb6e5c13b465ee214a2428043`. It remains a valid audit record.
Subsequent commits through `f890b76` changed the tested system. Iteration 16
now binds those changes to fresh automated evidence, but it does not replace
independent review of that exact commit.

## Required to re-evaluate

1. Verify `evidence/iter-16/SHA256SUMS` and every manifest artifact hash.
2. Obtain and record independent code review for `f890b76`.
3. Obtain and record independent architecture/product-boundary review for
   `f890b76`, explicitly reconciling generator versus executor coverage.
4. Move this page only after those reviews approve the same commit and evidence.
