# 三个 wise_combine_test 实现对比与评价

## 1. 对比对象

| 简称 | 目录 | 实现语言/标准 | 定位 |
| --- | --- | --- | --- |
| plan_goal | `wise_combine_test.plan_goal` | C++17，仅标准库 + POSIX | 当前工作区实现，自研文本 DSL |
| lazycodex | `wise_combine_test.lazycodex` | C++20，仅标准库 + POSIX，CMake | JSON DSL + 外部 adapter 进程 |
| omx | `wise_combine_test.omx` | C11，最小 POSIX 依赖，Makefile | 文本 DSL + 公开 C API + 薄 CLI |

三者都面向同一个需求：描述对象状态图和函数关系后，生成并执行组合调用流程，检测状态迁移和函数组合调用中的问题。实现路线差异较大，下面从建模能力、生成算法、执行与隔离、报告与日志、测试与质量、工程化等角度逐一对比。

## 2. 总体印象

三者的成熟度和能力梯度大致是：

- **lazycodex 最接近“可交付的产品”**：模块划分清晰，校验严格，执行隔离和协议契约完整，报告与退出码稳定，测试矩阵最丰富。
- **plan_goal 是“功能面较全的原型”**：DSL 覆盖面广（含 mutex/constraint），双执行模式、日志轮转、覆盖率/内存/测量报告都有，但参数传递、预期结果校验、进程隔离等关键点还停留在浅层或缺失。
- **omx 是“最小可用库 + 自认未完成的骨架”**：以 C API 为中心，结构和内存所有权约定清楚，但执行引擎只是“每个可达迁移跑一次”，不是真正的组合枚举，且文档明确列出一批未实现项。

## 3. 建模与 DSL

| 维度 | plan_goal | lazycodex | omx |
| --- | --- | --- | --- |
| 描述格式 | 自研文本 DSL | JSON（version 1） | 空白分隔文本 DSL |
| 状态图 | `object { state; transition }` | `states / transitions / initial_state` | `state_graph / state / transition` |
| 函数关系 | `function / parameter / order / mutex / constraint` | `functions / relations(kind=argument,before)` | `relation_graph / call / relation` |
| 校验内容 | 状态、迁移、函数引用，初始状态唯一，参数两端存在性，order 无环，mutex/constraint 引用 | 严格 JSON schema + 类型匹配、重复绑定、自环、未知引用、order 无环 | 状态/迁移重复与引用、call 引用、关系端点、自环、重复边、关系无环 |
| 类型系统 | 参数有 name/type，但类型只存不校验兼容性 | 完整标量类型（null/bool/int/number/string），参数与返回类型校验 | 仅字符串形式参数，`$name` 引用，无类型系统 |
| 参数/数据流 | 可声明 `parameter`，仅做存在性校验，执行时不真正传递 | `argument` 关系在运行时按 producer 返回值绑定 consumer 参数 | `$call` 形式引用前序 call 结果，运行时替换 |
| 高级关系 | 支持 `mutex`、`constraint count()` | 不支持 mutex/constraint，只有 argument/before | 无 mutex/constraint |
| 解析健壮性 | 行级解析，错误带文件/行号 | 自实现 RFC8259 JSON 解析 + 规范化 + 指针定位诊断 | 行级 `strtok` 解析，固定 2048 行长限制 |

评价：

- **lazycodex 的建模最严谨**。它把 spec、model、generate、runtime 分层，类型匹配和重复绑定在解析阶段就能拦下，adapter 协议也依赖这套类型系统。
- **plan_goal 的 DSL 表达面最广**，尤其是 mutex 和 constraint，这正对应提案里的“高级要求”。但类型字段只是摆设，`parameter` 关系没有真正参与执行，属于“声明了但不落地”。
- **omx 的 DSL 最简**，可读性和上手成本低，但表达力最弱；`$name` 引用只能指向前序 call，且没有类型约束。

## 4. 组合生成

| 维度 | plan_goal | lazycodex | omx |
| --- | --- | --- | --- |
| 状态路径生成 | 有界 DFS，按 final 状态截断，`max_depth / max_state_visits / max_flows` | 有界 DFS，按可执行转换扩展，`max_cases / max_steps`，去重 | 不做路径枚举，只做可达性 BFS，每个可达迁移执行一次 |
| 函数序列生成 | 全拓扑序枚举（有约束），无约束函数全排列，受 `max_flows` 限制 | 把 argument/before 作为 prerequisite，在状态图上生成 step 序列 | 单条字典序拓扑序，`max_flows` 截断 |
| 去重 | 无显式序列去重 | `std::set<vector<string>>` 去重 | 不适用 |
| 有界状态语义 | 截断并设置 truncated | 区分 dead_end / case_limit / step_limit，退出码 3 | `uncovered` 计数 |
| 组合约束 | mutex 过滤、count 约束过滤 | 无 | 无 |
| seed | 无 seed | 有 seed 字段，但当前生成器 `static_cast<void>(seed)` 未真正使用 | 有 seed 字段，仅写入报告 |

