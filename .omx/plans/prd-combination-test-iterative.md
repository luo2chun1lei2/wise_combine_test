# PRD: wise_combine_test 迭代式组合测试工具

## Outcome

在 Linux 上交付一个可重复、低依赖的组合测试工具。用户能够以文件或 C API 描述对象状态图、状态迁移函数、函数节点及其参数/调用顺序关系，工具生成并执行确定性的组合流程，输出可定位的失败场景；每个迭代必须先通过本轮验证，才允许进入下一轮。

## Scope and Non-goals

### In scope (mandatory)

- 状态图模型：状态、迁移、迁移输入/期望结果、被测迁移回调；生成覆盖声明迁移的测试流程。
- 函数关系模型：函数节点、参数来源/绑定、前置调用顺序和结果传递；生成合法的组合调用流程并执行。
- Linux 构建与运行，优先使用 C11、POSIX/标准库和 Make，不引入运行时第三方依赖。
- 功能测试、CPU/峰值内存测量、AddressSanitizer/UndefinedBehaviorSanitizer 与 Valgrind（可用时）检查越界、UAF、泄漏。
- 用户说明、失败日志（场景 ID、步骤、输入摘要、实际/期望、错误原因）和每轮验证证据。

### Explicit non-goal

不实现“编码任意调用模式并枚举参数空间以发现遗漏”的探索器；只执行用户声明的状态/关系模型和受控生成策略。

## Default Technical Direction

- **语言/工具链：** C11，`cc`/GCC/Clang，Makefile；通过编译开关启用 `-Wall -Wextra -Werror -g` 及 sanitizer。
- **输入：** I0 冻结版本化文本 schema、CLI flags 和 C API（包括语法版本、退出码、错误 JSON）；后续迭代只能向后兼容。解析结果先转为 canonical IR，再由状态/关系执行器消费。IR 明确定义值类型（整数、布尔、字符串、字节串、引用）、所有权/释放责任和每节点/流程/输入/输出字节及深度/数量 limits；拒绝未知字段、重复 ID、未定义引用和约束冲突并带行列号。
- **执行：** 统一模型校验器、确定性 seed、步数/流程数上限、超时/失败隔离；每个场景在 POSIX `fork` 子进程中执行，父进程用 `waitpid` + 单调时钟强制超时并记录 signal/exit，防止回调污染后续场景。状态迁移与函数流程共享执行报告接口。
- **测量：** `/usr/bin/time -v` 采集 wall/user/sys CPU 与最大 RSS，输出机器可读 TSV/JSON；固定 fixture、seed、流程上限后比较迭代结果。

## Iterations

### I0 — 可构建基线与验证骨架

建立目录布局（`src/`, `include/`, `tests/`, `fixtures/`, `tools/`, `docs/`, `evidence/`）、canonical IR 与值类型/所有权/limits 契约、版本化 schema、稳定 CLI/API/退出码、公共错误/日志/随机 seed/报告接口、Make 目标（`test`, `sanitize`, `valgrind`, `measure`）和最小 CLI `--help/--version`。加入一个始终通过的 smoke fixture 与测试脚本；schema/CLI/API 在本轮冻结。

**本轮门槛：** `make test`、sanitizer smoke、`make measure` 均成功；结果写入 `evidence/iter-0/`。未满足则停止后续迭代。

### I1 — 状态图迁移组合测试

实现状态/迁移数据模型、DSL/C API 解析与静态校验、迁移序列生成（从初始状态出发，覆盖所有 reachable 声明边，报告 unreachable 边；支持 seed 和最大步数）、回调执行与状态/期望断言。定义迁移失败原子性（失败不得提交状态/上下文副作用）及循环语义（重复状态按上限/策略终止且可解释）；覆盖正常、非法状态、缺失目标、迁移失败、循环上限等错误路径；提供可复现 fixture 和失败报告。

**本轮门槛：** 状态图单元/集成测试全绿；ASan/UBSan 与 Valgrind（若安装）无错误；测量报告存在；证据写入 `evidence/iter-1/`。

### I2 — 函数关系与调用顺序组合测试

扩展关系模型支持节点 ID、参数来源（常量/前序结果/上下文）、前置依赖和顺序约束；将关系规范化为 DAG，覆盖每条声明 edge 并报告不可达/未采样 edge；定义 tie-break（稳定 ID 字典序）和 seed 驱动采样策略，trace 元数据记录 schema/IR hash、seed、limits、选择原因和 edge IDs。实现拓扑排序/受限流程生成、环检测、参数类型/数量校验、每步结果断言和失败定位。生成策略仅在用户声明关系的有限图上运行，并受流程数/深度上限控制。

