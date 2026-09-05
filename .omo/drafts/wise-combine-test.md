---
slug: wise-combine-test
status: awaiting-approval
intent: unclear
review_required: true
plan_path: .omo/plans/wise-combine-test.md
plan_sha256: null
review_round_id: null
pending-action: write and review .omo/plans/wise-combine-test.md
review:
  momus:
    status: pending
    workspace_root: null
    runtime_home: null
    target: .omo/plans/wise-combine-test.md
    round_id: null
    plan_sha256: null
    launch_id: null
    session: null
    result: null
  independent:
    status: pending
    workspace_root: null
    runtime_home: null
    target: .omo/plans/wise-combine-test.md
    round_id: null
    plan_sha256: null
    launch_id: null
    session: null
    result: null
approach: "Plan a standard-library-first C++17 Linux CLI with declarative spec parsing, typed state/relation modeling, deterministic bounded generation, adapter-based execution, structured reporting, tests, sanitizers, measurements, and README documentation."
---

# Draft: wise-combine-test

## Components (topology ledger)
<!-- Lock the SHAPE before depth. One row per top-level component that can succeed or fail independently. -->
<!-- id | outcome (one line) | status: active|deferred | evidence path -->
| model | Typed state graph, function, parameter, and relation domain model | active | `AGENTS.md:8-11` |
| spec | Declarative input format, parsing, normalization, and semantic validation | active | `AGENTS.md:8-11`; no existing schema |
| generate | Deterministic bounded legal transition and call-flow generation | active | `AGENTS.md:8-11`; combinatorial risk from empty repo analysis |
| runtime | Safe adapter boundary to invoke a system under test and observe outcomes | active | `AGENTS.md:10-11`; no existing invocation API |
| report-cli | Linux CLI, traces, diagnostics, and user-facing result reports | active | `AGENTS.md:16-20`; `AGENTS.md:77-79` |
| verify | Unit/integration tests, memory safety, measurements, and documentation | active | `AGENTS.md:16-34` |

## Open assumptions (announced defaults)
<!-- Intent is UNCLEAR: research resolves ambiguity, defaults are adopted (not asked), and each is surfaced in the plan's human TL;DR for veto. -->
<!-- assumption | adopted default | rationale | reversible? -->
| implementation language | C++17 with the standard library first | `.gitignore` contains C/C++/CMake artifact conventions; satisfies Linux/minimal-dependency requirement | yes |
| build system | CMake with a small `src/` and `tests/` layout | reproducible Linux build and existing ignore rules | yes |
| input format | Versioned JSON-like declarative schema implemented without runtime third-party dependency (or a vendored narrow parser only if required) | separates user specification from execution and keeps dependency count low | yes |
| invocation contract | Explicit adapter/subprocess boundary, never arbitrary dynamic calls by default | makes external function execution auditable and safer | yes |
| generation policy | Seeded deterministic generation with maximum path length and case count; report bounds rather than promise exhaustiveness | controls combinatorial explosion while preserving reproducibility | yes |
| verification tools | Compiler sanitizers as mandatory evidence; Valgrind as supplementary when installed; CTest/coverage commands documented | directly addresses leak/out-of-bounds requirement | yes |
| optional goals | Plan hooks for logging, CPU/memory measurement, decision records, and 80% coverage, without making them prerequisites for core architecture | keeps all stated optional requirements visible without inventing extra product scope | yes |

## Findings (cited - path:lines)

- The only tracked project files are `AGENTS.md`, `.gitignore`, and `LICENSE`; there are no source symbols, entry points, tests, build files, CI workflows, or README in `HEAD` (`git ls-files`; `AGENTS.md:52-68`).
- Mandatory behavior is state-graph testing and function-relation call-flow generation (`AGENTS.md:8-11`).
- Mandatory delivery gates are Linux support, minimal dependencies, test/measurement, leak and out-of-bounds checks, user instructions, and per-step Git commits (`AGENTS.md:15-20`).
- Optional scope includes configurable relations, project/decision records, 80% coverage, CPU/memory measurement, and diagnostic logging (`AGENTS.md:22-34`).
- Encoding call patterns solely to vary parameters and find omissions is explicitly forbidden (`AGENTS.md:37-42`).
- `.gitignore` indicates C/C++/CMake output conventions but is not an existing build (`.gitignore:1-69`).

## Decisions (with rationale)

- Treat this as an architecture/bootstrap plan, not an MVP: the full mandatory request must be covered end to end.
- Use a layered design (`model`, `spec`, `generate`, `runtime`, `report/cli`) so parsing, generation, execution, and evidence remain independently testable.
- Make the generated sequence, normalized specification, seed, limits, tool version, and observed failures part of the report artifact to prove reproducibility.
- Model argument flow and call ordering as named declarative relations; reject unknown symbols, duplicate transitions, invalid edges, and impossible order constraints before execution.
- Distinguish expected model state from observed SUT state and report divergence with the exact sequence and step index.

## Scope IN

- Mandatory state-graph and function-relation description, validation, deterministic combination generation, execution through an explicit adapter, and actionable reports.
- Linux build/run workflow with minimal dependencies, functional tests, measurements, sanitizer evidence, and user documentation.
- Optional requirements as documented extension points and verification targets where they do not compromise the mandatory path.

## Scope OUT (Must NOT have)

- The explicitly forbidden parameter-sweeping/call-pattern completeness checker from the memorandum.
- Arbitrary unsafe in-process symbol invocation as the default adapter.
- Network service, GUI, plugin marketplace, distributed execution, or third-party test framework unless later requirements add them.
- Claims of exhaustive combination coverage when configured bounds make generation partial.

## Open questions

None blocking: the remaining technical choices are recorded as reversible defaults above; the approval gate is the owner checkpoint.

## Approval gate
status: awaiting-approval
approach: "Implement the full mandatory path as a standard-library-first C++17/CMake Linux CLI, with explicit schema, adapter, deterministic bounds, evidence artifacts, and test/safety gates."
next-action: "After explicit approval, generate .omo/plans/wise-combine-test.md, run mandatory Metis gap analysis, append decision-complete todos, then run the required dual high-accuracy review before handoff."
<!-- When exploration is exhausted and unknowns are answered, set status: awaiting-approval. -->
<!-- That durable record is the loop guard: on a later turn read it and resume at the gate instead of re-running exploration. -->
