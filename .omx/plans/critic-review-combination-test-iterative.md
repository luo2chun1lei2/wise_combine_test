# Critic Review: wise_combine_test iterative plan

## Verdict

**APPROVE** for execution planning handoff, subject to the hard gates below remaining mandatory during implementation.

## Review Basis

- Planner PRD: `.omx/plans/prd-combination-test-iterative.md`
- Planner test specification: `.omx/plans/test-spec-combination-test-iterative.md`
- Architect review: first-cycle review recorded in the Ralplan transcript; it returned `ITERATE` and its P0/P1 findings are incorporated in the `Architect Refinements` section of the test specification.

## Quality Checks

1. **Principle/option consistency:** C11 + Make is consistent with the Linux/minimal-runtime and mandatory memory-safety drivers. The plan constrains the hand-written parser with a versioned schema, canonical IR, strict limits, and sanitizers; Python remains a non-runtime alternative.
2. **Iteration boundaries:** I0 freezes schema, CLI, API/ABI, exit codes, IR, and limits. I1 and I2 add bounded capabilities without reopening the contract. I3 is compatibility/release regression only.
3. **Testability:** Each iteration has concrete commands, fixtures, expected exit behavior, memory checks, measurements, and an evidence directory. A later iteration is blocked unless the prior summary and digest validate.
4. **Coverage semantics:** State tests begin at an initial state and report reachable plus unreachable edges. Relation tests require declared-edge coverage on a normalized DAG and record deterministic tie-breaking and sampling metadata.
5. **Safety and diagnosis:** ASan/UBSan are hard gates with fixed options and OOB/leak sentinels; Valgrind absence is explicit. POSIX child isolation, timeout/reap behavior, stable trace metadata, and canonical replay checks make failures reproducible.
6. **Scope control:** The excluded arbitrary parameter-space omission finder is explicitly out of scope.

## Required Implementation Gate Checks

- Do not start I1, I2, or I3 when the previous `manifest.json` is absent, has a non-PASS status, has a digest mismatch, or lacks commit/push evidence.
- Treat missing sanitizer support, failed sentinel detection, unavailable process isolation, and failed mandatory push as `BLOCKED`; do not downgrade them to warnings.
- Keep Valgrind `SKIP` records explicit and retain sanitizer evidence.
- Preserve the same fixture/seed/limits (or normalized per-step metrics) when comparing measurements, and use the frozen trace schema/hash for replay.

No further plan revision is required before execution. Execution should proceed in I0 -> I3 order with verification and durable evidence after every iteration.
