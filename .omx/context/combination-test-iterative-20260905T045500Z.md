# Ralplan Context: Iterative Combination Test Implementation

## Task statement

按照逐步迭代的方式实现 wise_combine_test 需求；每次迭代都必须有可重复的验证结果。

## Desired outcome

在 Linux 上交付一个依赖尽可能少的组合测试程序，至少支持：

1. 通过对象状态图描述状态迁移，并围绕迁移函数生成测试。
2. 通过函数集合及其参数传递、调用顺序等关系，生成组合调用流程并执行测试。
3. 对实现进行功能测试、内存泄露/越界检查和资源测量。
4. 提供用户使用说明，并保留每轮迭代的验证证据。

## Known facts / evidence

- `AGENTS.md:10-22` defines the two mandatory combination-test capabilities, Linux support, minimal dependencies, testing/measurement, memory safety checks, and user documentation.
- `AGENTS.md:24-36` lists optional advanced capabilities such as richer relation configuration, 80% coverage, resource measurement, and diagnostic logs.
- `AGENTS.md:39-44` explicitly excludes the exploratory feature of encoding arbitrary call patterns and parameter settings to find all omissions.
- The repository currently contains only `AGENTS.md`, `omx.md`, `LICENSE`, and `c.md`; no implementation, test runner, build files, or dependency manifest exists.
- Existing worktree state contains unrelated deleted paths `a.md` and `b.md`; they are outside this task and must remain untouched.

## Constraints

- Plan first; do not edit product code during Ralplan.
- Every implementation iteration must have a concrete verification command and recorded result before the next iteration starts.
- Linux is the required host platform.
- Keep third-party dependencies minimal and prefer the standard toolchain.
- Memory leak and out-of-bounds checks are mandatory, not optional.
- The excluded arbitrary parameter-space omission finder must not be implemented as scope creep.

## Unknowns / decisions to resolve in planning

- Implementation language and minimum compiler/toolchain target.
- Input/schema format for state graphs and function relations.
- Runtime/API shape for registering transitions and callable functions.
- Generation strategy and deterministic/randomized test controls.
- Test harness and memory-safety tooling available on target Linux environments.
- Measurement methodology and artifact format for CPU/memory results.
- Logging and failure-report format sufficient to identify the scenario and cause.
- Iteration boundaries and acceptance criteria for each milestone.

## Likely touchpoints

- New source and public API modules for model parsing, relation validation, generation, and execution.
- Unit/integration/system test directories and deterministic fixtures.
- Build configuration and CI/local test scripts.
- User documentation and iteration evidence under a documented artifact directory.

## Iteration rule

Each plan story must end with: implementation evidence, a focused verification command, expected pass/fail signals, memory-safety evidence where applicable, and a recorded artifact path. A later story cannot begin until the prior story's verification is green or an explicit bounded blocker is recorded.
