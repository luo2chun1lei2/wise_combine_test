# Combination Test

## 项目 wise_combine_test

下面描述中，“[必须]”是必须实现，“[可选]”是可以选择实现。
如果某个语句没有描述 必须还是可选，那么看它上一级，如果直到最开始都没有说明，那么就是“必须”。

### 需求
组合测试：描述了接口的关系后，可以进行组合测试。
1. [必须] 可以通过描述对象的状态图，围绕着对象的状态迁移用的函数进行测试。
1. [必须] 可以通过描述一组函数和它们之间的关系，比如参数之间传递的关系，调用顺序的关系等，生成各种组合调用流程测试程序是否存在问题。

### 要求

#### 基本要求
- [必须] 程序可以运行在 Linux 系统上，依赖最少的第三方模块。
- [必须] 所有的程序都必须经过测试和测量。
	- [必须] 检查内存泄露和内存越界访问。
- [必须] 提供给用户使用说明。
- [必须] 每进行一步，都需要将本目录下的文件，包括 AI 用的文件，比如 .omo/.agents 等都提交到 git 中，除了编译和测试用的二进制文件等。

#### [可选] 高级要求

- 功能
	- 允许用户最大程度的设定接口的关系。
- 项目管理
	- 记录项目管理所有的步骤信息，可以将管理项目和代码、文档等对应。
	- 记录项目中重大决策。
- 测试
	- 测试用例要让代码覆盖率达到80%。
- 测量
	- 度量使用方法和内存、cpu占用之间的关系。
- 维护
	- 程序提供的运行信息（比如日志）可以记录运行情况，和错误后输出的信息（比如日志），能够准确定位错误发生的场景和原因。


### 补充需求

此章节的下面内容不用读取，仅仅作为开发者的备忘录。

不一定能实现，或者说是一种尝试：
1. [不要实现] 通过编码等方式描述一组函数的调用模式后，通过设置各种参数，查看这个调用模式是否处理了所有的情况，是否有遗漏

---

# PROJECT KNOWLEDGE BASE

**Updated:** 2026-09-12  **Commit:** 8a19d50  **Branch:** layzcodex

## OVERVIEW

`wise_combine_test` is a C++20/CMake Linux combination-testing tool. It models
object state graphs, typed functions, producer-consumer argument flow and
global call ordering, then generates bounded flows and executes them through
an allowlisted external adapter process. Reports are available in JSON and
text form with CLI measurements and stable failure exit codes.

## STRUCTURE

```text
wise_combine_test.lazycodex/
├── AGENTS.md       # project specification and repository guidance
├── CMakeLists.txt  # C++20 build, sanitizer/coverage options, and CTest registration
├── README.md       # English usage instructions
├── README.zh.md    # Chinese usage instructions
├── docs/           # improvement roadmap and future design records
├── src/            # model, spec, generator, runtime, report, and CLI
├── tests/          # unit, runtime, CLI, and integration fixtures
├── .omo/           # plans, ledgers, and verification evidence
├── LICENSE         # project license
└── .gitignore      # C/C++/CMake and test-artifact exclusions
```

`.codegraph/`, `.pytest_cache/`, and build directories are generated state or
ignored artifacts, not source modules.

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Requirements | `AGENTS.md` | Authoritative functional and delivery specification |
| Artifact conventions | `.gitignore` | C/C++/CMake outputs and test caches are ignored |
| License | `LICENSE` | Applies to future source and documentation |
| State/function model | `src/model/` | States, transitions, functions, typed relations, and validation |
| Versioned specification | `src/spec/` | Strict JSON parsing, normalization, and diagnostics |
| Combination generation | `src/generate/` | Seeded bounded deterministic flow generation |
| Safe execution | `src/runtime/` | Adapter protocol, process isolation, timeouts, and limits |
| Reports and CLI | `src/report/`, `src/cli/` | JSON/text reports, measurements, and exit codes |
| Tests and fixtures | `tests/` | Unit, failure matrix, CLI, and integration coverage |
| Usage and improvements | `README*.md`, `docs/` | Synchronized instructions and enhancement roadmap |

## CODE MAP

The CLI entry point is `src/main.cpp`; product layers are separated by the
directories listed above. Use CMake/CTest as the authoritative build and test
surface; CodeGraph state remains generated metadata.

## CONVENTIONS

- Linux is the required runtime platform; keep dependencies minimal.
- Every step must commit repository files, including `.omo/` and `.agents/`;
  compiled and test binaries are the only stated exclusions.
- New implementation must provide reproducible tests and measurements,
  including leak and out-of-bounds checks.
- Preserve `[必须]` versus `[可选]` when mapping requirements to evidence.
- Keep `README.md` and `README.zh.md` synchronized for usage and behavior.
- Treat `docs/improvement-roadmap.md` as the backlog for reviewed enhancements.

## ANTI-PATTERNS (THIS PROJECT)

- Do not implement the memorandum item that encodes call patterns solely to
  vary parameters and check for missing cases; it is explicitly out of scope.
- Do not claim completion without runnable tests and measurements.
- Do not treat `.codegraph/` or `.pytest_cache/` as source or verification.
- Do not add child `AGENTS.md` files until a distinct implementation boundary
  exists.

## COMMANDS

Debug build and tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
(cd build && ctest --output-on-failure)
```

Sanitizer build and tests:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DWISE_COMBINE_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
(cd build-asan && ctest --output-on-failure)
```

CLI usage, Valgrind, coverage, adapter safety, and report commands are
maintained in `README.md` and `README.zh.md`.

Coverage build and threshold check:

```sh
cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug \
  -DWISE_COMBINE_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel
(cd build-coverage && ctest --output-on-failure)
cmake --build build-coverage --target coverage-check
```

## NOTES

This root file is the project-wide guidance. Re-score locations after adding
substantial modules, and create child guidance only for domains with distinct
conventions.
