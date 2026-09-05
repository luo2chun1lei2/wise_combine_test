# Deep Interview Specification: wise_combine_test

## Metadata

- Profile: standard
- Context type: greenfield
- Rounds: 6
- Final ambiguity: 0.08
- Threshold: 0.20
- Context snapshot: `.omx/context/combination-test-iterative-20260905T045500Z.md`
- Existing plan references: `.omx/plans/prd-combination-test-iterative.md`, `.omx/plans/test-spec-combination-test-iterative.md`

## Clarity Breakdown

| Dimension | Score | Evidence |
|---|---:|---|
| Intent | 0.95 | Need a usable Linux combination-test tool matching both mandatory AGENTS capabilities |
| Outcome | 0.90 | C API plus first-class DSL/CLI and standalone example runner |
| Scope | 0.95 | State graphs and function-relation flows in four gated iterations |
| Constraints | 0.90 | C11, GCC/Clang, minimal runtime dependencies, strict memory checks |
| Success criteria | 0.85 | Per-iteration tests, sanitizer evidence, measurements, manifests, docs, commit/push |

## Intent

Create a practical combination-testing tool that lets users describe object state transitions and relationships among callable functions, then execute reproducible bounded workflows with diagnosable failures.

## Desired Outcome

Deliver a Linux tool whose stable first-release surface includes a C API, a versioned DSL/CLI, and a standalone example C runner. The implementation advances through I0-I3 only when the current iteration's functional, memory-safety, measurement, evidence, and git-delivery gates pass.

## In Scope

- Object state graphs, migration callbacks, reachable-edge coverage, state assertions, bounded cycles, and isolated scenarios.
- Function nodes, parameter binding, prerequisite ordering, DAG validation, bounded deterministic flow generation, and replayable traces.
- Versioned DSL/CLI and equivalent C API using one canonical IR.
- GCC/Clang C11 build, Make-based test targets, ASan/UBSan, Valgrind when available, CPU/RSS measurement, structured failure logs, README usage.
- Standalone example C test runner and fixtures.

## Out of Scope / Non-goals

- Arbitrary parameter-space enumeration intended to discover unmodeled omissions.
- External test-framework adapters in the first release.
- Unbounded generation, implicit relationships, or claims of exhaustive coverage outside declared state/relation edges and configured limits.

## Decision Boundaries

- OMX may choose internal module layout, naming, deterministic tie-break implementation, and evidence file layout as long as the frozen API/schema and acceptance criteria remain intact.
- OMX may select a small text DSL representation and canonical serialization, but must document and test it before I0 passes.
- Any change to the public C API, schema version, required toolchain, memory-safety policy, or in/out-of-scope features requires renewed user confirmation.

## Constraints

- Linux target; C11; common GCC or Clang compiler is sufficient for the baseline.
- Prefer standard C/POSIX and Make; no required third-party runtime dependency.
- ASan/UBSan are mandatory gates; Valgrind is run when installed and otherwise recorded explicitly.
- Each iteration must record commands, exit codes, environment/tool versions, artifact hashes, commit and push status, and must block the next iteration on failure.

## Testable Acceptance Criteria

1. A state-graph fixture executes every reachable declared transition and reports unreachable edges, callback failures, expected/actual state, scenario ID, seed, and step.
2. A function-relation fixture passes typed parameters and prior results in deterministic dependency order, rejects cycles/unknown nodes/arity or type mismatches, and emits a replayable trace.
3. GCC and Clang C11 builds pass with strict warnings; the standalone example runner executes on Linux.
4. Every iteration has green functional tests, mandatory sanitizer checks including OOB/leak sentinels, measurements, and a manifest bound to the prior iteration digest.
5. README examples and replay commands work from a clean checkout; no acceptance criterion promises discovery of undeclared parameter-space omissions.

## Assumptions Exposed and Resolved

- “API-first” does not mean CLI-only or DSL-only: the user resolved the apparent conflict in favor of both C API and DSL/CLI first-class support.
- Without an external framework, a checked-in example runner is the user-facing verification path.
- GCC/Clang compatibility is preferred over a single compiler or a broad distro matrix for I0.

## Scenario Pressure Findings

- If a user chooses C API first but no CLI exists, the example runner must still make a fixture executable; this is now required.
- If a later answer omits DSL/CLI from non-goals, the explicit Round 5 decision governs and keeps both interfaces in scope.

## Docs / Terminology Ledger

- Governing terms: “state graph”, “state migration”, “function relationship/call order”, “memory leak/out-of-bounds”, and the explicit excluded “parameter-space omission” exploration from `AGENTS.md`.
- “API-first” is treated as delivery priority, not permission to omit the DSL/CLI acceptance surface.
- No code/doc contradiction was found; the existing PRD was narrower than the final interview decision and must be updated during execution planning.

## Handoff Options

- `$ralplan` is recommended next to reconcile the updated first-class DSL/CLI decision with the existing plan and test matrix.
- `$ultragoal` is the default durable execution lane after planning updates.
- `$autopilot` is suitable for supervised planning plus execution; `$team` is appropriate only if implementation is split into independent lanes.
- `$ralph` remains an explicit fallback for a single-owner persistence loop.
