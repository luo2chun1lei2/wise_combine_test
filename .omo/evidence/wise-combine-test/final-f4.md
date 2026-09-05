# F4 Scope Audit: Red Gate

Status: **FAIL / BLOCKED**

Audit date: 2026-09-05 (Asia/Shanghai)  
Repository HEAD audited: `82034fd` (`test(integration): add workflow specifications`)  
Scope: product source, tests, CMake, README, and the mandatory parameter-flow
contract. No product code was edited by this audit.

## Required mandatory failure

The mandatory function-relationship requirement is **not implemented end to
end**. The model and generator record an argument relation as a typed
prerequisite, but runtime execution does not transfer the producer's return
value to the consumer:

- `src/runtime/runtime.cpp:26-51` builds each request from the consumer
  transition's static `t.args` only. There is no input for prior step returns.
- `src/runtime/runtime.cpp:121` checks only that the adapter response contains
  a `"returns"` field; it does not parse, retain, or expose any returned value.
- `src/runtime/runtime.cpp:127-135` launches one subprocess per transition and
  passes no accumulated relation bindings to later steps.
- `src/model/model.cpp:130-150` validates the relation's names/types and
  `src/generate/generate.cpp:84-90` converts it to an ordering prerequisite,
  but neither implements value propagation.

The integration fixture masks this defect. In
`tests/integration/relation_workflow.json:1`, the consumer already has the
literal `"args":{"input":"from-producer"}`. The corresponding
`tests/fixtures/bin/adapter_ok:1-7` chooses a state from the function name and
returns `{}`; it never checks that `input` came from the producer. Consequently
the relation workflow passes while a producer return is ignored. This is a
mandatory acceptance failure, not a test-quality-only issue.

**FAIL/BLOCKED verdict:** F4 cannot pass until a runtime test proves a value
returned by the producer is parsed and injected into the consumer request, and
the workflow no longer masks that path with a hard-coded consumer argument.

## Exact scope scan

The implementation and test surface was enumerated before this artifact was
written:

```text
$ rg --files -g '!*.o' -g '!*.out' -g '!build' -g '!dist' | sort
AGENTS.md
CMakeLists.txt
LICENSE
README.md
src/cli/cli.cpp
src/cli/cli.hpp
src/generate/generate.cpp
src/generate/generate.hpp
src/main.cpp
src/model/model.cpp
src/model/model.hpp
src/report/report.cpp
src/report/report.hpp
src/runtime/runtime.cpp
src/runtime/runtime.hpp
src/smoke.cpp
src/spec/spec.cpp
src/spec/spec.hpp
tests/fixture_adapter.cpp
tests/fixtures/invalid.json
tests/fixtures/relation_valid.json
tests/fixtures/state_valid.json
tests/generator_test.cpp
tests/integration/relation_workflow.json
tests/integration/state_workflow.json
tests/model_test.cpp
tests/runtime_test.cpp
tests/smoke_test.cpp
```

The scan confirms that relation, generator, runtime, report, CLI, and
integration layers exist, but no test names or adapter fixture cover producer
return-value propagation.

## Verification baseline

The current Debug build and CTest run completed successfully:

```text
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build --parallel
$ (cd build && ctest --output-on-failure)
Test project .../build
      Start  1: smoke
 1/18 Test  #1: smoke ............................   Passed    0.00 sec
 ...
18/18 Test #18: cli_invalid ......................   Passed    0.00 sec

100% tests passed, 0 tests failed out of 18
Total Test time (real) =   0.27 sec
```

This is a green regression baseline only; it does not clear the red mandatory
scope finding above. The existing relation run also reports `passed:1` because
the fixture's literal argument and empty producer returns avoid exercising the
missing contract.

## Other concrete semantic gaps

1. **Transition argument schema is unchecked.** `src/spec/spec.cpp:48-55`
   accepts arbitrary scalar keys in `transition.args`, and
   `src/model/model.cpp:122-128` validates only transition state/function
   references. Missing required parameters, unknown argument names, and scalar
   types that disagree with declared parameter types are accepted.

2. **Non-self-loop transition reuse is suppressed.**
   `src/generate/generate.cpp:29-32` rejects a transition ID already present in
   a sequence unless it is a self-loop. A bounded state graph cycle such as
   `a -> b -> a` therefore cannot enumerate repeated edge traversals, reducing
   state-transition coverage.

3. **Adapter responses are parsed structurally by substring heuristics.**
   `src/runtime/runtime.cpp:54-70` and `:121` do not parse JSON, reject only a
   literal `"extra"` marker, and do not detect duplicate/unknown fields or
   malformed nested values reliably.

4. **Object state is not persistent across steps.**
   `src/runtime/runtime.cpp:84-102` forks/executes a new adapter for each
   transition. The request carries `flow_id` and `step`, but no persistent
   object/session handle; a stateful adapter must reconstruct state externally.

These are secondary to the parameter-flow failure and are recorded to prevent
the green CTest count from being mistaken for complete semantic coverage.

## Cleanup receipt

The audit created no product/build files and left no long-running processes or
temporary repository artifacts. The only intended repository mutation is this
evidence file. Verification after writing:

```text
$ test -s .omo/evidence/wise-combine-test/final-f4.md
exit=0

$ git diff --check
exit=0
```

Existing ignored `build/` output and pre-existing `.omo/ulw-loop/` state were
preserved. No cleanup command removed user files.

## Final verdict

F4 is **RED**. The project builds and all 18 registered tests pass, but the
mandatory parameter-flow contract is blocked by runtime ignoring producer
returns, and the current relation fixture masks the defect with a hard-coded
consumer argument. Do not promote this audit to PASS without a failing-first
regression test and an implementation that propagates returned values.

## Re-audit after blocker repair

The RED finding above is retained as the failing-first record. Commit `d520342`
added return parsing, per-flow relation binding, the focused
`runtime_argument_relation` test, and removed the hard-coded consumer argument
from both relation fixtures. On current tree `ae7de11`:

- `runtime_argument_relation` passes under Debug and ASan/UBSan/LSan.
- The integration CLI exits 0 only when the adapter sees
  `input=from-producer`; a wrong or static consumer value yields mismatch.
- The forbidden-scope scan exits 1 (no matches) for parameter sweeping,
  GUI/network/distributed/plugin surfaces.
- The source tree contains only the documented model, parser, generator,
  runtime, report, CLI, test, and fixture surfaces.

Broader cycle enumeration, persistent adapter state, and stricter JSON parsing
remain hardening opportunities, not scope violations or blockers for the F4
scope criterion. F4 re-audit: **PASS**.
