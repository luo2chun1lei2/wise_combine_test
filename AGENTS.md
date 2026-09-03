# PROJECT KNOWLEDGE BASE

**Generated:** 2026-09-03T12:14:40Z
**Commit:** 8d12e7f
**Branch:** main
**Agent Tool:** LazyCodex(=oh-my-openagent)

## OVERVIEW

`wise_combine_test` is currently a documentation-only repository for describing relationships between interfaces for combination testing. The checkout contains no implementation language, package, test, or build system.

## STRUCTURE

```
wise_combine_test/
├── README.md   # Project purpose, in Chinese
├── LICENSE     # BSD 2-Clause license
└── .gitignore  # Ignore rules
```

`.codegraph/`, `.codex/`, and `.agents/` are local tooling/support directories. They are not project modules and should not receive child guidance files.

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Read project purpose | `README.md` | Only project-level description currently present |
| Check licensing | `LICENSE` | BSD 2-Clause text |
| Check ignored artifacts | `.gitignore` | Repository ignore rules |
| Find implementation entry points | Repository root | None exist in this checkout |
| Inspect code relationships | `.codegraph/codegraph.db` | Schema exists, but index has 0 files, nodes, and edges |

## CODE MAP

No symbols, exports, references, or entry points are available. LSP/codegraph centrality is unmeasured because the repository has no source files and the codegraph index is empty.

## CONVENTIONS

- Keep project documentation in `README.md`; it is currently written in Chinese.
- Preserve the existing BSD 2-Clause license when adding implementation files.
- Keep local codegraph and agent state out of commits; `.codegraph/`, `.codex/`, and `.agents/` are support locations.

## ANTI-PATTERNS (THIS PROJECT)

- Do not document `.codegraph/`, `.codex/`, or `.agents/` as application modules.
- Do not claim a build, test, or executable workflow exists until corresponding files are added.
- Do not treat an empty codegraph database as evidence of absent relationships in a future implementation checkout.

## COMMANDS

No project build, test, lint, or run command is defined in this checkout.

## NOTES

The remote is `origin` (`github.com/luo2chun1lei2/wise_combine_test.git`). If implementation is expected, obtain the intended branch, submodule, or source checkout before adding module-specific guidance.