评价：

- **lazycodex 的生成器最正规**：把参数依赖和顺序依赖统一为 prerequisite，配合状态迁移做有界 DFS，且对 `case_limit` / `step_limit` 有明确、可机器判读的状态码。
- **plan_goal 的生成器在“函数组合”上更强**：它能枚举所有拓扑序，无约束函数的全排列、mutex 和 count 约束都是另外两者没有的。但状态路径和函数序列是两套独立流程，合并方式较粗糙（直接拼接），且没有去重，容易产生重复或语义不清晰的结果。
- **omx 本质上不是组合枚举器**。`run_state` 只是按声明顺序把每个可达迁移各执行一次，`run_relation` 只是跑一条拓扑序，无法覆盖多条组合流程，与“组合测试”的核心目标差距最大。

## 5. 执行与隔离

| 维度 | plan_goal | lazycodex | omx |
| --- | --- | --- | --- |
| 执行模型 | `fork` 子进程内 `dlopen` 直接调用被测函数 | 每步 `fork + execve` 启动外部 adapter 进程 | 进程内通过 C 回调函数执行 |
| 参数传递 | 无（运行时忽略声明参数） | 有，按 relation 在步骤间传递返回值 | 有，`$name` 替换前序 call 结果 |
| 预期结果校验 | 仅非零返回值判失败，不校验输出/状态 | `expect.state` 或迁移目标状态 vs `observed_state` | `expect` 字符串 vs callback 返回的 `actual` |
| 崩溃/超时 | `alarm` 超时，信号判定 crash，父进程读取 pipe | step 2s / total 30s 超时，输出 16 MiB 上限，进程组 | 无超时/隔离（回调由调用方控制） |
| 安全边界 | `dlopen` 在子进程，隔离部分崩溃 | `execve` 无 shell、固定 env、允许名单 adapter | 无（C API 由宿主进程负责） |
| 协议 | 无跨进程协议 | 明确 JSON 协议，校验 protocol/status/observed_state/returns/stderr | C 函数签名契约 |

评价：

- **lazycodex 的执行隔离最可靠**。它把被测对象完全放在独立进程里，通过严格协议交互，能区分 passed / mismatch / protocol_error / timeout / crashed / launch_error，超时、输出上限、环境固定、允许名单都考虑到了。这是唯一真正把“内存错误不拖垮工具本身”落实到位的实现。
- **plan_goal 的隔离方向对但粒度粗**。每个 flow 一个子进程，能捕获崩溃和超时，但它直接 `dlopen` 并忽略参数与返回值，只把“返回 0”当通过；崩溃场景里父进程写 pipe 的方式也可能丢信息。独立被测程序模式解决了 ASan/Valgrind 检查，但生成代码同样只看返回值，不校验预期输出。
- **omx 完全依赖调用方回调**，便于库式集成，但没有任何进程隔离、超时或崩溃保护。它把“如何跑”的责任留给宿主，对 CLI 而言示例回调只是演示，不是可用的生产执行器。

## 6. 报告、日志与测量

| 维度 | plan_goal | lazycodex | omx |
| --- | --- | --- | --- |
| 报告格式 | text / json 汇总 | 每个 flow 一个 json+txt，另有 summary.json | 仅 `wct_report` 结构 + CLI 文本 |
| 失败信息 | status/exit_code/detail | status + 每步 observed_state/stderr/exit/detail | scenario/expected/actual/error/failed_step |
| 日志 | 分级日志、文件、大小上限与轮转 | 无独立日志文件（报告即输出） | 无日志轮转 |
| 测量 | 有 `doc/measurement-report.md`（time -v） | CLI 内嵌 wall/cpu/peak RSS 到 JSON | Makefile `measure` 输出 TSV |
| 退出码 | 1/2/3 粗粒度 | 0/2/3/4/5/6 稳定细分 | 0/1/2 粗粒度 |

评价：

- **lazycodex 的报告和退出码最可集成**：每条流程独立报告 + 汇总 JSON，且把 wall/CPU/RSS 直接放进机器可读输出，适合 CI。退出码把“生成耗尽、观测失败、运行时失败、用法错误”分开。
- **plan_goal 在日志上做了另外两者没有的轮转策略**，符合提案里“日志大小上限、轮转或清理”的要求，报告也够用。但 JSON 是手拼字符串，`detail` 未转义，遇到引号/换行会产出不合法 JSON。
- **omx 的报告字段最贴近“失败定位”**，`failed_step`、`scenario`、`expected`、`actual`、`error` 都有，但没有结构化文件输出，也没有测量到 CLI JSON 的闭环。

## 7. 测试与质量保障

