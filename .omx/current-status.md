# Current release status

**Release: BLOCKED (2026-09-14).**

The only remaining gate is independent approval of the source/evidence pair:
source `41669164a69033a5b51383c26c638f90b2a9e52a` and
[`evidence/iter-18/summary.json`](../evidence/iter-18/summary.json). I18 has
status `AUTOMATED_GATES_PASS`; it is not a release PASS.

## Auditable baseline

- Current release candidate: [`4166916`](https://github.com/luo2chun1lei2/wise_combine_test/commit/41669164a69033a5b51383c26c638f90b2a9e52a)
  (`fix: fail closed review gate edges`); post-fix review pending.
- Fresh evidence: [`evidence/iter-18/manifest.json`](../evidence/iter-18/manifest.json)
  and [`evidence/iter-18/SHA256SUMS`](../evidence/iter-18/SHA256SUMS).
- Historical evidence remains in I15-I17 and must not be substituted for I18.

## Required to re-evaluate

1. Verify I18 `SHA256SUMS` and every manifest artifact hash.
2. Obtain post-fix independent code approval for `4166916`.
3. Obtain post-fix independent architecture/product-boundary approval for the
   `4166916` + I18 pair.
4. Reconcile approval evidence here before changing release status.
