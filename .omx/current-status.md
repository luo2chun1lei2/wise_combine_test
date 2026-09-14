# Current release status

**Release: CONDITIONALLY PASS (2026-09-14).**

Both independent reviews approved the source/evidence pair:
source `41669164a69033a5b51383c26c638f90b2a9e52a` and
[`evidence/iter-18/summary.json`](../evidence/iter-18/summary.json). I18 has
status `AUTOMATED_GATES_PASS`; review reconciliation is recorded in
[`reviews/iter-18-final-review.md`](reviews/iter-18-final-review.md).

## Auditable baseline

- Current release candidate: [`4166916`](https://github.com/luo2chun1lei2/wise_combine_test/commit/41669164a69033a5b51383c26c638f90b2a9e52a)
  (`fix: fail closed review gate edges`); independently approved.
- Fresh evidence: [`evidence/iter-18/manifest.json`](../evidence/iter-18/manifest.json)
  and [`evidence/iter-18/SHA256SUMS`](../evidence/iter-18/SHA256SUMS).
- Historical evidence remains in I15-I17 and must not be substituted for I18.

## Required to re-evaluate

Completed gates: manifest/hash verification, independent code approval, and
independent architecture/product-boundary approval. Future source changes require
a fresh evidence iteration and review.