| 维度 | plan_goal | lazycodex | omx |
| --- | --- | --- | --- |
| 单元测试 | 单一 `test_wise.cpp`，用 `assert` | CTest 多测试二进制：model/spec/generator/runtime/smoke | `test_api.c` 框架无关 + shell 冒烟 |
| 集成测试 | 示例 DSL + 示例动态库 | CLI 子命令 + fixture adapter 集成测试 | 无真正 adapter 集成 |
| 内存检查 | ASan/LSan，报告自述无告警 | ASan/UBSan/LSan 选项 + 可选 Valgrind | ASan/UBSan/Valgrind 目标 |
| 覆盖率 | 自述行覆盖率 80.63% | 未提供覆盖数字 | 未提供，release-readiness 明确列 gap |
| 边界/模糊 | 部分非法输入断言 | 运行时多类协议异常 + 输出上限 | release-readiness 明确“待补充” |

评价：

- **lazycodex 的测试工程化程度最高**。它把每个库拆成独立 target 和 test binary，runtime 覆盖了 pass/mismatch/malformed/extra/timeout/crash/cap/relation 多类场景，CI 可 `ctest` 一键验证。
- **plan_goal 覆盖面数字好看（80.63% 行覆盖），但测试是单个大文件 + assert**，缺少运行时协议和参数绑定的深度测试；内存报告是自述，无法从仓库直接一键复现完整证据链。
- **omx 测试是“合同式”的**，`test_api.c` 直接锁定 C API 行为，诚实记录了 I1-I3 未完成项，但整体功能测试深度和自动化程度都低于另外两者。

## 8. 工程化与文档

- **lazycodex**：CMake + CTest，源码目录按 spec/model/generate/runtime/report/cli 分层，依赖接口清晰；README 英中双语并声明同步策略。是三个中最接近“正式项目”的仓库。
- **plan_goal**：根 Makefile + `src`/`test`/`doc`/`ai` 结构，中文文档齐全（proposal/design/task + dsl + 报告），符合本仓库 AGENTS.md 的流程要求。但代码都压在 `wise.cpp` 一个文件里，模块边界不明显。
- **omx**：Makefile + `include/src/tools/tests/fixtures/docs`，有 API contract 和 release-readiness 文档，公开 C API 是最像“库”的设计。但仓库里夹杂大量 OMX 团队协作自动 checkpoint 提交，工程可读性一般。

## 9. 优势与劣势小结

### plan_goal

优势：

- DSL 覆盖面广，明确支持 mutex 和 count 约束；
- 有日志轮转、独立被测程序生成、文本/JSON 报告；
- 中文文档齐全，覆盖率、内存、测量报告齐备。

劣势：

- 参数传递只声明不执行，预期输出不校验，只以“返回 0”为通过；
- 生成结果合并粗糙、无去重；
- 代码单文件堆叠，JSON 报告手拼且转义不严谨；
- 运行时直接 `dlopen`，崩溃捕获和结果判定不够可靠。

### lazycodex

优势：

- 模块化最清晰，spec/model/generate/runtime/report/cli 各司其职；
- JSON 校验严格，类型系统和参数绑定真正参与执行；
- 外部 adapter 进程隔离、协议契约、超时/输出上限/允许名单完备；
- 稳定细分退出码和机器可读测量，CI 集成友好；
- 测试矩阵最丰富。

劣势：

- DSL 表达面较窄，没有 mutex/constraint 等高级关系；
- seed 字段目前是摆设，未真正参与生成；
- 用户必须额外编写 adapter 可执行程序，使用门槛更高。

### omx

优势：

- C API 边界清楚，适合作为库嵌入宿主程序；
- 依赖最少、构建简单，`-Werror` 等质量门槛明确；
- 文档诚实标注未完成项，API contract 写得很清楚。

劣势：

- 组合生成能力最弱，state/relation 都只跑一条路径/每个迁移一次；
- 无进程隔离、超时、崩溃保护；
- 无类型系统、无 JSON 报告、无日志轮转；
- 高级关系和测量自动化不足。

## 10. 结论与建议

如果目标是**“能真正发现组合调用问题的可交付工具”**，当前三者的排序建议是：

1. **lazycodex**：功能完整度和工程质量最高，最值得作为继续演进的基线；补上 mutex/constraint 或更丰富的 DSL 后会更完整。
2. **plan_goal**：方向最贴近原始提案（高级关系、日志、双模式、报告文档齐全），适合作为“需求表达更丰富”的参考实现；但需要把参数绑定、预期结果校验、JSON 转义和模块化补上，才能达到同等可靠性。
3. **omx**：适合作为最小 C 库或教学示例，距离“组合测试工具”的目标还有明显差距；若要保留，建议先补齐真正的路径/排列枚举与进程隔离。

综合来看，理想路径是**吸收 lazycodex 的工程架构与执行隔离、plan_goal 的 DSL 表达面和文档报告、omx 的 C API/最小依赖思路**，形成一个模块化、强校验、可安全执行、又支持高级关系的版本。
