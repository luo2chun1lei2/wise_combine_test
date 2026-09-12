# Frozen common experiment Q1–Q6

The six cases in cases.json use exactly the same stateful C queue implementation.
For each Qn, run clean MUTANT=0 and its matching MUTANT=n, twice. Every clean step
must satisfy the declared expected return; the mutant must fail at a real
mismatch. Expected negative returns are successful negative tests, not crashes.

Use each project's model generator and native runner/generated harness with
assertions expressed by its normal API/model. Do not evaluate expected values
inside SUT or dispatch adapter. Thin adapters may translate raw return values
and preserve QueueState between per-step processes using a private state file;
report this explicitly and reset between scenarios. No source repairs to products.

This is a supplied-trigger execution test, NOT proof of automatic discovery or
exhaustive combination coverage. Record generated sequences and whether model
aliases were needed for repeated calls. Independent generation probes cover
non-self cycles, subsets/repeats, limits, constraints and parameter transfer.

Budget: 10 seconds per test process (cleanup any descendants); max 100 cases,
max depth 8, deterministic seed 1 where configurable; 24 runs per project.
Outputs, compile commands, model files, generated harness and result JSON must
remain in each project's dedicated review-assets directory.