**本轮门槛：** 参数传递、顺序、分支、环/未定义引用、回调失败 fixture 全绿；sanitizer/Valgrind 无错误；CPU/RSS 报告与 I1 可比较；证据写入 `evidence/iter-2/`。

### I3 — 发布硬化、文档和全量回归

I0 已冻结的输入格式和 CLI/API 只做兼容性回归，不在 I3 扩展 schema；补齐边界/拒绝性测试、DSL fuzz、确定性回放（trace schema/hash/退出码）和结构化日志；编写 `README.md`（安装、模型示例、命令、故障排查、限制）；添加 CI/本地脚本，执行全量测试、覆盖率（可选目标 80%）和发布测量。

**本轮门槛：** 干净构建可复现；全量功能与内存安全检查通过；README 示例可运行；发布证据写入 `evidence/iter-3/`。只有该门槛通过才标记项目完成。

## Acceptance Criteria

1. 给定状态图 fixture，工具能执行至少一条包含每条声明迁移的流程，并在故意错误回调时报告稳定的场景/步骤信息。
2. 给定函数关系 fixture，工具能按依赖顺序传递前序结果并拒绝环、未定义节点、参数不匹配；生成流程受 seed/上限约束且可回放。
3. Linux 标准工具链可构建；运行时无必需第三方模块。
4. 每轮有命令、退出码/关键摘要、版本与环境信息、测试/测量/内存工具输出的持久化证据；后轮不会在前轮失败时开始。
5. 不包含任意参数空间遗漏探测或未声明关系的穷举承诺。

## RALPLAN-DR Summary

### Principles

1. 先建立可运行且可回归的最小基线，再增加模型能力。
2. 用户声明的关系是生成边界；所有生成均确定性、可限额、可回放。
3. 每轮验证是硬门槛，功能、资源和内存安全证据同等重要。
4. 失败输出必须包含足够上下文以定位模型、步骤和原因。

### Decision Drivers

1. Linux 可移植性与第三方依赖最小化。
2. 能可靠执行越界/泄漏检查并保留证据。
3. 输入错误和组合流程失败的可诊断性、可复现性。

### Viable Options

| 选项 | 优点 | 代价/风险 |
| --- | --- | --- |
| **A. C11 + Make + 自有小型 DSL/严格解析器（采用）** | 无运行时依赖；ASan/UBSan/Valgrind 直接覆盖；Linux 性能和内存可量化 | 手写解析与内存管理工作量较高，需严格边界测试 |
| B. Python 3 标准库 CLI | 开发快，解析/数据结构简单，测试编写快 | 依赖 Python 版本；本机内存越界难以覆盖；性能/分发基线不一致 |
| C. C++17 + header-only JSON 库 | 类型/容器表达力强，解析生态成熟 | 引入第三方依赖与 ABI/构建复杂度，不符合最少依赖优先级 |

**选择依据：** 选 A，因为内存越界/泄漏是强制验收项且目标为 Linux 最小依赖；通过 I0 先锁定解析和报告接口，降低 C 的后续风险。B 可作为未来原型/辅助脚本，不进入本交付；C 被依赖约束否决。

## Risks and Mitigations

- DSL 复杂度失控：I0 只冻结满足两个 fixture 的版本化子集；未知语法明确报错，后续扩展需新迭代。
- 组合数量爆炸：流程数、深度、超时和 seed 必须是 CLI 参数，默认上限；报告实际生成数量。
- sanitizer/Valgrind 在环境不可用：sanitizer 作为必需 CI 目标；Valgrind 缺失时记录 `SKIP` 及原因并保留 sanitizer 证据，不宣称完成 Valgrind 检查。
- 测量噪声：固定容器/fixture/seed，记录内核、编译器和命令，使用重复运行并报告中位数/范围。
- 并发或回调副作用导致不可复现：首版单进程、串行执行；显式生命周期和每场景隔离。
- 子进程超时或信号处理错误：统一 `fork`/`waitpid` harness，超时 kill/wait 再收集证据；将异常退出视为失败而不是挂起。

## Handoff / Stop Rules

执行代理按 I0→I3 顺序交付；每轮提交代码、测试、文档和 `evidence/iter-N/`，并在下一轮开始前运行本轮门槛命令。任何必需测试、sanitizer 或模型校验失败都暂停迭代并记录 bounded blocker；不得用增加未声明功能来绕过失败。完成条件是 I3 门槛全部有新鲜证据。
